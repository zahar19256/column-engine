#include "ScalarOperators.h"
#include "Column.h"
#include "Scheme.h"
#include <memory>
#include <re2/re2.h>
#include <stdexcept>
#include <string>

template <NumericColumn LeftColumnType, NumericColumn RightColumnType, typename ResultColumnType>
std::shared_ptr<Column> ColumnAdd(std::shared_ptr<LeftColumnType> left, std::shared_ptr<RightColumnType> right,
                                  ResultColumnType result) {
    result->Resize(left->Size());
    auto* data = result->Data();
    const auto* left_data = left->Data();
    const auto* right_data = right->Data();
    for (size_t i = 0; i < left->Size(); ++i) {
        data[i] = (left_data[i] + right_data[i]);
    }
    return result;
}

std::shared_ptr<Column>
DropToHours(std::shared_ptr<Column> current) { // TODO пока что данная функция заменяет DATE_FORMAT и по сути является
                                               // хардкодом для кликбенча
    if (current->GetType() != ColumnType::Timestamp) {
        throw std::runtime_error("Not timestamp column type in DropToHours!");
    }
    std::shared_ptr<TimeStampColumn> result = std::make_shared<TimeStampColumn>();
    result->Reserve(current->Size());
    const int64_t* data = As<TimeStampColumn>(current)->Data();
    for (size_t i = 0; i < current->Size(); ++i) {
        result->Push_Back(data[i] - data[i] % 3600);
    }
    return result;
}

std::shared_ptr<Column> DropToMinute(std::shared_ptr<Column> current) {
    if (current->GetType() != ColumnType::Timestamp) {
        throw std::runtime_error("Not timestamp column type in DropToMinute!");
    }
    std::shared_ptr<TimeStampColumn> result = std::make_shared<TimeStampColumn>();
    result->Reserve(current->Size());
    const int64_t* data = As<TimeStampColumn>(current)->Data();
    for (size_t i = 0; i < current->Size(); ++i) {
        result->Push_Back(data[i] - data[i] % 60);
    }
    return result;
}

std::shared_ptr<Column> ExtractMinute(std::shared_ptr<Column> current) {
    if (current->GetType() != ColumnType::Timestamp) {
        throw std::runtime_error("Not timestamp column Type in ExtractMinute!");
    }
    std::shared_ptr<Int8Column> result = std::make_shared<Int8Column>();
    result->Reserve(current->Size());
    const int64_t* data = As<TimeStampColumn>(current)->Data();
    for (size_t i = 0; i < current->Size(); ++i) {
        result->Push_Back(data[i] % 3600 / 60);
    }
    return result;
}

std::shared_ptr<Column> Length(std::shared_ptr<Column> current) {
    if (current->GetType() != ColumnType::String) {
        throw std::runtime_error("Not string column in Length scalar operator!");
    }
    std::shared_ptr<Int64Column> result = std::make_shared<Int64Column>();
    result->Reserve(current->Size());
    const auto source = As<StringColumn>(current);
    for (size_t i = 0; i < current->Size(); ++i) {
        result->Push_Back(static_cast<int64_t>(source->At(i).Size()));
    }
    return result;
}

std::shared_ptr<Column> ApplyRegexpReplace(std::shared_ptr<Column> current, const std::string& pattern,
                                           const std::string& replacement, Utility::StringArena* arena) {
    if (current->GetType() != ColumnType::String) {
        throw std::runtime_error("Not string column in RegReplace scalar operator!");
    }
    const RE2 regex(pattern);
    if (!regex.ok()) {
        throw std::runtime_error("Invalid RegReplace regex: " + regex.error());
    }
    std::shared_ptr<StringColumn> source = As<StringColumn>(current);
    std::shared_ptr<StringColumn> result = std::make_shared<StringColumn>(arena);
    result->Reserve(source->Size());
    for (size_t i = 0; i < source->Size(); ++i) {
        std::string value(source->At_view(i));
        RE2::GlobalReplace(&value, regex, replacement);
        result->AppendFromString(value.data(), value.size());
    }
    return result;
}

template <NumericColumn LeftColumnType, NumericColumn RightColumnType, typename ResultColumnType>
std::shared_ptr<Column> ColumnSub(std::shared_ptr<LeftColumnType> left, std::shared_ptr<RightColumnType> right,
                                  ResultColumnType result) {
    result->Resize(left->Size());
    auto* data = result->Data();
    const auto* left_data = left->Data();
    const auto* right_data = right->Data();
    for (size_t i = 0; i < left->Size(); ++i) {
        data[i] = (left_data[i] - right_data[i]);
    }
    return result;
}

template <typename LeftColumnType, typename RightColumnType, typename ResultColumnType>
std::shared_ptr<Column> ExecuteBinaryOpTyped(BinaryExprType op, std::shared_ptr<LeftColumnType> left,
                                             std::shared_ptr<RightColumnType> right,
                                             std::shared_ptr<ResultColumnType> result) {
    switch (op) {
    case BinaryExprType::Add:
        return ColumnAdd(left, right, result);
    case BinaryExprType::Sub:
        return ColumnSub(left, right, result);
    default:
        throw std::runtime_error("Unknow ExprbinaryType!");
    };
}

template <typename LeftColumnType, typename RightColumnType>
std::shared_ptr<Column> ExecuteBinaryOp(BinaryExprType op, const std::shared_ptr<LeftColumnType>& left,
                                        const std::shared_ptr<RightColumnType>& right, ColumnType result_type) {

    using LeftT = typename LeftColumnType::ValueType;
    using RightT = typename RightColumnType::ValueType;

    if (!left || !right) {
        throw std::runtime_error("Null column in ExecuteBinaryOp!");
    }

    if (left->Size() != right->Size()) {
        throw std::runtime_error("Column size mismatch in ExecuteBinaryOp!");
    }

    if constexpr (IsDateOrTimeStampColumnV<LeftColumnType> || IsDateOrTimeStampColumnV<RightColumnType>) {
        throw std::runtime_error("Date/TimeStamp binary arithmetic is not supported!");
    } else if constexpr (std::is_floating_point_v<LeftT> && std::is_floating_point_v<RightT>) {
        return ExecuteBinaryOpTyped(op, left, right, std::make_shared<DoubleColumn>());
    } else if constexpr (std::is_integral_v<LeftT> && std::is_integral_v<RightT>) {
        return ExecuteBinaryOpTyped(op, left, right, std::make_shared<Int64Column>());
    } else if constexpr (std::is_same_v<LeftT, std::string> || std::is_same_v<RightT, std::string>) {
        throw std::runtime_error("String binary arithmetic is not supported!");
    } else {
        throw std::runtime_error("Unsupported column types in ExecuteBinaryOp!");
    }
}

template <typename LeftColumnType>
std::shared_ptr<Column> DispatchRightBinaryOp(BinaryExprType op, std::shared_ptr<LeftColumnType> left,
                                              std::shared_ptr<Column> right, ColumnType result_type) {
    switch (right->GetType()) {
    case ColumnType::Int8:
        return ExecuteBinaryOp(op, left, As<Int8Column>(right), result_type);
    case ColumnType::Int16:
        return ExecuteBinaryOp(op, left, As<Int16Column>(right), result_type);
    case ColumnType::Int32:
        return ExecuteBinaryOp(op, left, As<Int32Column>(right), result_type);
    case ColumnType::Int64:
        return ExecuteBinaryOp(op, left, As<Int64Column>(right), result_type);
    case ColumnType::Int128:
        return ExecuteBinaryOp(op, left, As<Int128Column>(right), result_type);
    case ColumnType::Double:
        return ExecuteBinaryOp(op, left, As<DoubleColumn>(right), result_type);
    case ColumnType::Date:
        return ExecuteBinaryOp(op, left, As<DateColumn>(right), result_type);
    case ColumnType::Timestamp:
        return ExecuteBinaryOp(op, left, As<TimeStampColumn>(right), result_type);
    case ColumnType::String:
        return ExecuteBinaryOp(op, left, As<StringColumn>(right), result_type);
    default:
        throw std::runtime_error("Unknown column type!");
    }
}

std::shared_ptr<Column> DispatchLeftBinaryOp(BinaryExprType op, std::shared_ptr<Column> left,
                                             std::shared_ptr<Column> right, ColumnType result_type) {
    switch (left->GetType()) {
    case ColumnType::Int8:
        return DispatchRightBinaryOp(op, As<Int8Column>(left), right, result_type);
    case ColumnType::Int16:
        return DispatchRightBinaryOp(op, As<Int16Column>(left), right, result_type);
    case ColumnType::Int32:
        return DispatchRightBinaryOp(op, As<Int32Column>(left), right, result_type);
    case ColumnType::Int64:
        return DispatchRightBinaryOp(op, As<Int64Column>(left), right, result_type);
    case ColumnType::Int128:
        return DispatchRightBinaryOp(op, As<Int128Column>(left), right, result_type);
    case ColumnType::Double:
        return DispatchRightBinaryOp(op, As<DoubleColumn>(left), right, result_type);
    case ColumnType::Date:
        return DispatchRightBinaryOp(op, As<DateColumn>(left), right, result_type);
    case ColumnType::Timestamp:
        return DispatchRightBinaryOp(op, As<TimeStampColumn>(left), right, result_type);
    case ColumnType::String:
        return DispatchRightBinaryOp(op, As<StringColumn>(left), right, result_type);
    default:
        throw std::runtime_error("Unknown column type!");
    }
}

std::shared_ptr<Column> ApplyBinaryOp(BinaryExprType op, std::shared_ptr<Column> left, std::shared_ptr<Column> right,
                                      ColumnType result_type) {
    return DispatchLeftBinaryOp(op, left, right, result_type);
}

std::shared_ptr<Column> ApplyUnaryOp(UnaryExprType op, std::shared_ptr<Column> current) {
    switch (op) {
    case UnaryExprType::ExtractMinute:
        return ExtractMinute(current);
    case UnaryExprType::Length:
        return Length(current);
    case UnaryExprType::DateFormatHour:
        return DropToHours(current);
    case UnaryExprType::DateTruncMinute:
        return DropToMinute(current);
    case UnaryExprType::RegexpReplace:
        throw std::runtime_error("RegexpReplace requires pattern and replacement from UnaryExpr!");
    default:
        throw std::runtime_error("Not implemented unary op type!");
    }
}
