#pragma once
#include "../../FileBasicTools/DataStructures/Column.h"
#include "../../FileBasicTools/DataStructures/Types.h"
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <re2/re2.h>
#include <concepts>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Filters {
    enum class OpType {
        Equal,
        NotEqual,
        Less,
        LessOrEqual,
        Greater,
        GreaterOrEqual,
        Like,
        NotLike,
    };
    template <typename ColumnType , typename ValueType>
    inline boost::dynamic_bitset<> MakeEqual(const ColumnType& column , const ValueType& value , bool Not = false) {
        boost::dynamic_bitset<> result(column.Size());
        for (size_t index = 0; index < column.Size(); ++index) {
            result[index] = Not ^ (column.At(index) == value);
        }
        return result;
    }

    template <typename ColumnType , typename ValueType>
    inline boost::dynamic_bitset<> MakeLess(const ColumnType& column , const ValueType& value , bool Not = false) {
        boost::dynamic_bitset<> result(column.Size());
        for (size_t index = 0; index < column.Size(); ++index) {
            result[index] = Not ^ (column.At(index) < value);
        }
        return result;
    }

    template <typename ColumnType , typename ValueType>
    inline boost::dynamic_bitset<> MakeLessOrEqual(const ColumnType& column , const ValueType& value , bool Not = false) {
        boost::dynamic_bitset<> result(column.Size());
        for (size_t index = 0; index < column.Size(); ++index) {
            result[index] = Not ^ (column.At(index) <= value);
        }
        return result;
    }

    inline void AppendRegexLiteral(std::string& regex , char c) {
        if (strchr(".[](){}?+*|^$\\", c)) {
            regex += '\\';
        }
        regex += c;
    }

    template<typename T>
    concept StringLike = std::same_as<T, std::string> ||
                     std::same_as<T, std::string_view> ||
                     std::same_as<T, const char*> ||
                     std::same_as<T, char*>;
    template <typename ColumnType, StringLike ValueType>
    inline boost::dynamic_bitset<> MakeLike(const ColumnType& column , const ValueType& value , bool Not = false) {
        std::string regex = "^";
        for (size_t i = 0; i < value.size(); ++i) {
            const char c = value[i];
            if (c == '\\' && i + 1 < value.size()) {
                AppendRegexLiteral(regex , value[++i]);
            } else if (c == '%') {
                regex += ".*";
            } else if (c == '_') {
                regex += ".";
            } else {
                AppendRegexLiteral(regex , c);
            }
        }
        regex += '$';
        const RE2 compiled_regex(regex);
        if (!compiled_regex.ok()) {
            throw std::runtime_error("Invalid LIKE regex!");
        }
        boost::dynamic_bitset<> result(column.Size());
        for (size_t index = 0; index < column.Size(); ++index) {
            result[index] = Not ^ RE2::FullMatch(column.At(index) , compiled_regex);
        }
        return result;
    }

    template <typename ColumnType , typename ValueType>
    inline boost::dynamic_bitset<> ColumnTypedFilter(const ColumnType& column , OpType type , const ValueType& value) {
        switch(type) {
            case OpType::Equal:
                return MakeEqual(column , value);
            case OpType::NotEqual:
                return MakeEqual(column, value , true);
            case OpType::Less:
                return MakeLess(column , value);
            case OpType::LessOrEqual:
                return MakeLessOrEqual(column, value);
            case OpType::Greater:
                return MakeLessOrEqual(column, value , true);
            case OpType::GreaterOrEqual:
                return MakeLess(column, value , true);
            case OpType::Like:
                if constexpr (StringLike<ValueType>) {
                    return MakeLike(column , value);
                } else {
                    throw std::runtime_error("Like filter is support only for string columns!");
                }
            case OpType::NotLike:
                if constexpr (StringLike<ValueType>) {
                    return MakeLike(column , value , true);
                } else {
                    throw std::runtime_error("Like filter is support only for string columns!");
                }
            default:
                throw std::runtime_error("Unknown FilterType!");
        }
    }

    inline boost::dynamic_bitset<> ColumnFilter(const std::shared_ptr<Column>& column , OpType type , const std::string& value) {
        if (type == OpType::Like) {
            if (column->GetType() != ColumnType::String) {
                throw std::runtime_error("LIKE filter is supported only for string columns!");
            }
            return MakeLike(*As<StringColumn>(column), std::string_view(value));
        }

        switch(column->GetType()) {
            case ColumnType::Int8:
                return ColumnTypedFilter(*As<Int8Column>(column), type, Data::ParseInteger<int8_t>(value));
            case ColumnType::Int16:
                return ColumnTypedFilter(*As<Int16Column>(column), type, Data::ParseInteger<int16_t>(value));
            case ColumnType::Int32:
                return ColumnTypedFilter(*As<Int32Column>(column), type, Data::ParseInteger<int32_t>(value));
            case ColumnType::Int64:
                return ColumnTypedFilter(*As<Int64Column>(column), type, Data::ParseInteger<int64_t>(value));
            case ColumnType::String:
                return ColumnTypedFilter(*As<StringColumn>(column), type, std::string_view(value));
            case ColumnType::Double:
                return ColumnTypedFilter(*As<DoubleColumn>(column), type, Data::ParseDouble(value));
            case ColumnType::Date:
                return ColumnTypedFilter(*As<DateColumn>(column), type, Data::ParseDate(value));
            case ColumnType::Timestamp:
                return ColumnTypedFilter(*As<TimeStampColumn>(column), type, Data::ParseTimestamp(value));
            default: 
                throw std::runtime_error("Unknown Column type!");
        };
    }
} // namespace Filters
