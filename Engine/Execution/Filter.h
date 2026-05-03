#pragma once
#include "../../FileBasicTools/DataStructures/Column.h"
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <bitset>
#include <memory>

namespace Filters {
    enum class FilterOp {
        EQUALS = 0,
        NOT_EQUALS = 1,
        GREATER = 2,
        LESS = 3,
        GREATER_OR_EQUALS = 4,
        LESS_OR_EQUALS = 5
    };
    template <typename TColumn>
    inline boost::dynamic_bitset<> ColumnFilter(TColumn& column , FilterOp type , const std::string& value) {
        
    }
}