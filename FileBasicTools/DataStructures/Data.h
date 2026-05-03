#pragma once

namespace Data {
    enum class DataType : uint8_t {
        Int8 = 0,
        Int16 = 1,
        Int32 = 2,
        Int64 = 3,
        Int128 = 4,
        String = 5,
        Timestamp = 6,
        Data = 7, 
        
        Unknown = 255
    };
}