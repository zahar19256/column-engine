// Attention
// тут я через llm строил запросы, чтобы они были как тесты

#pragma once
#include "../../FileBasicTools/src/CSVWriter.h"
#include "Executor.h"
#include "../Planer/Base.h"
#include "../Helpers/ExprHelper.h"
#include "../Helpers/AggrBuilder.h"
#include "Functions/Expression.h"
#include "Functions/Filter.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClickBench {

    using namespace LogicPlaner;

    inline bool ContainsAggregate(const QueryNode& query) {
        if (dynamic_cast<const AggregateNode*>(&query)) {
            return true;
        }
        for (const auto& child : query.next_nodes_) {
            if (ContainsAggregate(*child)) {
                return true;
            }
        }
        return false;
    }

    inline void CollectColumns(const QueryNode& query, std::unordered_set<std::string>& column_names) { // TODO в будущем сделать static cast + GetType
        if (dynamic_cast<const HavingNode*>(&query)) {
        } else if (auto* filter = dynamic_cast<const FilterNode*>(&query)) {
            if (filter->predicates_) {
                filter->predicates_->CollectColumns(column_names);
            }
        }
        if (auto* aggregate = dynamic_cast<const AggregateNode*>(&query)) {
            for (const auto& group_expr : aggregate->group_by_) {
                if (group_expr) {
                    group_expr->CollectColumns(column_names);
                }
            }
            for (const auto& call : aggregate->aggregates_) {
                if (call.expression) {
                    call.expression->CollectColumns(column_names);
                }
            }
        }
        if (auto* projection = dynamic_cast<const ProjectionNode*>(&query)) {
            bool collect_projection = true;
            for (const auto& child : projection->next_nodes_) {
                if (ContainsAggregate(*child)) {
                    collect_projection = false;
                }
            }
            if (collect_projection) {
                for (const auto& column_name : projection->need_columns_) {
                    column_names.insert(column_name);
                }
            }
        }
        if (auto* order_by = dynamic_cast<const OrderByNode*>(&query)) {
            bool collect_order_by = true;
            for (const auto& child : order_by->next_nodes_) {
                if (ContainsAggregate(*child)) {
                    collect_order_by = false;
                }
            }
            if (collect_order_by) {
                for (const auto& expr : order_by->order_by_) {
                    if (expr) {
                        expr->CollectColumns(column_names);
                    }
                }
            }
        }
        for (const auto& child : query.next_nodes_) {
            CollectColumns(*child, column_names);
        }
    }

    inline std::vector<std::string> RequiredColumns(const QueryNode& query) {
        std::unordered_set<std::string> column_names;
        CollectColumns(query, column_names);

        std::vector<std::string> result(column_names.begin(), column_names.end());
        return result;
    }

    inline void SetScanColumns(QueryNode& query, const std::vector<std::string>& column_names) {
        if (auto* scan = dynamic_cast<ScanNode*>(&query)) {
            scan->SetColumns(column_names);
            return;
        }
        for (const auto& child : query.next_nodes_) {
            SetScanColumns(*child, column_names);
        }
    }

    inline void PushRequiredColumnsToScans(QueryNode& query) {
        SetScanColumns(query, RequiredColumns(query));
    }

    inline std::unique_ptr<QueryNode> MakeProjectionPlan(
        std::unique_ptr<QueryNode> child ,
        std::vector<std::string> need_columns ,
        std::vector<std::string> new_names = {}) {
        auto projection = std::make_unique<ProjectionNode>();
        projection->need_columns_ = std::move(need_columns);
        projection->new_names_ = std::move(new_names);
        projection->next_nodes_.push_back(std::move(child));
        return projection;
    }

    inline std::unique_ptr<QueryNode> MakeFilterPlan(
        std::unique_ptr<QueryNode> child ,
        std::unique_ptr<PredicateExpr> predicate) {
        auto filter = std::make_unique<FilterNode>();
        filter->predicates_ = std::shared_ptr<PredicateExpr>(std::move(predicate));
        filter->next_nodes_.push_back(std::move(child));
        return filter;
    }

    inline std::unique_ptr<QueryNode> MakeLimitPlan(
        std::unique_ptr<QueryNode> child ,
        size_t limit ,
        size_t offset = 0) {
        auto limit_node = std::make_unique<LimitNode>();
        limit_node->limit_ = limit;
        limit_node->offset_ = offset;
        limit_node->next_nodes_.push_back(std::move(child));
        return limit_node;
    }

    inline std::unique_ptr<QueryNode> MakeOrderByLimitPlan(
        std::unique_ptr<QueryNode> child ,
        std::vector<std::shared_ptr<ScalarExpr>> order_by ,
        std::vector<SortDirection> directions ,
        size_t limit) {
        auto order = std::make_unique<OrderByNode>();
        order->order_by_ = std::move(order_by);
        order->directions_ = std::move(directions);
        order->limit_ = limit;
        order->has_limit_ = true;
        order->next_nodes_.push_back(std::move(child));
        return order;
    }

    inline std::unique_ptr<QueryNode> MakeGroupByQueryPlan(
        const std::string& table_name,
        std::vector<std::shared_ptr<ScalarExpr>> group_by,
        std::vector<Aggregation::AggregationCall> aggregates) {
        auto scan = std::make_unique<ScanNode>(table_name);

        auto aggregate = std::make_unique<AggregateNode>();
        aggregate->group_by_ = std::move(group_by);
        aggregate->aggregates_ = std::move(aggregates);
        aggregate->next_nodes_.push_back(std::move(scan));

        PushRequiredColumnsToScans(*aggregate);
        return aggregate;
    }

    inline std::unique_ptr<QueryNode> MakeGroupByOrderByLimitQueryPlan(
        const std::string& table_name ,
        std::vector<std::shared_ptr<ScalarExpr>> group_by ,
        std::vector<Aggregation::AggregationCall> aggregates ,
        std::vector<std::shared_ptr<ScalarExpr>> order_by ,
        std::vector<SortDirection> directions ,
        size_t limit) {
        auto aggregate = MakeGroupByQueryPlan(table_name , std::move(group_by) , std::move(aggregates));
        return MakeOrderByLimitPlan(std::move(aggregate) , std::move(order_by) , std::move(directions) , limit);
    }

    inline std::unique_ptr<QueryNode> MakeFilteredGroupByOrderByLimitQueryPlan(
        const std::string& table_name ,
        std::unique_ptr<PredicateExpr> predicate ,
        std::vector<std::shared_ptr<ScalarExpr>> group_by ,
        std::vector<Aggregation::AggregationCall> aggregates ,
        std::vector<std::shared_ptr<ScalarExpr>> order_by ,
        std::vector<SortDirection> directions ,
        size_t limit) {
        auto scan = std::make_unique<ScanNode>(table_name);
        auto filter = MakeFilterPlan(std::move(scan) , std::move(predicate));
        auto aggregate = std::make_unique<AggregateNode>();
        aggregate->group_by_ = std::move(group_by);
        aggregate->aggregates_ = std::move(aggregates);
        aggregate->next_nodes_.push_back(std::move(filter));
        auto order = MakeOrderByLimitPlan(std::move(aggregate) , std::move(order_by) , std::move(directions) , limit);
        PushRequiredColumnsToScans(*order);
        return order;
    }

    inline std::unique_ptr<QueryNode> MakeFilteredGroupByHavingOrderByLimitQueryPlan(
        const std::string& table_name ,
        std::unique_ptr<PredicateExpr> predicate ,
        std::vector<std::shared_ptr<ScalarExpr>> group_by ,
        std::vector<Aggregation::AggregationCall> aggregates ,
        std::unique_ptr<PredicateExpr> having ,
        std::vector<std::shared_ptr<ScalarExpr>> order_by ,
        std::vector<SortDirection> directions ,
        size_t limit) {
        auto scan = std::make_unique<ScanNode>(table_name);
        auto filter = MakeFilterPlan(std::move(scan) , std::move(predicate));
        auto aggregate = std::make_unique<AggregateNode>();
        aggregate->group_by_ = std::move(group_by);
        aggregate->aggregates_ = std::move(aggregates);
        aggregate->next_nodes_.push_back(std::move(filter));
        auto having_node = std::make_unique<HavingNode>();
        having_node->predicates_ = std::shared_ptr<PredicateExpr>(std::move(having));
        having_node->next_nodes_.push_back(std::move(aggregate));
        auto order = MakeOrderByLimitPlan(std::move(having_node) , std::move(order_by) , std::move(directions) , limit);
        PushRequiredColumnsToScans(*order);
        return order;
    }

    inline std::unique_ptr<QueryNode> MakeProjectionOrderByLimitQueryPlan(
        const std::string& table_name ,
        std::vector<std::string> need_columns ,
        std::vector<std::shared_ptr<ScalarExpr>> order_by ,
        std::vector<SortDirection> directions ,
        size_t limit) {
        auto scan = std::make_unique<ScanNode>(table_name);
        auto order = MakeOrderByLimitPlan(std::move(scan) , std::move(order_by) , std::move(directions) , limit);
        auto projection = MakeProjectionPlan(std::move(order) , std::move(need_columns));
        PushRequiredColumnsToScans(*projection);
        return projection;
    }

    inline std::unique_ptr<QueryNode> MakeFilteredProjectionOrderByLimitQueryPlan(
        const std::string& table_name ,
        std::unique_ptr<PredicateExpr> predicate ,
        std::vector<std::string> need_columns ,
        std::vector<std::shared_ptr<ScalarExpr>> order_by ,
        std::vector<SortDirection> directions ,
        size_t limit) {
        auto scan = std::make_unique<ScanNode>(table_name);
        auto filter = MakeFilterPlan(std::move(scan) , std::move(predicate));
        auto order = MakeOrderByLimitPlan(std::move(filter) , std::move(order_by) , std::move(directions) , limit);
        auto projection = MakeProjectionPlan(std::move(order) , std::move(need_columns));
        PushRequiredColumnsToScans(*projection);
        return projection;
    }

    inline std::shared_ptr<Executor> BuildExecutor(const QueryNode& query) {
        if (const auto* scan = dynamic_cast<const ScanNode*>(&query)) {
            return std::make_shared<ScanExecutor>(scan->table_name_, scan->ColumnNames());
        }
        if (const auto* filter = dynamic_cast<const FilterNode*>(&query)) {
            if (filter->next_nodes_.size() != 1) {
                throw std::runtime_error("Filter node must have exactly one child!");
            }
            auto executor = std::make_shared<FilterExecutor>(filter->predicates_);
            executor->child = BuildExecutor(*filter->next_nodes_.front());
            return executor;
        }
        if (const auto* aggregate = dynamic_cast<const AggregateNode*>(&query)) {
            if (aggregate->next_nodes_.size() != 1) {
                throw std::runtime_error("Aggregate node must have exactly one child!");
            }
            std::shared_ptr<Executor> executor;
            if (aggregate->group_by_.empty()) {
                executor = std::make_shared<GlobalAggregationExecutor>(aggregate->aggregates_);
            } else {
                executor = std::make_shared<GroupByAggregationExecutor>(aggregate->group_by_ , aggregate->aggregates_);
            }
            executor->child = BuildExecutor(*aggregate->next_nodes_.front());
            return executor;
        }
        if (const auto* projection = dynamic_cast<const ProjectionNode*>(&query)) {
            if (projection->next_nodes_.size() != 1) {
                throw std::runtime_error("Projection node must have exactly one child!");
            }
            std::vector<std::pair<std::string , std::string>> alias;
            const size_t count = std::min(projection->need_columns_.size() , projection->new_names_.size());
            alias.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                if (!projection->new_names_[i].empty()) {
                    alias.emplace_back(projection->need_columns_[i] , projection->new_names_[i]);
                }
            }
            auto executor = std::make_shared<ProjectionExecutor>(projection->need_columns_ , std::move(alias));
            executor->child = BuildExecutor(*projection->next_nodes_.front());
            return executor;
        }
        if (const auto* order_by = dynamic_cast<const OrderByNode*>(&query)) {
            if (order_by->next_nodes_.size() != 1) {
                throw std::runtime_error("OrderBy node must have exactly one child!");
            }
            if (!order_by->has_limit_) {
                throw std::runtime_error("OrderByExecutor requires LIMIT for TopK!");
            }
            auto executor = std::make_shared<OrderByExecutor>(
                order_by->order_by_ ,
                order_by->limit_ ,
                order_by->directions_);
            executor->child = BuildExecutor(*order_by->next_nodes_.front());
            return executor;
        }
        if (const auto* limit = dynamic_cast<const LimitNode*>(&query)) {
            if (limit->next_nodes_.size() != 1) {
                throw std::runtime_error("Limit node must have exactly one child!");
            }
            auto executor = std::make_shared<LimitExecutor>(limit->limit_ , limit->offset_);
            executor->child = BuildExecutor(*limit->next_nodes_.front());
            return executor;
        }
        throw std::runtime_error("Unsupported query node!");
    }

    inline constexpr const char* kQuery1ResultColumn = "q1";
    inline constexpr const char* kQuery2ResultColumn = "q2";
    inline constexpr const char* kQuery3SumAdvEngineIdColumn = "q3_sum_adv_engine_id";
    inline constexpr const char* kQuery3CountColumn = "q3_count";
    inline constexpr const char* kQuery3AvgResolutionWidthColumn = "q3_avg_resolution_width";
    inline constexpr const char* kQuery4ResultColumn = "q4";
    inline constexpr const char* kQuery5ResultColumn = "q5";
    inline constexpr const char* kQuery6ResultColumn = "q6";
    inline constexpr const char* kQuery7MinEventDateColumn = "q7_min_event_date";
    inline constexpr const char* kQuery7MaxEventDateColumn = "q7_max_event_date";
    inline constexpr size_t kQuery30Columns = 90;

    struct Query3Result {
        int64_t sum_adv_engine_id = 0;
        int64_t count = 0;
        int64_t avg_resolution_width = 0;
    };

    struct Query7Result {
        int64_t min_event_date = 0;
        int64_t max_event_date = 0;
    };

    inline std::string Query30OutputColumnName(size_t offset) {
        return "q30_sum_resolution_width_plus_" + std::to_string(offset);
    }

    inline Batch MakeThirtiethQueryResult(__int128_t sum_resolution_width , int64_t rows) {
        Scheme scheme;
        std::vector<std::shared_ptr<Column>> columns;
        columns.reserve(kQuery30Columns);

        for (size_t offset = 0; offset < kQuery30Columns; ++offset) {
            scheme.Push_Back(SchemeNode{Query30OutputColumnName(offset), ColumnType::Int64});

            auto column = std::make_shared<Int64Column>();
            const __int128_t value = sum_resolution_width + static_cast<__int128_t>(offset) * rows;
            column->Push_Back(static_cast<int64_t>(value));
            columns.push_back(std::move(column));
        }

        Batch result;
        result.SetScheme(scheme);
        for (auto& column : columns) {
            result.AddColumn(std::move(column));
        }
        result.InitMsk();
        return result;
    }

    inline std::unique_ptr<QueryNode> MakeSecondQueryPlan(const std::string& table_name) {
        auto scan = std::make_unique<ScanNode>(table_name);

        auto filter = std::make_unique<FilterNode>();
        filter->predicates_ = std::shared_ptr<PredicateExpr>(
            MakeFilter({"AdvEngineID" , "0" , Filters::OpType::NotEqual}));
        filter->next_nodes_.push_back(std::move(scan));

        auto aggregate = std::make_unique<AggregateNode>();
        Aggregation::AggregationCall count_call;
        count_call.type = Aggregation::AggregationType::Count;
        count_call.new_name = kQuery2ResultColumn;
        count_call.input_type = ColumnType::Int16;
        count_call.output_type = ColumnType::Int64;
        aggregate->aggregates_.push_back(std::move(count_call));
        aggregate->next_nodes_.push_back(std::move(filter));

        PushRequiredColumnsToScans(*aggregate);
        return aggregate;
    }

    inline std::unique_ptr<QueryNode> MakeThirdQueryPlan(const std::string& table_name) {
        auto scan = std::make_unique<ScanNode>(table_name);

        auto aggregate = std::make_unique<AggregateNode>();
        aggregate->aggregates_.push_back(MakeAggrCall(
            Aggregation::AggregationType::Sum,
            std::string("AdvEngineID"),
            kQuery3SumAdvEngineIdColumn,
            ColumnType::Int16,
            ColumnType::Int64));
        aggregate->aggregates_.push_back(MakeAggrCall(
            Aggregation::AggregationType::Count,
            std::nullopt,
            kQuery3CountColumn,
            ColumnType::Unknown,
            ColumnType::Int64));
        aggregate->aggregates_.push_back(MakeAggrCall(
            Aggregation::AggregationType::Avg,
            std::string("ResolutionWidth"),
            kQuery3AvgResolutionWidthColumn,
            ColumnType::Int16,
            ColumnType::Int16));
        aggregate->next_nodes_.push_back(std::move(scan));

        PushRequiredColumnsToScans(*aggregate);
        return aggregate;
    }

    inline std::unique_ptr<QueryNode> MakeFourthQueryPlan(const std::string& table_name) {
        auto scan = std::make_unique<ScanNode>(table_name);

        auto aggregate = std::make_unique<AggregateNode>();
        aggregate->aggregates_.push_back(MakeAggrCall(
            Aggregation::AggregationType::Avg,
            std::string("UserID"),
            kQuery4ResultColumn,
            ColumnType::Int64,
            ColumnType::Int64));
        aggregate->next_nodes_.push_back(std::move(scan));

        PushRequiredColumnsToScans(*aggregate);
        return aggregate;
    }

    inline std::unique_ptr<QueryNode> MakeFifthQueryPlan(const std::string& table_name) {
        auto scan = std::make_unique<ScanNode>(table_name);

        auto aggregate = std::make_unique<AggregateNode>();
        aggregate->aggregates_.push_back(MakeAggrCall(
            Aggregation::AggregationType::Distinct,
            std::string("UserID"),
            kQuery5ResultColumn,
            ColumnType::Int64,
            ColumnType::Int64));
        aggregate->next_nodes_.push_back(std::move(scan));

        PushRequiredColumnsToScans(*aggregate);
        return aggregate;
    }

    inline std::unique_ptr<QueryNode> MakeSixthQueryPlan(const std::string& table_name) {
        auto scan = std::make_unique<ScanNode>(table_name);

        auto aggregate = std::make_unique<AggregateNode>();
        aggregate->aggregates_.push_back(MakeAggrCall(
            Aggregation::AggregationType::Distinct,
            std::string("SearchPhrase"),
            kQuery6ResultColumn,
            ColumnType::String,
            ColumnType::Int64));
        aggregate->next_nodes_.push_back(std::move(scan));

        PushRequiredColumnsToScans(*aggregate);
        return aggregate;
    }

    inline std::unique_ptr<QueryNode> MakeSeventhQueryPlan(const std::string& table_name) {
        auto scan = std::make_unique<ScanNode>(table_name);

        auto aggregate = std::make_unique<AggregateNode>();
        aggregate->aggregates_.push_back(MakeAggrCall(
            Aggregation::AggregationType::Min,
            std::string("EventDate"),
            kQuery7MinEventDateColumn,
            ColumnType::Date,
            ColumnType::Date));
        aggregate->aggregates_.push_back(MakeAggrCall(
            Aggregation::AggregationType::Max,
            std::string("EventDate"),
            kQuery7MaxEventDateColumn,
            ColumnType::Date,
            ColumnType::Date));
        aggregate->next_nodes_.push_back(std::move(scan));

        PushRequiredColumnsToScans(*aggregate);
        return aggregate;
    }

    inline std::unique_ptr<PredicateExpr> MakeNotEmptyPredicate(const std::string& column_name) {
        return MakeFilter({column_name , "" , Filters::OpType::NotEqual});
    }

    inline std::unique_ptr<PredicateExpr> MakeAndPredicate(
        std::unique_ptr<PredicateExpr> left ,
        std::unique_ptr<PredicateExpr> right) {
        return std::make_unique<PredicateOpExpr>(PredicateOpExprType::And , std::move(left) , std::move(right));
    }

    inline std::unique_ptr<PredicateExpr> MakeOrPredicate(
        std::unique_ptr<PredicateExpr> left ,
        std::unique_ptr<PredicateExpr> right) {
        return std::make_unique<PredicateOpExpr>(PredicateOpExprType::Or , std::move(left) , std::move(right));
    }

    inline std::unique_ptr<PredicateExpr> MakeInPredicate(
        const std::string& column_name ,
        const std::vector<std::string>& values) {
        if (values.empty()) {
            throw std::runtime_error("IN predicate requires at least one value!");
        }
        auto result = MakeFilter({column_name , values.front() , Filters::OpType::Equal});
        for (size_t index = 1; index < values.size(); ++index) {
            result = MakeOrPredicate(std::move(result) , MakeFilter({column_name , values[index] , Filters::OpType::Equal}));
        }
        return result;
    }

    inline std::unique_ptr<PredicateExpr> MakeJulyCounterPredicate(
        bool dont_count_hits ,
        bool require_url ,
        bool require_title = false) {
        auto result = MakeFilter({"CounterID" , "62" , Filters::OpType::Equal});
        result = MakeAndPredicate(std::move(result) , MakeFilter({"EventDate" , "2013-07-01" , Filters::OpType::GreaterOrEqual}));
        result = MakeAndPredicate(std::move(result) , MakeFilter({"EventDate" , "2013-07-31" , Filters::OpType::LessOrEqual}));
        result = MakeAndPredicate(std::move(result) , MakeFilter({"IsRefresh" , "0" , Filters::OpType::Equal}));
        if (dont_count_hits) {
            result = MakeAndPredicate(std::move(result) , MakeFilter({"DontCountHits" , "0" , Filters::OpType::Equal}));
        }
        if (require_url) {
            result = MakeAndPredicate(std::move(result) , MakeNotEmptyPredicate("URL"));
        }
        if (require_title) {
            result = MakeAndPredicate(std::move(result) , MakeNotEmptyPredicate("Title"));
        }
        return result;
    }

    inline std::vector<std::string> AllHitsColumns() {
        return {
            "WatchID",
            "JavaEnable",
            "Title",
            "GoodEvent",
            "EventTime",
            "EventDate",
            "CounterID",
            "ClientIP",
            "RegionID",
            "UserID",
            "CounterClass",
            "OS",
            "UserAgent",
            "URL",
            "Referer",
            "IsRefresh",
            "RefererCategoryID",
            "RefererRegionID",
            "URLCategoryID",
            "URLRegionID",
            "ResolutionWidth",
            "ResolutionHeight",
            "ResolutionDepth",
            "FlashMajor",
            "FlashMinor",
            "FlashMinor2",
            "NetMajor",
            "NetMinor",
            "UserAgentMajor",
            "UserAgentMinor",
            "CookieEnable",
            "JavascriptEnable",
            "IsMobile",
            "MobilePhone",
            "MobilePhoneModel",
            "Params",
            "IPNetworkID",
            "TraficSourceID",
            "SearchEngineID",
            "SearchPhrase",
            "AdvEngineID",
            "IsArtifical",
            "WindowClientWidth",
            "WindowClientHeight",
            "ClientTimeZone",
            "ClientEventTime",
            "SilverlightVersion1",
            "SilverlightVersion2",
            "SilverlightVersion3",
            "SilverlightVersion4",
            "PageCharset",
            "CodeVersion",
            "IsLink",
            "IsDownload",
            "IsNotBounce",
            "FUniqID",
            "OriginalURL",
            "HID",
            "IsOldCounter",
            "IsEvent",
            "IsParameter",
            "DontCountHits",
            "WithHash",
            "HitColor",
            "LocalEventTime",
            "Age",
            "Sex",
            "Income",
            "Interests",
            "Robotness",
            "RemoteIP",
            "WindowName",
            "OpenerName",
            "HistoryLength",
            "BrowserLanguage",
            "BrowserCountry",
            "SocialNetwork",
            "SocialAction",
            "HTTPError",
            "SendTiming",
            "DNSTiming",
            "ConnectTiming",
            "ResponseStartTiming",
            "ResponseEndTiming",
            "FetchTiming",
            "SocialSourceNetworkID",
            "SocialSourcePage",
            "ParamPrice",
            "ParamOrderID",
            "ParamCurrency",
            "ParamCurrencyID",
            "OpenstatServiceName",
            "OpenstatCampaignID",
            "OpenstatAdID",
            "OpenstatSourceID",
            "UTMSource",
            "UTMMedium",
            "UTMCampaign",
            "UTMContent",
            "UTMTerm",
            "FromTag",
            "HasGCLID",
            "RefererHash",
            "URLHash",
            "CLID",
        };
    }

    inline std::vector<Aggregation::AggregationCall> CountAs(std::string alias = "c") {
        std::vector<Aggregation::AggregationCall> result;
        result.push_back(MakeAggrCall(
            Aggregation::AggregationType::Count,
            std::nullopt,
            std::move(alias),
            ColumnType::Unknown,
            ColumnType::Int64));
        return result;
    }

    inline std::unique_ptr<QueryNode> MakeEighthQueryPlan(const std::string& table_name) {
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            MakeFilter({"AdvEngineID" , "0" , Filters::OpType::NotEqual}),
            {MakeColumnExpr("AdvEngineID" , ColumnType::Int16)},
            CountAs("c"),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            std::numeric_limits<size_t>::max());
    }

    inline std::unique_ptr<QueryNode> MakeNinthQueryPlan(const std::string& table_name) {
        std::vector<Aggregation::AggregationCall> aggregates;
        aggregates.push_back(MakeAggrCall(
            Aggregation::AggregationType::Distinct,
            std::string("UserID"),
            "u",
            ColumnType::Int64,
            ColumnType::Int64));
        return MakeGroupByOrderByLimitQueryPlan(
            table_name,
            {MakeColumnExpr("RegionID" , ColumnType::Int64)},
            std::move(aggregates),
            {MakeColumnExpr("u" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeTenthQueryPlan(const std::string& table_name) {
        std::vector<Aggregation::AggregationCall> aggregates;
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Sum, std::string("AdvEngineID"), "sum_adv_engine_id", ColumnType::Int16, ColumnType::Int64));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Count, std::nullopt, "c", ColumnType::Unknown, ColumnType::Int64));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Avg, std::string("ResolutionWidth"), "avg_resolution_width", ColumnType::Int16, ColumnType::Int16));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Distinct, std::string("UserID"), "uniq_user_id", ColumnType::Int64, ColumnType::Int64));
        return MakeGroupByOrderByLimitQueryPlan(
            table_name,
            {MakeColumnExpr("RegionID" , ColumnType::Int64)},
            std::move(aggregates),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeEleventhQueryPlan(const std::string& table_name) {
        std::vector<Aggregation::AggregationCall> aggregates;
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Distinct, std::string("UserID"), "u", ColumnType::Int64, ColumnType::Int64));
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            MakeNotEmptyPredicate("MobilePhoneModel"),
            {MakeColumnExpr("MobilePhoneModel" , ColumnType::String)},
            std::move(aggregates),
            {MakeColumnExpr("u" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeTwelfthQueryPlan(const std::string& table_name) {
        std::vector<Aggregation::AggregationCall> aggregates;
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Distinct, std::string("UserID"), "u", ColumnType::Int64, ColumnType::Int64));
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            MakeNotEmptyPredicate("MobilePhoneModel"),
            {MakeColumnExpr("MobilePhone" , ColumnType::Int16), MakeColumnExpr("MobilePhoneModel" , ColumnType::String)},
            std::move(aggregates),
            {MakeColumnExpr("u" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeThirteenthQueryPlan(const std::string& table_name) {
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            MakeNotEmptyPredicate("SearchPhrase"),
            {MakeColumnExpr("SearchPhrase" , ColumnType::String)},
            CountAs("c"),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeFourteenthQueryPlan(const std::string& table_name) {
        std::vector<Aggregation::AggregationCall> aggregates;
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Distinct, std::string("UserID"), "u", ColumnType::Int64, ColumnType::Int64));
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            MakeNotEmptyPredicate("SearchPhrase"),
            {MakeColumnExpr("SearchPhrase" , ColumnType::String)},
            std::move(aggregates),
            {MakeColumnExpr("u" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeFifteenthQueryPlan(const std::string& table_name) {
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            MakeNotEmptyPredicate("SearchPhrase"),
            {MakeColumnExpr("SearchEngineID" , ColumnType::Int16), MakeColumnExpr("SearchPhrase" , ColumnType::String)},
            CountAs("c"),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeSixteenthQueryPlan(const std::string& table_name) {
        return MakeGroupByOrderByLimitQueryPlan(
            table_name,
            {MakeColumnExpr("UserID" , ColumnType::Int64)},
            CountAs("c"),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeSeventeenthQueryPlan(const std::string& table_name) {
        return MakeGroupByOrderByLimitQueryPlan(
            table_name,
            {MakeColumnExpr("UserID" , ColumnType::Int64), MakeColumnExpr("SearchPhrase" , ColumnType::String)},
            CountAs("c"),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeEighteenthQueryPlan(const std::string& table_name) {
        auto aggregate = MakeGroupByQueryPlan(
            table_name,
            {MakeColumnExpr("UserID" , ColumnType::Int64), MakeColumnExpr("SearchPhrase" , ColumnType::String)},
            CountAs("c"));
        return MakeLimitPlan(std::move(aggregate) , 10);
    }

    inline std::unique_ptr<QueryNode> MakeNineteenthQueryPlan(const std::string& table_name) {
        return MakeGroupByOrderByLimitQueryPlan(
            table_name,
            {
                MakeColumnExpr("UserID" , ColumnType::Int64),
                MakeExtractMinuteExpr(MakeColumnExpr("EventTime" , ColumnType::Timestamp)),
                MakeColumnExpr("SearchPhrase" , ColumnType::String),
            },
            CountAs("c"),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeTwentiethQueryPlan(const std::string& table_name) {
        auto scan = std::make_unique<ScanNode>(table_name);
        auto filter = MakeFilterPlan(std::move(scan) , MakeFilter({"UserID" , "435090932899640449" , Filters::OpType::Equal}));
        auto projection = MakeProjectionPlan(std::move(filter) , {"UserID"});
        PushRequiredColumnsToScans(*projection);
        return projection;
    }

    inline std::unique_ptr<QueryNode> MakeTwentyFirstQueryPlan(const std::string& table_name) {
        auto scan = std::make_unique<ScanNode>(table_name);
        auto filter = MakeFilterPlan(std::move(scan) , MakeFilter({"URL" , "%google%" , Filters::OpType::Like}));
        auto aggregate = std::make_unique<AggregateNode>();
        aggregate->aggregates_ = CountAs("q21");
        aggregate->next_nodes_.push_back(std::move(filter));
        PushRequiredColumnsToScans(*aggregate);
        return aggregate;
    }

    inline std::unique_ptr<QueryNode> MakeTwentySecondQueryPlan(const std::string& table_name) {
        auto predicate = MakeFilter({"URL" , "%google%" , Filters::OpType::Like});
        predicate = MakeAndPredicate(std::move(predicate) , MakeNotEmptyPredicate("SearchPhrase"));

        std::vector<Aggregation::AggregationCall> aggregates;
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Min, std::string("URL"), "min_url", ColumnType::String, ColumnType::String));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Count, std::nullopt, "c", ColumnType::Unknown, ColumnType::Int64));
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            std::move(predicate),
            {MakeColumnExpr("SearchPhrase" , ColumnType::String)},
            std::move(aggregates),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeTwentyThirdQueryPlan(const std::string& table_name) {
        auto predicate = MakeFilter({"Title" , "%Google%" , Filters::OpType::Like});
        predicate = MakeAndPredicate(std::move(predicate) , MakeFilter({"URL" , "%.google.%" , Filters::OpType::NotLike}));
        predicate = MakeAndPredicate(std::move(predicate) , MakeNotEmptyPredicate("SearchPhrase"));

        std::vector<Aggregation::AggregationCall> aggregates;
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Min, std::string("URL"), "min_url", ColumnType::String, ColumnType::String));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Min, std::string("Title"), "min_title", ColumnType::String, ColumnType::String));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Count, std::nullopt, "c", ColumnType::Unknown, ColumnType::Int64));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Distinct, std::string("UserID"), "u", ColumnType::Int64, ColumnType::Int64));
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            std::move(predicate),
            {MakeColumnExpr("SearchPhrase" , ColumnType::String)},
            std::move(aggregates),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeTwentyFourthQueryPlan(const std::string& table_name) {
        return MakeFilteredProjectionOrderByLimitQueryPlan(
            table_name,
            MakeFilter({"URL" , "%google%" , Filters::OpType::Like}),
            AllHitsColumns(),
            {MakeColumnExpr("EventTime" , ColumnType::Timestamp)},
            {SortDirection::Asc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeTwentyFifthQueryPlan(const std::string& table_name) {
        return MakeFilteredProjectionOrderByLimitQueryPlan(
            table_name,
            MakeNotEmptyPredicate("SearchPhrase"),
            {"SearchPhrase"},
            {MakeColumnExpr("EventTime" , ColumnType::Timestamp)},
            {SortDirection::Asc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeTwentySixthQueryPlan(const std::string& table_name) {
        return MakeFilteredProjectionOrderByLimitQueryPlan(
            table_name,
            MakeNotEmptyPredicate("SearchPhrase"),
            {"SearchPhrase"},
            {MakeColumnExpr("SearchPhrase" , ColumnType::String)},
            {SortDirection::Asc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeTwentySeventhQueryPlan(const std::string& table_name) {
        return MakeFilteredProjectionOrderByLimitQueryPlan(
            table_name,
            MakeNotEmptyPredicate("SearchPhrase"),
            {"SearchPhrase"},
            {MakeColumnExpr("EventTime" , ColumnType::Timestamp), MakeColumnExpr("SearchPhrase" , ColumnType::String)},
            {SortDirection::Asc, SortDirection::Asc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeTwentyEighthQueryPlan(const std::string& table_name) {
        std::vector<Aggregation::AggregationCall> aggregates;
        aggregates.push_back(MakeAggrCall(
            Aggregation::AggregationType::Avg,
            MakeLengthExpr(MakeColumnExpr("URL" , ColumnType::String)),
            "l",
            ColumnType::Int64,
            ColumnType::Int64));
        aggregates.push_back(MakeAggrCall(
            Aggregation::AggregationType::Count,
            std::nullopt,
            "c",
            ColumnType::Unknown,
            ColumnType::Int64));
        return MakeFilteredGroupByHavingOrderByLimitQueryPlan(
            table_name,
            MakeNotEmptyPredicate("URL"),
            {MakeColumnExpr("CounterID" , ColumnType::Int64)},
            std::move(aggregates),
            MakeFilter({"c" , "100000" , Filters::OpType::Greater}),
            {MakeColumnExpr("l" , ColumnType::Int64)},
            {SortDirection::Desc},
            25);
    }

    inline std::unique_ptr<QueryNode> MakeThirtyFirstQueryPlan(const std::string& table_name) {
        std::vector<Aggregation::AggregationCall> aggregates;
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Count, std::nullopt, "c", ColumnType::Unknown, ColumnType::Int64));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Sum, std::string("IsRefresh"), "sum_is_refresh", ColumnType::Int16, ColumnType::Int64));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Avg, std::string("ResolutionWidth"), "avg_resolution_width", ColumnType::Int16, ColumnType::Int16));
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            MakeNotEmptyPredicate("SearchPhrase"),
            {MakeColumnExpr("SearchEngineID" , ColumnType::Int16), MakeColumnExpr("ClientIP" , ColumnType::Int64)},
            std::move(aggregates),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeThirtySecondQueryPlan(const std::string& table_name) {
        std::vector<Aggregation::AggregationCall> aggregates;
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Count, std::nullopt, "c", ColumnType::Unknown, ColumnType::Int64));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Sum, std::string("IsRefresh"), "sum_is_refresh", ColumnType::Int16, ColumnType::Int64));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Avg, std::string("ResolutionWidth"), "avg_resolution_width", ColumnType::Int16, ColumnType::Int16));
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            MakeNotEmptyPredicate("SearchPhrase"),
            {MakeColumnExpr("WatchID" , ColumnType::Int64), MakeColumnExpr("ClientIP" , ColumnType::Int64)},
            std::move(aggregates),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeThirtyThirdQueryPlan(const std::string& table_name) {
        std::vector<Aggregation::AggregationCall> aggregates;
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Count, std::nullopt, "c", ColumnType::Unknown, ColumnType::Int64));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Sum, std::string("IsRefresh"), "sum_is_refresh", ColumnType::Int16, ColumnType::Int64));
        aggregates.push_back(MakeAggrCall(Aggregation::AggregationType::Avg, std::string("ResolutionWidth"), "avg_resolution_width", ColumnType::Int16, ColumnType::Int16));
        return MakeGroupByOrderByLimitQueryPlan(
            table_name,
            {MakeColumnExpr("WatchID" , ColumnType::Int64), MakeColumnExpr("ClientIP" , ColumnType::Int64)},
            std::move(aggregates),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeThirtyFourthQueryPlan(const std::string& table_name) {
        return MakeGroupByOrderByLimitQueryPlan(
            table_name,
            {MakeColumnExpr("URL" , ColumnType::String)},
            CountAs("c"),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeThirtyFifthQueryPlan(const std::string& table_name) {
        return MakeGroupByOrderByLimitQueryPlan(
            table_name,
            {MakeLiteralExpr(int64_t{1}, ColumnType::Int64), MakeColumnExpr("URL" , ColumnType::String)},
            CountAs("c"),
            {MakeColumnExpr("c" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeThirtySeventhQueryPlan(const std::string& table_name) {
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            MakeJulyCounterPredicate(true, true),
            {MakeColumnExpr("URL" , ColumnType::String)},
            CountAs("PageViews"),
            {MakeColumnExpr("PageViews" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeThirtyEighthQueryPlan(const std::string& table_name) {
        return MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            MakeJulyCounterPredicate(true, false, true),
            {MakeColumnExpr("Title" , ColumnType::String)},
            CountAs("PageViews"),
            {MakeColumnExpr("PageViews" , ColumnType::Int64)},
            {SortDirection::Desc},
            10);
    }

    inline std::unique_ptr<QueryNode> MakeThirtyNinthQueryPlan(const std::string& table_name) {
        auto predicate = MakeJulyCounterPredicate(false, false);
        predicate = MakeAndPredicate(std::move(predicate) , MakeFilter({"IsLink" , "0" , Filters::OpType::NotEqual}));
        predicate = MakeAndPredicate(std::move(predicate) , MakeFilter({"IsDownload" , "0" , Filters::OpType::Equal}));
        auto order = MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            std::move(predicate),
            {MakeColumnExpr("URL" , ColumnType::String)},
            CountAs("PageViews"),
            {MakeColumnExpr("PageViews" , ColumnType::Int64)},
            {SortDirection::Desc},
            1010);
        return MakeLimitPlan(std::move(order) , 10 , 1000);
    }

    inline std::unique_ptr<QueryNode> MakeFortyFirstQueryPlan(const std::string& table_name) {
        auto predicate = MakeJulyCounterPredicate(false, false);
        predicate = MakeAndPredicate(std::move(predicate) , MakeInPredicate("TraficSourceID" , {"-1" , "6"}));
        predicate = MakeAndPredicate(std::move(predicate) , MakeFilter({"RefererHash" , "3594120000172545465" , Filters::OpType::Equal}));
        auto order = MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            std::move(predicate),
            {MakeColumnExpr("URLHash" , ColumnType::Int64), MakeColumnExpr("EventDate" , ColumnType::Date)},
            CountAs("PageViews"),
            {MakeColumnExpr("PageViews" , ColumnType::Int64)},
            {SortDirection::Desc},
            110);
        return MakeLimitPlan(std::move(order) , 10 , 100);
    }

    inline std::unique_ptr<QueryNode> MakeFortySecondQueryPlan(const std::string& table_name) {
        auto predicate = MakeJulyCounterPredicate(true, false);
        predicate = MakeAndPredicate(std::move(predicate) , MakeFilter({"URLHash" , "2868770270353813622" , Filters::OpType::Equal}));
        auto order = MakeFilteredGroupByOrderByLimitQueryPlan(
            table_name,
            std::move(predicate),
            {MakeColumnExpr("WindowClientWidth" , ColumnType::Int16), MakeColumnExpr("WindowClientHeight" , ColumnType::Int16)},
            CountAs("PageViews"),
            {MakeColumnExpr("PageViews" , ColumnType::Int64)},
            {SortDirection::Desc},
            10010);
        return MakeLimitPlan(std::move(order) , 10 , 10000);
    }

    inline int64_t RunFirstQueryCount(const std::string& table_name) {
        
        Aggregation::AggregationCall count_call;
        count_call.type = Aggregation::AggregationType::Count;
        count_call.new_name = kQuery1ResultColumn;
        count_call.input_type = ColumnType::Unknown;
        count_call.output_type = ColumnType::Int64;

        GlobalAggregationExecutor aggregate({std::move(count_call)});
        aggregate.child = std::make_shared<ScanExecutor>(table_name, std::vector<std::string>{});

        Batch output;
        if (!aggregate.Next(output)) {
            throw std::runtime_error("ClickBench query 1 did not produce result!");
        }

        auto result = As<Int64Column>(output.GetColumn(kQuery1ResultColumn));
        if (!result || result->Size() != 1) {
            throw std::runtime_error("ClickBench query 1 produced invalid result!");
        }

        return result->At(0);
    }

    inline int64_t RunSecondQueryCount(const std::string& table_name) {
        const auto query = MakeSecondQueryPlan(table_name);
        const auto executor = BuildExecutor(*query);

        Batch output;
        if (!executor->Next(output)) {
            throw std::runtime_error("ClickBench query 2 did not produce result!");
        }

        auto result = As<Int64Column>(output.GetColumn(kQuery2ResultColumn));
        if (!result || result->Size() != 1) {
            throw std::runtime_error("ClickBench query 2 produced invalid result!");
        }

        return result->At(0);
    }

    inline Query3Result RunThirdQuery(const std::string& table_name) {
        const auto query = MakeThirdQueryPlan(table_name);
        const auto executor = BuildExecutor(*query);

        Batch output;
        if (!executor->Next(output)) {
            throw std::runtime_error("ClickBench query 3 did not produce result!");
        }

        auto sum_adv_engine_id = As<Int64Column>(output.GetColumn(kQuery3SumAdvEngineIdColumn));
        auto count = As<Int64Column>(output.GetColumn(kQuery3CountColumn));
        auto avg_resolution_width = As<Int16Column>(output.GetColumn(kQuery3AvgResolutionWidthColumn));
        if (!sum_adv_engine_id || !count || !avg_resolution_width ||
            sum_adv_engine_id->Size() != 1 || count->Size() != 1 || avg_resolution_width->Size() != 1) {
            throw std::runtime_error("ClickBench query 3 produced invalid result!");
        }

        return Query3Result{
            .sum_adv_engine_id = sum_adv_engine_id->At(0),
            .count = count->At(0),
            .avg_resolution_width = avg_resolution_width->At(0),
        };
    }

    inline int64_t RunFourthQueryAvgUserID(const std::string& table_name) {
        const auto query = MakeFourthQueryPlan(table_name);
        const auto executor = BuildExecutor(*query);

        Batch output;
        if (!executor->Next(output)) {
            throw std::runtime_error("ClickBench query 4 did not produce result!");
        }

        auto result = As<Int64Column>(output.GetColumn(kQuery4ResultColumn));
        if (!result || result->Size() != 1) {
            throw std::runtime_error("ClickBench query 4 produced invalid result!");
        }

        return result->At(0);
    }

    inline int64_t RunFifthQueryDistinctUserID(const std::string& table_name) {
        const auto query = MakeFifthQueryPlan(table_name);
        const auto executor = BuildExecutor(*query);

        Batch output;
        if (!executor->Next(output)) {
            throw std::runtime_error("ClickBench query 5 did not produce result!");
        }

        auto result = As<Int64Column>(output.GetColumn(kQuery5ResultColumn));
        if (!result || result->Size() != 1) {
            throw std::runtime_error("ClickBench query 5 produced invalid result!");
        }

        return result->At(0);
    }

    inline int64_t RunSixthQueryDistinctSearchPhrase(const std::string& table_name) {
        const auto query = MakeSixthQueryPlan(table_name);
        const auto executor = BuildExecutor(*query);

        Batch output;
        if (!executor->Next(output)) {
            throw std::runtime_error("ClickBench query 6 did not produce result!");
        }

        auto result = As<Int64Column>(output.GetColumn(kQuery6ResultColumn));
        if (!result || result->Size() != 1) {
            throw std::runtime_error("ClickBench query 6 produced invalid result!");
        }

        return result->At(0);
    }

    inline Query7Result RunSeventhQueryEventDateMinMax(const std::string& table_name) {
        const auto query = MakeSeventhQueryPlan(table_name);
        const auto executor = BuildExecutor(*query);

        Batch output;
        if (!executor->Next(output)) {
            throw std::runtime_error("ClickBench query 7 did not produce result!");
        }

        auto min_event_date = As<DateColumn>(output.GetColumn(kQuery7MinEventDateColumn));
        auto max_event_date = As<DateColumn>(output.GetColumn(kQuery7MaxEventDateColumn));
        if (!min_event_date || !max_event_date ||
            min_event_date->Size() != 1 || max_event_date->Size() != 1) {
            throw std::runtime_error("ClickBench query 7 produced invalid result!");
        }

        return Query7Result{
            .min_event_date = min_event_date->At(0),
            .max_event_date = max_event_date->At(0),
        };
    }

    inline Batch RunThirtiethQuery(const std::string& table_name) {
        ScanExecutor scan(table_name, std::vector<std::string>{"ResolutionWidth"});

        __int128_t sum_resolution_width = 0;
        int64_t rows = 0;
        Batch input;
        while (scan.Next(input)) {
            auto resolution_width = As<Int16Column>(input.GetColumn("ResolutionWidth"));
            if (!resolution_width) {
                throw std::runtime_error("ClickBench query 30 expected Int16 ResolutionWidth column!");
            }

            const int16_t* data = resolution_width->Data();
            for (size_t row = 0; row < resolution_width->Size(); ++row) {
                sum_resolution_width += data[row];
            }
            rows += static_cast<int64_t>(resolution_width->Size());
        }

        return MakeThirtiethQueryResult(sum_resolution_width, rows);
    }

    inline Batch RunQuery(std::unique_ptr<QueryNode> query) {
        const auto executor = BuildExecutor(*query);
        Batch output;
        if (!executor->Next(output)) {
            throw std::runtime_error("ClickBench query did not produce result!");
        }
        return output;
    }

    inline Batch RunEighthQuery(const std::string& table_name) {
        return RunQuery(MakeEighthQueryPlan(table_name));
    }

    inline Batch RunNinthQuery(const std::string& table_name) {
        return RunQuery(MakeNinthQueryPlan(table_name));
    }

    inline Batch RunTenthQuery(const std::string& table_name) {
        return RunQuery(MakeTenthQueryPlan(table_name));
    }

    inline Batch RunEleventhQuery(const std::string& table_name) {
        return RunQuery(MakeEleventhQueryPlan(table_name));
    }

    inline Batch RunTwelfthQuery(const std::string& table_name) {
        return RunQuery(MakeTwelfthQueryPlan(table_name));
    }

    inline Batch RunThirteenthQuery(const std::string& table_name) {
        return RunQuery(MakeThirteenthQueryPlan(table_name));
    }

    inline Batch RunFourteenthQuery(const std::string& table_name) {
        return RunQuery(MakeFourteenthQueryPlan(table_name));
    }

    inline Batch RunFifteenthQuery(const std::string& table_name) {
        return RunQuery(MakeFifteenthQueryPlan(table_name));
    }

    inline Batch RunSixteenthQuery(const std::string& table_name) {
        return RunQuery(MakeSixteenthQueryPlan(table_name));
    }

    inline Batch RunSeventeenthQuery(const std::string& table_name) {
        return RunQuery(MakeSeventeenthQueryPlan(table_name));
    }

    inline Batch RunEighteenthQuery(const std::string& table_name) {
        return RunQuery(MakeEighteenthQueryPlan(table_name));
    }

    inline Batch RunNineteenthQuery(const std::string& table_name) {
        return RunQuery(MakeNineteenthQueryPlan(table_name));
    }

    inline Batch RunTwentiethQuery(const std::string& table_name) {
        return RunQuery(MakeTwentiethQueryPlan(table_name));
    }

    inline Batch RunTwentyFirstQuery(const std::string& table_name) {
        return RunQuery(MakeTwentyFirstQueryPlan(table_name));
    }

    inline Batch RunTwentySecondQuery(const std::string& table_name) {
        return RunQuery(MakeTwentySecondQueryPlan(table_name));
    }

    inline Batch RunTwentyThirdQuery(const std::string& table_name) {
        return RunQuery(MakeTwentyThirdQueryPlan(table_name));
    }

    inline Batch RunTwentyFourthQuery(const std::string& table_name) {
        return RunQuery(MakeTwentyFourthQueryPlan(table_name));
    }

    inline Batch RunTwentyFifthQuery(const std::string& table_name) {
        return RunQuery(MakeTwentyFifthQueryPlan(table_name));
    }

    inline Batch RunTwentySixthQuery(const std::string& table_name) {
        return RunQuery(MakeTwentySixthQueryPlan(table_name));
    }

    inline Batch RunTwentySeventhQuery(const std::string& table_name) {
        return RunQuery(MakeTwentySeventhQueryPlan(table_name));
    }

    inline Batch RunTwentyEighthQuery(const std::string& table_name) {
        return RunQuery(MakeTwentyEighthQueryPlan(table_name));
    }

    inline Batch RunThirtyFirstQuery(const std::string& table_name) {
        return RunQuery(MakeThirtyFirstQueryPlan(table_name));
    }

    inline Batch RunThirtySecondQuery(const std::string& table_name) {
        return RunQuery(MakeThirtySecondQueryPlan(table_name));
    }

    inline Batch RunThirtyThirdQuery(const std::string& table_name) {
        return RunQuery(MakeThirtyThirdQueryPlan(table_name));
    }

    inline Batch RunThirtyFourthQuery(const std::string& table_name) {
        return RunQuery(MakeThirtyFourthQueryPlan(table_name));
    }

    inline Batch RunThirtyFifthQuery(const std::string& table_name) {
        return RunQuery(MakeThirtyFifthQueryPlan(table_name));
    }

    inline Batch RunThirtySeventhQuery(const std::string& table_name) {
        return RunQuery(MakeThirtySeventhQueryPlan(table_name));
    }

    inline Batch RunThirtyEighthQuery(const std::string& table_name) {
        return RunQuery(MakeThirtyEighthQueryPlan(table_name));
    }

    inline Batch RunThirtyNinthQuery(const std::string& table_name) {
        return RunQuery(MakeThirtyNinthQueryPlan(table_name));
    }

    inline Batch RunFortyFirstQuery(const std::string& table_name) {
        return RunQuery(MakeFortyFirstQueryPlan(table_name));
    }

    inline Batch RunFortySecondQuery(const std::string& table_name) {
        return RunQuery(MakeFortySecondQueryPlan(table_name));
    }

} // namespace ClickBench
