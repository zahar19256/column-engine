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

    inline bool CheckLikeSymbol(std::string_view value) {
        for (const char c : value) {
            if (c == '%' || c == '_' || c == '\\') {
                return true;
            }
        }
        return false;
    }

    template<typename T>
    concept StringLike = std::same_as<T, std::string> ||
                     std::same_as<T, std::string_view> ||
                     std::same_as<T, const char*> ||
                     std::same_as<T, char*>;
    template <typename ColumnType , StringLike ValueType>
    inline boost::dynamic_bitset<> Contain(const ColumnType& column , const ValueType& value , bool Not = false) {
        const std::string_view needle(value);
        boost::dynamic_bitset<> result(column.Size());
        for (size_t index = 0; index < column.Size(); ++index) {
            result[index] = Not ^ (column.At_view(index).find(needle) != std::string_view::npos);
        }
        return result;
    }

    template <typename ColumnType , StringLike ValueType>
    inline boost::dynamic_bitset<> StartWith(const ColumnType& column , const ValueType& value , bool Not = false) {
        const std::string_view prefix(value);
        boost::dynamic_bitset<> result(column.Size());
        for (size_t index = 0; index < column.Size(); ++index) {
            result[index] = Not ^ column.At_view(index).starts_with(prefix);
        }
        return result;
    }

    template <typename ColumnType , StringLike ValueType>
    inline boost::dynamic_bitset<> EndWith(const ColumnType& column , const ValueType& value , bool Not = false) {
        const std::string_view suffix(value);
        boost::dynamic_bitset<> result(column.Size());
        for (size_t index = 0; index < column.Size(); ++index) {
            result[index] = Not ^ column.At_view(index).ends_with(suffix);
        }
        return result;
    }

    template <typename ColumnType, StringLike ValueType>
    inline boost::dynamic_bitset<> MakeLike(const ColumnType& column , const ValueType& value , bool Not = false) {
        const std::string_view pattern(value);
        if (pattern.empty()) {
            return MakeEqual(column , std::string_view("") , Not);
        }
        if (!CheckLikeSymbol(pattern)) {
            return MakeEqual(column , pattern , Not);
        }
        if (pattern.size() >= 2 && pattern.front() == '%' && pattern.back() == '%' && !CheckLikeSymbol(pattern.substr(1 , pattern.size() - 2))) {
            return Contain(column , pattern.substr(1 , pattern.size() - 2) , Not);
        }
        if (pattern.back() == '%' && !CheckLikeSymbol(pattern.substr(0 , pattern.size() - 1))) {
            return StartWith(column , pattern.substr(0 , pattern.size() - 1) , Not);
        }
        if (pattern.front() == '%' && !CheckLikeSymbol(pattern.substr(1))) {
            return EndWith(column , pattern.substr(1) , Not);
        }
        std::string regex = "^";
        for (size_t i = 0; i < pattern.size(); ++i) {
            const char c = pattern[i];
            if (c == '\\' && i + 1 < pattern.size()) {
                AppendRegexLiteral(regex , pattern[++i]);
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
            result[index] = Not ^ RE2::FullMatch(column.At_view(index) , compiled_regex);
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
