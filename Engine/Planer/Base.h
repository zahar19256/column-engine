#pragma once
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include "../Functions/Expression.h"

namespace LogicPlaner {
    struct QueryNode {
        virtual ~QueryNode() = default;
        std::vector<std::unique_ptr<QueryNode>> next_nodes_;
    };
    struct ScanNode : QueryNode {
        std::string table_name_;
        std::vector<std::string> column_names_;
    };
    struct FilterNode : QueryNode {
        std::shared_ptr<Expr> predicates_;
    };
    struct ProjectionNode : QueryNode {
        std::vector<std::shared_ptr<Expr>> projections_;
    };
    struct AggregateNode : QueryNode {
        std::vector<std::shared_ptr<Expr>> group_by_;
        // std::vector<AggregateCall> aggregates_; TODO придумать чё тут и как делать 
    };
    struct OrderByNode : QueryNode {
        // std::vector<OrderBy> order_by_; TODO придумать чё тут и как делать 
    };
    struct LimitNode : QueryNode {
        size_t limit_;
    };
}