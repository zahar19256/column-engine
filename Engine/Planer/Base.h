#pragma once
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <cstddef>
#include "../Functions/Expression.h"
#include "../Functions/Aggregation.h"

namespace LogicPlaner {
    struct QueryNode {
        virtual ~QueryNode() = default;
        virtual const std::vector<std::string>& ColumnNames() const {
            static const std::vector<std::string> empty;
            return empty;
        }
        std::vector<std::unique_ptr<QueryNode>> next_nodes_;
    };
    struct ScanNode : public QueryNode {
        ScanNode(const std::string& table_name) : table_name_(table_name) {
        }
        template <typename T>
        void SetColumns(T&& column_names) {
            column_names_ = std::move(column_names);
        }
        template <typename T>
        void SetColumns(const T& column_names) {
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
    struct ProjectionNode : QueryNode {
        std::vector<std::shared_ptr<Expr>> projections_;
    };
    struct AggregateNode : QueryNode {
        std::vector<std::shared_ptr<ScalarExpr>> group_by_;
        std::vector<Aggregation::AggregationCall> aggregates_;
    };
    struct OrderByNode : QueryNode {
        // std::vector<OrderBy> order_by_; TODO придумать чё тут и как делать 
    };
    struct LimitNode : QueryNode {
        size_t limit_;
    };
    struct OutputNode : QueryNode {
        size_t query_index_ = 0;
    };
}
