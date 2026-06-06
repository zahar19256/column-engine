#pragma once
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include "../../FileBasicTools/DataStructures/Batch.h"
#include "../../FileBasicTools/DataStructures/Utility.h"
#include "../Operators/ScalarOperators.h"
#include "Filter.h"
#include "Scheme.h"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <optional>

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

enum class ScalarExprType: int8_t {
    Column = 0,
    Literal,
    Binary,
    Unary,
};

struct EvalContext {
    Utility::StringArena* string_arena;
};

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
    virtual std::shared_ptr<Column> EvalBatch(const Batch& data , EvalContext& env) const = 0;
    virtual ColumnType GetType() const {
        return ColumnType::Unknown;
    }
};

class CaseWhenExpr : public ScalarExpr {
public:
    // attention я устал и заставил AI родить этот класс по описанию
    // TODO сделать его более урезанным так как для кликбенча ИИ-перестарался
    CaseWhenExpr(
        std::vector<std::pair<std::shared_ptr<PredicateExpr> , std::shared_ptr<ScalarExpr>>> cases ,
        std::shared_ptr<ScalarExpr> else_expr ,
        ColumnType out_type = ColumnType::Unknown)
        : cases_(std::move(cases)) ,
          else_(std::move(else_expr)) ,
          out_type_(out_type) {
    }

    ColumnType GetType() const override {
        if (out_type_ != ColumnType::Unknown) {
            return out_type_;
        }
        if (else_ && else_->GetType() != ColumnType::Unknown) {
            return else_->GetType();
        }
        for (const auto& [predicate , expression] : cases_) {
            if (expression && expression->GetType() != ColumnType::Unknown) {
                return expression->GetType();
            }
        }
        return ColumnType::Unknown;
    }

    void CollectColumns(std::unordered_set<std::string>& result) const override {
        for (const auto& [predicate , expression] : cases_) {
            if (predicate) {
                predicate->CollectColumns(result);
            }
            if (expression) {
                expression->CollectColumns(result);
            }
        }
        if (else_) {
            else_->CollectColumns(result);
        }
    }

    std::shared_ptr<Column> EvalBatch(const Batch& data , EvalContext& env) const override {
        if (!else_) {
            throw std::runtime_error("CaseWhenExpr requires ELSE expression!");
        }
        const size_t rows = data.GetRows();
        const ColumnType result_type = GetType();
        if (result_type == ColumnType::Unknown) {
            throw std::runtime_error("Unknown CaseWhenExpr output type!");
        }

        std::vector<boost::dynamic_bitset<>> masks;
        std::vector<std::shared_ptr<Column>> columns;
        masks.reserve(cases_.size());
        columns.reserve(cases_.size());
        for (const auto& [predicate , expression] : cases_) {
            if (!predicate || !expression) {
                throw std::runtime_error("CaseWhenExpr has empty case!");
            }
            auto mask = predicate->Eval(data);
            if (mask.size() != rows) {
                throw std::runtime_error("CaseWhenExpr condition mask size mismatch!");
            }
            auto column = expression->EvalBatch(data , env);
            CheckColumn(column , rows , result_type);
            masks.push_back(std::move(mask));
            columns.push_back(std::move(column));
        }

        auto else_column = else_->EvalBatch(data , env);
        CheckColumn(else_column , rows , result_type);

        auto result = MakeColumn(result_type , env.string_arena);
        result->Reserve(rows);
        for (size_t row = 0; row < rows; ++row) {
            bool was_inserted = false;
            for (size_t case_id = 0; case_id < cases_.size(); ++case_id) {
                if (masks[case_id][row]) {
                    PushValue(result , columns[case_id] , row , result_type);
                    was_inserted = true;
                    break;
                }
            }
            if (!was_inserted) {
                PushValue(result , else_column , row , result_type);
            }
        }
        return result;
    }
private:
    static void CheckColumn(const std::shared_ptr<Column>& column , size_t rows , ColumnType type) {
        if (!column) {
            throw std::runtime_error("CaseWhenExpr got empty column!");
        }
        if (column->Size() != rows) {
            throw std::runtime_error("CaseWhenExpr column size mismatch!");
        }
        if (column->GetType() != type) {
            throw std::runtime_error("CaseWhenExpr column type mismatch!");
        }
    }

    template <typename ColumnT>
    static void PushTypedValue(const std::shared_ptr<Column>& result , const std::shared_ptr<Column>& source , size_t row) {
        auto result_column = As<ColumnT>(result);
        auto source_column = As<ColumnT>(source);
        if (!result_column || !source_column) {
            throw std::runtime_error("CaseWhenExpr typed column mismatch!");
        }
        result_column->Push_Back(source_column->At(row));
    }

    static void PushStringValue(const std::shared_ptr<Column>& result , const std::shared_ptr<Column>& source , size_t row) {
        auto result_column = As<StringColumn>(result);
        auto source_column = As<StringColumn>(source);
        if (!result_column || !source_column) {
            throw std::runtime_error("CaseWhenExpr string column mismatch!");
        }
        auto value = source_column->At_view(row);
        result_column->AppendFromString(value.data(),
        value.size());
    }

    static void PushValue(const std::shared_ptr<Column>& result , const std::shared_ptr<Column>& source , size_t row , ColumnType type) {
        switch(type) {
            case ColumnType::Int8:
                PushTypedValue<Int8Column>(result , source , row);
                return;
            case ColumnType::Int16:
                PushTypedValue<Int16Column>(result , source , row);
                return;
            case ColumnType::Int32:
                PushTypedValue<Int32Column>(result , source , row);
                return;
            case ColumnType::Int64:
                PushTypedValue<Int64Column>(result , source , row);
                return;
            case ColumnType::Double:
                PushTypedValue<DoubleColumn>(result , source , row);
                return;
            case ColumnType::String:
                PushStringValue(result , source , row);
                return;
            case ColumnType::Date:
                PushTypedValue<DateColumn>(result , source , row);
                return;
            case ColumnType::Timestamp:
                PushTypedValue<TimeStampColumn>(result , source , row);
                return;
            case ColumnType::Int128:
                PushTypedValue<Int128Column>(result , source , row);
                return;
            default:
                throw std::runtime_error("Unknown type column in CaseWhen!");
        }
        throw std::runtime_error("Unknown CaseWhenExpr output type!");
    }

    std::vector <std::pair<std::shared_ptr<PredicateExpr> , std::shared_ptr<ScalarExpr>>> cases_;
    std::shared_ptr<ScalarExpr> else_;
    ColumnType out_type_ = ColumnType::Unknown;
};

class UnaryExpr : public ScalarExpr {
public:
    explicit UnaryExpr(std::shared_ptr<ScalarExpr> col , UnaryExprType type , std::optional<std::string> arg1 = std::nullopt , std::optional<std::string> arg2 = std::nullopt)
        : column_(std::move(col)) , type_(type) , arg1_(std::move(arg1)) , arg2_(std::move(arg2)) {
        if (type_ == UnaryExprType::RegexpReplace && (!arg1_ || !arg2_)) {
            throw std::runtime_error("RegexpReplace requires pattern and replacement!");
        }
    }
    std::shared_ptr<Column> EvalBatch(const Batch& data , EvalContext& env) const override {
        auto column = column_->EvalBatch(data , env);
        if (type_ == UnaryExprType::RegexpReplace) {
            return ApplyRegexpReplace(column , *arg1_ , *arg2_);
        }
        return ApplyUnaryOp(type_ , column);
    }
    void CollectColumns(std::unordered_set<std::string>& result) const override {
        if (column_) {
            column_->CollectColumns(result);
        }
    }
    ColumnType GetType() const override {
        switch(type_) {
            case UnaryExprType::ExtractMinute:
                return ColumnType::Int8;
            case UnaryExprType::Length:
                return ColumnType::Int64;
            case UnaryExprType::DateFormatHour:
            case UnaryExprType::DateTruncMinute:
                return ColumnType::Timestamp;
            case UnaryExprType::RegexpReplace:
                return ColumnType::String;
            default:
                throw std::runtime_error("Not supported unary type op!");
        }
    }
private:
    std::shared_ptr<ScalarExpr> column_; 
    UnaryExprType type_;
    std::optional<std::string> arg1_;
    std::optional<std::string> arg2_;
};

class ColumnExpr : public ScalarExpr {
public:
    explicit ColumnExpr(std::string column_name , ColumnType type = ColumnType::Unknown)
        : column_name_(std::move(column_name)) , type_(type) {
    }

    ColumnType GetType() const override {
        return type_;
    }
    const std::string& GetName() const {
        return column_name_;
    }
    void CollectColumns(std::unordered_set<std::string>& result) const override {
        result.insert(column_name_);
    }
    std::shared_ptr<Column> EvalBatch(const Batch& data , EvalContext& env) const override {
        if (column_index_ == std::nullopt) {
            column_index_ = data.GetScheme().GetIndex(column_name_);
        }
        return data.GetColumn(column_index_.value());
    }
private:
    std::string column_name_;
    mutable std::optional<size_t> column_index_;
    ColumnType type_ = ColumnType::Unknown;
};

class LiteralExpr: public ScalarExpr {
public:
    explicit LiteralExpr(Utility::LiteralValue value , ColumnType type = ColumnType::Unknown)
        : value_(std::move(value)) ,
          type_(type == ColumnType::Unknown ? InferType(value_) : type) {
    }

    const Utility::LiteralValue& GetValue() const {
        return value_;
    }
    ColumnType GetType() const override {
        return type_;
    }
    void CollectColumns(std::unordered_set<std::string>& storage) const override {
    }
    std::shared_ptr<Column> EvalBatch(const Batch& data , EvalContext& env) const override {
        return MakeLiteralColumn(data.GetRows() , env);
    }
private:
    static int64_t RequireInt64(const Utility::LiteralValue& value) {
        if (const auto* current = std::get_if<int64_t>(&value)) {
            return *current;
        }
        throw std::runtime_error("Literal value is not Int64-compatible!");
    }

    static double RequireDouble(const Utility::LiteralValue& value) {
        if (const auto* current = std::get_if<double>(&value)) {
            return *current;
        }
        if (const auto* current = std::get_if<int64_t>(&value)) {
            return static_cast<double>(*current);
        }
        throw std::runtime_error("Literal value is not Double-compatible!");
    }

    static __int128_t RequireInt128(const Utility::LiteralValue& value) {
        if (const auto* current = std::get_if<__int128_t>(&value)) {
            return *current;
        }
        if (const auto* current = std::get_if<int64_t>(&value)) {
            return static_cast<__int128_t>(*current);
        }
        throw std::runtime_error("Literal value is not Int128-compatible!");
    }

    static std::string_view RequireStringView(const Utility::LiteralValue& value) {
        if (const auto* current = std::get_if<std::string>(&value)) {
            return *current;
        }
        throw std::runtime_error("Literal value is not String-compatible!");
    }

    std::shared_ptr<Column> MakeLiteralColumn(size_t rows , EvalContext& env) const {
        auto column = MakeColumn(type_ , env.string_arena);
        switch(type_) {
            case ColumnType::Int8:
                As<Int8Column>(column)->Assign(rows , static_cast<int8_t>(RequireInt64(value_)));
                return column;
            case ColumnType::Int16:
                As<Int16Column>(column)->Assign(rows , static_cast<int16_t>(RequireInt64(value_)));
                return column;
            case ColumnType::Int32:
                As<Int32Column>(column)->Assign(rows , static_cast<int32_t>(RequireInt64(value_)));
                return column;
            case ColumnType::Int64:
                As<Int64Column>(column)->Assign(rows , RequireInt64(value_));
                return column;
            case ColumnType::Double:
                As<DoubleColumn>(column)->Assign(rows , RequireDouble(value_));
                return column;
            case ColumnType::String: {
                if (env.string_arena == nullptr) {
                    throw std::runtime_error("String literal requires string arena!");
                }
                std::string_view value = RequireStringView(value_);
                std::string_view stored = env.string_arena->Add(value.data() , value.size());
                As<StringColumn>(column)->Assign(rows , GermanStr(stored.data() , stored.size()));
                return column;
            }
            case ColumnType::Date:
                As<DateColumn>(column)->Assign(rows , RequireInt64(value_));
                return column;
            case ColumnType::Timestamp:
                As<TimeStampColumn>(column)->Assign(rows , RequireInt64(value_));
                return column;
            case ColumnType::Int128:
                As<Int128Column>(column)->Assign(rows , RequireInt128(value_));
                return column;
            default:
                throw std::runtime_error("Unknown column type in LiteralExpr!");
        }
        throw std::runtime_error("Unknown literal output column type!");
    }

    static ColumnType InferType(const Utility::LiteralValue& value) {
        return std::visit([](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                return ColumnType::Int64;
            } else if constexpr (std::is_same_v<T, std::string>) {
                return ColumnType::String;
            } else if constexpr (std::is_same_v<T, GermanStr>) {
                return ColumnType::String;
            } else if constexpr (std::is_same_v<T, double>) {
                return ColumnType::Double;
            } else if constexpr (std::is_same_v<T, __int128_t>) {
                return ColumnType::Int128;
            } else {
                return ColumnType::Unknown;
            }
        }, value);
    }

    Utility::LiteralValue value_;
    ColumnType type_ = ColumnType::Unknown;
};

class BinaryExpr : public ScalarExpr {
public:
    BinaryExpr(
        std::shared_ptr<ScalarExpr> left ,
        std::shared_ptr<ScalarExpr> right ,
        BinaryExprType op ,
        ColumnType out_type = ColumnType::Unknown)
        : op_(op) ,
          left_(std::move(left)) ,
          right_(std::move(right)) ,
          out_type_(out_type) {
    }

    ColumnType GetType() const override {
        return out_type_;
    }
    void CollectColumns(std::unordered_set<std::string>& result) const override {
        if (left_) {
            left_->CollectColumns(result);
        }
        if (right_) {
            right_->CollectColumns(result);
        }
    }
    std::shared_ptr<Column> EvalBatch(const Batch& data , EvalContext& env) const override {
        if (right_) {
            return ApplyBinaryOp(op_ , left_->EvalBatch(data , env) , right_->EvalBatch(data , env) , out_type_);
        }
        return left_->EvalBatch(data , env);
    }
private:
    BinaryExprType op_;
    std::shared_ptr<ScalarExpr> left_;
    std::shared_ptr<ScalarExpr> right_;
    ColumnType out_type_;
};

class FilterExpr : public PredicateExpr {
public:
    Filters::OpType filter_type;
    std::string column_name;
    mutable std::optional<size_t> column_index_;
    std::string value;
    FilterExpr(std::string name , std::string val , Filters::OpType type) {
        column_name = std::move(name);
        value = std::move(val);
        filter_type = type;
    }

    boost::dynamic_bitset<> Eval(const Batch& data) const override {
        if (column_index_ == std::nullopt) {
            column_index_.emplace(data.GetScheme().GetIndex(column_name));
        }
        std::shared_ptr<Column> current_column = data.GetColumn(column_index_.value());
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
