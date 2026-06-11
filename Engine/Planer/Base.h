#pragma once
#include "../Functions/Aggregation.h"
#include "../Functions/Expression.h"
#include "../Functions/TopK.h"
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace LogicPlaner {

enum class QueryType { ScanNode = 0, FilterNode, AggregationNode, OrderByNode, LimitNode, OutputNode };
struct QueryNode {
    virtual ~QueryNode() = default;
    virtual const std::vector<std::string>& ColumnNames() const {
        static const std::vector<std::string> empty;
        return empty;
    }
    std::vector<std::unique_ptr<QueryNode>> next_nodes_;
};
struct ScanNode : public QueryNode {
    ScanNode(const std::string& table_name) : table_name_(table_name) {}
    template <typename T> void SetColumns(T&& column_names) {
        column_names_ = std::move(column_names);
    }
    template <typename T> void SetColumns(const T& column_names) {
        column_names_ = column_names;
    }
    const std::vector<std::string>& ColumnNames() const override {
        return column_names_;
    }
    std::string table_name_;
    std::vector<std::string> column_names_;
};
struct FilterNode : QueryNode {
    std::shared_ptr<PredicateExpr> predicates_;
};
struct AggregateNode : QueryNode {
    std::vector<std::shared_ptr<ScalarExpr>> group_by_;
    std::vector<Aggregation::AggregationCall> aggregates_;
};
struct ProjectionNode : QueryNode {
    std::vector<std::string> need_columns_;
    std::vector<std::string> new_names_;
};
struct OrderByNode : QueryNode {
    std::vector<std::shared_ptr<ScalarExpr>> order_by_;
    std::vector<SortDirection> directions_;
    size_t limit_ = 0;
    bool has_limit_ = false;
};
struct HavingNode : FilterNode {};
struct LimitNode : QueryNode {
    size_t limit_;
    size_t offset_ = 0;
};
struct OutputNode : QueryNode {
    size_t query_index_ = 0;
};
} // namespace LogicPlaner
