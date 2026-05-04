#pragma once
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include "../../FileBasicTools/DataStructures/Batch.h"
#include "../Execution/Filter.h"

enum class ExprKind {
    Filter,
    Operator
};

enum class OperatorExprType : int8_t {
    And = 0,
    Or,
    Xor,
    Not
};


class Expr {
public:
    virtual ~Expr() = default;
};

class PredicateExpr : public Expr {
public:
    virtual boost::dynamic_bitset<> Eval(const Batch& data) const = 0;
};

class FilterExpr : public PredicateExpr {
public:
    Filters::OpType filter_type;
    std::string column_name;
    std::string value;

    boost::dynamic_bitset<> Eval(const Batch& data) const override {
        std::shared_ptr<Column> current_column = data.GetColumn(column_name);
        return Filters::ColumnFilter(current_column, filter_type, value);
    }
    Filters::OpType GetFilterType() const {
        return filter_type;
    }
};

class OperatorExpr : public PredicateExpr {
public:
    OperatorExprType operator_type;
    std::unique_ptr<PredicateExpr> left_node;
    std::unique_ptr<PredicateExpr> right_node;

    boost::dynamic_bitset<> Eval(const Batch& data) const override {
        switch(operator_type) {
            case OperatorExprType::And:
                return left_node->Eval(data) & right_node->Eval(data);
            case OperatorExprType::Or:
                return left_node->Eval(data) | right_node->Eval(data);
            case OperatorExprType::Xor:
                return left_node->Eval(data) ^ right_node->Eval(data);
            case OperatorExprType::Not:
                return ~left_node->Eval(data);
            default:
                throw std::runtime_error("Unknow predicate expression opeartor!");
        };
    }
    OperatorExprType GetOperatorType() const {
        return operator_type;
    }
};
