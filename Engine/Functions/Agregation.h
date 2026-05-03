#pragma once
#include "../../FileBasicTools/DataStructures/Column.h"
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <memory>

namespace functions {
    template <typename TColumn>
    __int128_t Sum(const TColumn& column , const boost::dynamic_bitset<>& mask) {
        __int128_t sum = 0;
        const int64_t* data = column.Data();
        for (size_t i = 0; i < column.Size(); ++i) {
            sum += data[i] * mask[i];
        }
        return sum;
    }

    template <typename TColumn>
    int64_t Min(const TColumn& column , const boost::dynamic_bitset<>& mask) {
        int64_t result = INT64_MAX;
        const int64_t* data = column.Data();
        for (size_t i = 0; i < column.Size(); ++i) {
            result = std::min(result, data[i] + !mask[i] * (INT64_MAX - data[i]));
        }
        return result;
    }
    

    template <typename TColumn>
    int64_t Max(const TColumn& column , const boost::dynamic_bitset<>& mask) {
        int64_t result = INT64_MIN;
        const int64_t* data = column.Data();
        for (size_t i = 0; i < column.Size(); ++i) {
            result = std::max(result, data[i] - !mask[i] * (data[i] - INT64_MIN));
        }
        return result;
    }

    template <typename TColumn>
    int64_t AVG(const TColumn& column , const boost::dynamic_bitset<>& mask) {
        __int128_t sum = Sum(column , mask);
        return static_cast<int64_t>(sum / mask.count());
    }
}