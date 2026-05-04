#pragma once
#include "../../FileBasicTools/DataStructures/Column.h"
#include "../../FileBasicTools/DataStructures/Types.h"
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <bitset>
#include <memory>
#include <stdexcept>

namespace Filters {
    enum class OpType {
        Equal,
        NotEqual,
        Less,
        LessOrEqual,
        Greater,
        GreaterOrEqual,
        Like
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

    template <typename ColumnType , typename ValueType>
    inline boost::dynamic_bitset<> MakeLike(const ColumnType& column , const ValueType& value , bool Not = false) {
        boost::dynamic_bitset<> result(column.Size());
        for (size_t index = 0; index < column.Size(); ++index) {
            result[index] = Not ^ (column.At(index) <= value);
        }
        return result; // TODO ПЕРЕДЕЛАТЬ ЭТО ПОКА ЧТО <=
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
                return MakeLike(column , value);
            default:
                throw std::runtime_error("Unknown FilterType!");
        }
    }

    inline boost::dynamic_bitset<> ColumnFilter(const std::shared_ptr<Column>& column , OpType type , const std::string& value) {
        switch(column->GetType()) {
            case ColumnType::Int64:
                return ColumnTypedFilter(*As<Int64Column>(column), type, Data::ParseInt64(value));
            case ColumnType::String:
                return ColumnTypedFilter(*As<StringColumn>(column), type, std::string_view(value));
            case ColumnType::Double:
            case ColumnType::Date:
            case ColumnType::Timestamp:
            default: 
                throw std::runtime_error("Unknown Column type!");
        };
    }
}