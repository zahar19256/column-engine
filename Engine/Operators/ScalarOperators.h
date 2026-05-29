#pragma once
#include "../../FileBasicTools/DataStructures/Column.h"
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include "Scheme.h"

enum class BinaryExprType : int8_t {
    Add = 0,
    Sub,
};

enum class UnaryExprType : int8_t {
    Length = 0,
    ExtractMinute,
    RegexpReplace,
    DateFormatHour,
    DateTruncMinute
};

template <typename T>
struct IsDateOrTimeStampColumn : std::false_type {};

template <>
struct IsDateOrTimeStampColumn<DateColumn> : std::true_type {};

template <>
struct IsDateOrTimeStampColumn<TimeStampColumn> : std::true_type {};

template <typename T>
inline constexpr bool IsDateOrTimeStampColumnV = IsDateOrTimeStampColumn<T>::value;

template <typename T>
concept HasValueType = requires (T column){
    typename T::ValueType;
    column.Data();
};

template <typename T>
concept NumericColumn = HasValueType<T> && (
    std::is_integral_v<typename T::ValueType> ||
    std::is_floating_point_v<typename T::ValueType> ||
    std::is_same_v<typename T::ValueType, __int128_t>
);

std::shared_ptr<Column> ApplyBinaryOp(BinaryExprType op , std::shared_ptr<Column> left , std::shared_ptr<Column> right , ColumnType result_type);

std::shared_ptr<Column> ApplyUnaryOp(UnaryExprType op , std::shared_ptr<Column> current);
std::shared_ptr<Column> ApplyRegexpReplace(std::shared_ptr<Column> current , const std::string& pattern , const std::string& replacement);
