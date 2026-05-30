#pragma once
#include "Column.h"
#include "Scheme.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

template<typename T> concept StringLike = std::same_as<T, std::string> || std::same_as<T, std::string_view> || std::same_as<T, const char*> || std::same_as<T, char*>;

enum class CodecType : int8_t {
    Raw = 0,
    BitPack, // bit + delta for digits
    Delta, // 
    Diction // only for strings
};

struct EncodedColumn {
    CodecType codec;
    std::vector<uint8_t> data;
    EncodedColumn() : codec(CodecType::Raw) {
    }
    EncodedColumn(CodecType type , std::vector<uint8_t>&& bytes) : codec(type) , data(std::move(bytes)) {
    };
};


namespace Raw {
    template <typename T , typename ColumnT , typename = void>
    void Apply(std::shared_ptr<ColumnT> col , EncodedColumn& result) {
        if constexpr (std::same_as<ColumnT , StringColumn>) {
            size_t data_sz = col->GetDataSize();
            size_t offset_sz = col->Size();
            result.data.resize(offset_sz * sizeof(size_t) + data_sz + 2 * sizeof(size_t));
            size_t pos = 0;
            memcpy(result.data.data() + pos , &data_sz , sizeof(data_sz));
            pos += sizeof(data_sz);
            memcpy(result.data.data() + pos , &offset_sz , sizeof(offset_sz));
            pos += sizeof(offset_sz);
            memcpy(result.data.data() + pos , col->GetDataPointer() , data_sz);
            pos += data_sz;
            memcpy(result.data.data() + pos , col->GetOffsetPointer() , offset_sz * sizeof(size_t));
        } else {
            const T* data = col->Data();
            result.data.resize(sizeof(T) * col->Size());
            memcpy(result.data.data() , data , sizeof(T) * col->Size());
        }
        result.codec = CodecType::Raw;
    }

    inline void Apply(std::shared_ptr<Column> col , EncodedColumn& result) {
        if (!col) {
            throw std::runtime_error("Null column in Raw::Apply!");
        }
        switch(col->GetType()) {
            case ColumnType::Int8:
                Apply<int8_t>(As<Int8Column>(col) , result);
                return;
            case ColumnType::Int16:
                Apply<int16_t>(As<Int16Column>(col) , result);
                return;
            case ColumnType::Int32:
                Apply<int32_t>(As<Int32Column>(col) , result);
                return;
            case ColumnType::Int64:
                Apply<int64_t>(As<Int64Column>(col) , result);
                return;
            case ColumnType::Double:
                Apply<double>(As<DoubleColumn>(col) , result);
                return;
            case ColumnType::Date:
                Apply<int64_t>(As<DateColumn>(col) , result);
                return;
            case ColumnType::Timestamp:
                Apply<int64_t>(As<TimeStampColumn>(col) , result);
                return;
            case ColumnType::Int128:
                Apply<__int128_t>(As<Int128Column>(col) , result);
                return;
            case ColumnType::String:
                Apply<StringColumn>(As<StringColumn>(col) , result);
                return;
            case ColumnType::Unknown:
                break;
        }
        throw std::runtime_error("Unknown column type in Raw::Apply!");
    }

} // Raw

namespace BitPack {
    template <typename T>
    uint8_t RequiredBits(T value) {
        int64_t signed_value = static_cast<int64_t>(value);
        if (signed_value == std::numeric_limits<int64_t>::min()) {
            return 64;
        }
        uint64_t abs_value = static_cast<uint64_t>(std::llabs(signed_value));
        if (abs_value == 0) {
            return 1;
        }
        return static_cast<uint8_t>(std::bit_width(abs_value) + 1);
    }
    template <typename T>
    uint64_t EncodeSigned(T value, uint8_t bits_per_value) {
        int64_t signed_value = static_cast<int64_t>(value);
        uint64_t mask;
        if (bits_per_value == 64) {
            mask = ~uint64_t(0);
        } else {
            mask = (uint64_t(1) << bits_per_value) - 1;
        }
        return static_cast<uint64_t>(signed_value) & mask;
    }
    inline void WriteBits(std::vector<uint8_t>& out , size_t& bit_offset , uint64_t value , uint8_t bits) {
        for (uint8_t bit = 0; bit < bits; ++bit) {
            const size_t target_bit = bit_offset + bit;
            if ((value >> bit) & 1ULL) {
                out[target_bit / 8] |= static_cast<uint8_t>(1U << (target_bit % 8));
            }
        }
        bit_offset += bits;
    }
	    template <typename ColumnT>
	    size_t CountSize(std::shared_ptr<ColumnT> col) {
	        if (!col || col->Size() == 0) {
	            return sizeof(size_t) + sizeof(uint8_t);
	        }
        uint8_t bits_per_value = 1;
        const auto* data = col->Data();
        for (size_t i = 0; i < col->Size(); ++i) {
            bits_per_value = std::max(bits_per_value , RequiredBits(data[i]));
        }
	        size_t payload_bits = static_cast<size_t>(bits_per_value) * col->Size();
	        return sizeof(size_t) + sizeof(uint8_t) + (payload_bits + 7) / 8;
	    }
	    inline size_t CountSize(std::shared_ptr<Column> col) {
	        if (!col) {
	            throw std::runtime_error("Null column in BitPack::CountSize!");
	        }
	        switch(col->GetType()) {
	            case ColumnType::Int8:
	                return CountSize(As<Int8Column>(col));
	            case ColumnType::Int16:
	                return CountSize(As<Int16Column>(col));
	            case ColumnType::Int32:
	                return CountSize(As<Int32Column>(col));
	            case ColumnType::Int64:
	                return CountSize(As<Int64Column>(col));
	            case ColumnType::Date:
	                return CountSize(As<DateColumn>(col));
	            case ColumnType::Timestamp:
	                return CountSize(As<TimeStampColumn>(col));
	            default:
	                throw std::runtime_error("String or unknown type column in BitPack::CountSize!");
	        }
	    }
		template <typename ColumnT>
		void Apply(std::shared_ptr<ColumnT> col , EncodedColumn& result) {
        size_t rows_count = col ? col->Size() : 0;
        if (!col || col->Size() == 0) {
            std::vector<uint8_t> bytes(sizeof(size_t) + sizeof(uint8_t) , 0);
            memcpy(bytes.data() , &rows_count , sizeof(rows_count));
            result = {CodecType::BitPack , std::move(bytes)};
            return;
        }
        uint8_t bits_per_value = 1;
        const auto* data = col->Data();
        for (size_t i = 0; i < col->Size(); ++i) {
            bits_per_value = std::max(bits_per_value , RequiredBits(data[i]));
        }
        std::vector<uint8_t> bytes(CountSize(col) , 0);
        size_t pos = 0;
        memcpy(bytes.data() + pos , &rows_count , sizeof(rows_count));
        pos += sizeof(rows_count);
        bytes[pos] = bits_per_value;
        ++pos;
        size_t offset = pos * 8;
        for (size_t i = 0; i < col->Size(); ++i) {
            WriteBits(bytes , offset , EncodeSigned(data[i] , bits_per_value) , bits_per_value);
        }
        result = {CodecType::BitPack , std::move(bytes)};
    }
    inline void Apply(std::shared_ptr<Column> col , EncodedColumn& result) {
        if (!col) {
            throw std::runtime_error("Null column in Raw::Apply!");
        }
        switch(col->GetType()) {
            case ColumnType::Int8:
                Apply(As<Int8Column>(col) , result);
                return;
            case ColumnType::Int16:
                Apply(As<Int16Column>(col) , result);
                return;
            case ColumnType::Int32:
                Apply(As<Int32Column>(col) , result);
                return;
            case ColumnType::Int64:
                Apply(As<Int64Column>(col) , result);
                return;
            case ColumnType::Date:
                Apply(As<DateColumn>(col) , result);
                return;
            case ColumnType::Timestamp:
                Apply(As<TimeStampColumn>(col) , result);
                return;
            default: 
                throw std::runtime_error("String or unknown type column in BitPack!");
        }
        throw std::runtime_error("Unknown column type in Raw::Apply!");
    }
} // BitPack

namespace Dictionary {
    void Apply(std::shared_ptr<StringColumn> col , EncodedColumn& result);
} // Dictionary


EncodedColumn GetBestCompression(std::shared_ptr<Column> col);
