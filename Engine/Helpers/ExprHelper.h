#pragma once
#include "../../FileBasicTools/src/BelZReader.h"
#include "../Functions/Aggregation.h"
#include "../Functions/Expression.h"
#include "../Functions/Filter.h"
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

//TODO сделать обвязки для построения выражений

struct FilterCommandElement {
    FilterCommandElement(std::string name , std::string value , Filters::OpType op_type)
        : name(std::move(name)) , value(std::move(value)) , type(op_type) {
    }
    std::string name , value;
    Filters::OpType type;
};

using PredicateCommandElement = std::variant<FilterCommandElement , PredicateOpExprType>;

inline std::unique_ptr<PredicateExpr> MakeFilter(FilterCommandElement command) {
    return std::make_unique<FilterExpr>(std::move(command.name) , std::move(command.value) , command.type);
}

inline std::shared_ptr<ScalarExpr> MakeColumnExpr(std::string column_name , ColumnType type = ColumnType::Unknown) {
    return std::make_shared<ColumnExpr>(std::move(column_name) , type);
}

inline std::shared_ptr<ScalarExpr> MakeLiteralExpr(Utility::LiteralValue value , ColumnType type = ColumnType::Unknown) {
    return std::make_shared<LiteralExpr>(std::move(value) , type);
}

inline std::shared_ptr<ScalarExpr> MakeUnaryExpr(std::shared_ptr<ScalarExpr> expression , UnaryExprType type) {
    return std::make_shared<UnaryExpr>(std::move(expression) , type);
}

inline std::shared_ptr<ScalarExpr> MakeBinaryExpr(
    std::shared_ptr<ScalarExpr> left ,
    std::shared_ptr<ScalarExpr> right ,
    BinaryExprType type ,
    ColumnType out_type = ColumnType::Unknown) {
    return std::make_shared<BinaryExpr>(std::move(left) , std::move(right) , type , out_type);
}

inline std::shared_ptr<ScalarExpr> MakeAddExpr(
    std::shared_ptr<ScalarExpr> left ,
    std::shared_ptr<ScalarExpr> right ,
    ColumnType out_type = ColumnType::Unknown) {
    return MakeBinaryExpr(std::move(left) , std::move(right) , BinaryExprType::Add , out_type);
}

inline std::shared_ptr<ScalarExpr> MakeSubExpr(
    std::shared_ptr<ScalarExpr> left ,
    std::shared_ptr<ScalarExpr> right ,
    ColumnType out_type = ColumnType::Unknown) {
    return MakeBinaryExpr(std::move(left) , std::move(right) , BinaryExprType::Sub , out_type);
}

inline std::shared_ptr<ScalarExpr> MakeCaseWhenExpr(
    std::vector<std::pair<std::shared_ptr<PredicateExpr> , std::shared_ptr<ScalarExpr>>> cases ,
    std::shared_ptr<ScalarExpr> else_expr ,
    ColumnType out_type = ColumnType::Unknown) {
    return std::make_shared<CaseWhenExpr>(std::move(cases) , std::move(else_expr) , out_type);
}

inline std::shared_ptr<ScalarExpr> MakeLengthExpr(std::shared_ptr<ScalarExpr> expression) {
    return MakeUnaryExpr(std::move(expression) , UnaryExprType::Length);
}

inline std::shared_ptr<ScalarExpr> MakeExtractMinuteExpr(std::shared_ptr<ScalarExpr> expression) {
    return MakeUnaryExpr(std::move(expression) , UnaryExprType::ExtractMinute);
}

inline std::shared_ptr<ScalarExpr> MakeDateTruncMinuteExpr(std::shared_ptr<ScalarExpr> expression) {
    return MakeUnaryExpr(std::move(expression) , UnaryExprType::DateTruncMinute);
}

inline std::shared_ptr<ScalarExpr> MakeRegexpReplaceExpr(
    std::shared_ptr<ScalarExpr> expression ,
    std::string pattern ,
    std::string replacement) {
    return std::make_shared<UnaryExpr>(
        std::move(expression) ,
        UnaryExprType::RegexpReplace ,
        std::move(pattern) ,
        std::move(replacement));
}

inline std::unique_ptr<PredicateExpr> MakePredicate(const std::vector<PredicateCommandElement>& tree , size_t& pos) {
    if (pos >= tree.size()) {
        throw std::runtime_error("Predicate tree size not matching!");
    }
    const auto elem = tree[pos++];
    if (std::holds_alternative<FilterCommandElement>(elem)) {
        return MakeFilter(std::get<FilterCommandElement>(elem));
    }
    PredicateOpExprType op = std::get<PredicateOpExprType>(elem);
    if (op == PredicateOpExprType::Not) {
        return std::make_unique<PredicateOpExpr>(op , MakePredicate(tree , pos) , nullptr);
    }
    return std::make_unique<PredicateOpExpr>(op , MakePredicate(tree , pos) , MakePredicate(tree , pos));
}
