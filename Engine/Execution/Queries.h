#pragma once
#include "../../FileBasicTools/src/CSVWriter.h"
#include "Executor.h"
#include "../Planer/Base.h"
#include "../Helpers/ExprHelper.h"
#include "Functions/Expression.h"
#include "Functions/Filter.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClickBench {

    using namespace LogicPlaner;

    inline void CollectColumns(const QueryNode& query, std::unordered_set<std::string>& column_names) {
        if (auto* filter = dynamic_cast<const FilterNode*>(&query)) {
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
                if (call.column_name.has_value()) {
                    column_names.insert(*call.column_name);
                }
            }
        }
        for (const auto& child : query.next_nodes_) {
            CollectColumns(*child, column_names);
        }
    }

    inline constexpr const char* kQuery1ResultColumn = "q1";
    inline constexpr const char* kQuery2ResultColumn = "q2";
    inline std::unique_ptr<QueryNode> Make0QueryPlan(const std::string& table_name) {
        auto scan = std::make_unique<ScanNode>(table_name);

        auto aggregate = std::make_unique<AggregateNode>();
        Aggregation::AggregationCall count_call;
        count_call.type = Aggregation::AggregationType::Count;
        count_call.new_name = kQuery1ResultColumn;
        count_call.input_type = ColumnType::Unknown;
        count_call.output_type = ColumnType::Int64;
        aggregate->aggregates_.push_back(std::move(count_call));
        std::unordered_set<std::string> column_need;
        CollectColumns(*aggregate, column_need);
        scan->SetColumns(std::vector<std::string>(column_need.begin(), column_need.end()));
        aggregate->next_nodes_.push_back(std::move(scan));
        return aggregate;
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
        Aggregation::AggregationCall count_call;
        count_call.type = Aggregation::AggregationType::Count;
        count_call.new_name = kQuery2ResultColumn;
        count_call.input_type = ColumnType::Unknown;
        count_call.output_type = ColumnType::Int64;

        GlobalAggregationExecutor aggregate({std::move(count_call)});
        std::shared_ptr<PredicateExpr> filter_expr(MakeFilter({"AdvEngineID" , "0" , Filters::OpType::NotEqual}));
        auto filter_executor = std::make_shared<FilterExecutor>(std::move(filter_expr));
        filter_executor->child = std::make_shared<ScanExecutor>(table_name, std::vector<std::string>{"AdvEngineID"});
        aggregate.child = std::move(filter_executor);

        Batch output;
        if (!aggregate.Next(output)) {
            throw std::runtime_error("ClickBench query 2 did not produce result!");
        }

        auto result = As<Int64Column>(output.GetColumn(kQuery2ResultColumn));
        if (!result || result->Size() != 1) {
            throw std::runtime_error("ClickBench query 2 produced invalid result!");
        }

        return result->At(0);
    }

}
