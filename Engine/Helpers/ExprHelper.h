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

inline std::shared_ptr<ScalarExpr> MakeLiteralExpr(Utility::ScalarValue value , ColumnType type = ColumnType::Unknown) {
    return std::make_shared<LiteralExpr>(std::move(value) , type);
}

inline std::shared_ptr<ScalarExpr> MakeUnaryExpr(std::shared_ptr<ScalarExpr> expression , UnaryExprType type) {
    return std::make_shared<UnaryExpr>(std::move(expression) , type);
}

inline std::shared_ptr<ScalarExpr> MakeLengthExpr(std::shared_ptr<ScalarExpr> expression) {
    return MakeUnaryExpr(std::move(expression) , UnaryExprType::Length);
}

inline std::shared_ptr<ScalarExpr> MakeExtractMinuteExpr(std::shared_ptr<ScalarExpr> expression) {
    return MakeUnaryExpr(std::move(expression) , UnaryExprType::ExtractMinute);
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
