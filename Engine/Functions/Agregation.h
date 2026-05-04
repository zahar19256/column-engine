#pragma once
#include "../../FileBasicTools/DataStructures/Column.h"
#include "Functions.h"
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <memory>

namespace Functions {
    struct AgregationCall {
        FunctionsType func_type;
        std::string_view column_name;
        std::string alias;
    };
}