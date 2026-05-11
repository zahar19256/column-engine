#pragma once
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include "../../FileBasicTools/DataStructures/Batch.h"
#include "Filter.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <variant>

enum class ExprType {
    Filter,
    Operator
};

enum class PredicateOpExprType : int8_t {
    And = 0,
    Or,
    Xor,
    Not
};

enum class BinaryExprType : int8_t {
    sum = 0,
    sub,
    mul,
    div,
};

enum class ScalarExprType: int8_t {
    Column = 0,
    Literal,
    Binary,
    Function
};

enum class FunctionExprType : int8_t {
    Length = 0,
    ExtractMinute,
    RegexpReplace,
    DateFormatHour
};

using ScalarValue = std::variant<int64_t , std::string , double>;


class Expr {
public:
    virtual ~Expr() = default;
    virtual void CollectColumns(std::unordered_set<std::string>& result) const = 0;
};

class PredicateExpr : public Expr {
public:
    virtual boost::dynamic_bitset<> Eval(const Batch& data) const = 0;
};

class ScalarExpr : public Expr {
public:
    
};

class ColumnExpr : public ScalarExpr {
public:
    ColumnType GetType() const {
        return type_;
    }
    const std::string& GetName() const {
        return column_name_;
    }
    void CollectColumns(std::unordered_set<std::string>& result) const override {
        result.insert(column_name_);
    }
private:
    std::string column_name_;
    ColumnType type_ = ColumnType::Unknown;
};

class LiteralExpr: public ScalarExpr {
public:
    const ScalarValue& GetValue() const {
        return value_;
    }
    ColumnType GetType() const {
        return type_;
    }
    void CollectColumns(std::unordered_set<std::string>&) const override {
    }
private:
    ScalarValue value_;
    ColumnType type_ = ColumnType::Unknown;
};

class BinaryExpr : public ScalarExpr {
public:
    void Eval();
    void CollectColumns(std::unordered_set<std::string>& result) const override {
        if (left_) {
            left_->CollectColumns(result);
        }
        if (right_) {
            right_->CollectColumns(result);
        }
    }
private:
    BinaryExprType op_;
    std::shared_ptr<ScalarExpr> left_;
    std::shared_ptr<ScalarExpr> right_;
    ColumnType type_;
};

class FilterExpr : public PredicateExpr {
public:
    Filters::OpType filter_type;
    std::string column_name;
    std::string value;
    FilterExpr(std::string name , std::string val , Filters::OpType type) {
        column_name = std::move(name);
        value = std::move(val);
        filter_type = type;
    }

    boost::dynamic_bitset<> Eval(const Batch& data) const override {
        std::shared_ptr<Column> current_column = data.GetColumn(column_name);
        return Filters::ColumnFilter(current_column, filter_type, value);
    }

    void CollectColumns(std::unordered_set<std::string>& result) const override {
        result.insert(column_name);
    }

    Filters::OpType GetFilterType() const {
        return filter_type;
    }
};

class PredicateOpExpr : public PredicateExpr {
public:
    PredicateOpExprType operator_type;
    std::unique_ptr<PredicateExpr> left_node;
    std::unique_ptr<PredicateExpr> right_node;

    PredicateOpExpr(PredicateOpExprType type , std::unique_ptr<PredicateExpr> left , std::unique_ptr<PredicateExpr> right = nullptr) {
        operator_type = type;
        left_node = std::move(left);
        right_node = std::move(right);
    }

    boost::dynamic_bitset<> Eval(const Batch& data) const override {
        switch(operator_type) {
            case PredicateOpExprType::And:
                return left_node->Eval(data) & right_node->Eval(data);
            case PredicateOpExprType::Or:
                return left_node->Eval(data) | right_node->Eval(data);
            case PredicateOpExprType::Xor:
                return left_node->Eval(data) ^ right_node->Eval(data);
            case PredicateOpExprType::Not:
                return ~left_node->Eval(data);
            default:
                throw std::runtime_error("Unknow predicate expression opeartor!");
        };
    }

    void CollectColumns(std::unordered_set<std::string>& result) const override {
        if (left_node) {
            left_node->CollectColumns(result);
        }
        if (right_node) {
            right_node->CollectColumns(result);
        }
    }

    PredicateOpExprType GetOperatorType() const {
        return operator_type;
    }
};
