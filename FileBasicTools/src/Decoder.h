#pragma once
#include "Coder.h"
#include <cstring>
#include <memory>
#include <stdexcept>

namespace Decoder {

struct EncodedColumnView {
    CodecType codec;
    const uint8_t* data;
    size_t size;
};

template <typename T , typename ColumnT>
inline std::shared_ptr<Column> DecodeRawFixed(EncodedColumnView column) {
    auto result = std::make_shared<ColumnT>();
    size_t values_count = column.size / sizeof(T);
    result->Resize(values_count);
    if (values_count != 0) {
        memcpy(result->Data() , column.data , column.size);
    }
    return result;
}

inline std::shared_ptr<Column> DecodeRawString(EncodedColumnView column) {
    auto result = std::make_shared<StringColumn>();
    size_t data_sz = 0;
    size_t offset_sz = 0;
    size_t pos = 0;
    memcpy(&data_sz , column.data + pos , sizeof(data_sz));
    pos += sizeof(data_sz);
    memcpy(&offset_sz , column.data + pos , sizeof(offset_sz));
    pos += sizeof(offset_sz);
    result->ResizeData(data_sz);
    result->ResizeOffset(offset_sz);
    if (data_sz != 0) {
        memcpy(result->GetDataPointer() , column.data + pos , data_sz);
        pos += data_sz;
    }
    if (offset_sz != 0) {
        memcpy(result->GetOffsetPointer() , column.data + pos , offset_sz * sizeof(size_t));
    }
    return result;
}

inline std::shared_ptr<Column> DecodeRaw(EncodedColumnView column , ColumnType type) {
    switch(type) {
        case ColumnType::Int8:
            return DecodeRawFixed<int8_t , Int8Column>(column);
        case ColumnType::Int16:
            return DecodeRawFixed<int16_t , Int16Column>(column);
        case ColumnType::Int32:
            return DecodeRawFixed<int32_t , Int32Column>(column);
        case ColumnType::Int64:
            return DecodeRawFixed<int64_t , Int64Column>(column);
        case ColumnType::Double:
            return DecodeRawFixed<double , DoubleColumn>(column);
        case ColumnType::Date:
            return DecodeRawFixed<int64_t , DateColumn>(column);
        case ColumnType::Timestamp:
            return DecodeRawFixed<int64_t , TimeStampColumn>(column);
        case ColumnType::Int128:
            return DecodeRawFixed<__int128_t , Int128Column>(column);
        case ColumnType::String:
            return DecodeRawString(column);
        case ColumnType::Unknown:
            break;
    }
    throw std::runtime_error("Unknown column type in raw decoder!");
}

inline std::shared_ptr<Column> DecodeDictionary(EncodedColumnView column , ColumnType type) {
    if (type != ColumnType::String) {
        throw std::runtime_error("Dictionary codec is supported only for string columns!");
    }
    size_t pos = 0;
    int16_t unique_count_signed = 0;
    memcpy(&unique_count_signed , column.data + pos , sizeof(unique_count_signed));
    pos += sizeof(unique_count_signed);
    size_t unique_count = static_cast<size_t>(unique_count_signed);

    std::vector<int32_t> offsets(unique_count);
    if (unique_count != 0) {
        memcpy(offsets.data() , column.data + pos , unique_count * sizeof(int32_t));
        pos += unique_count * sizeof(int32_t);
    }

    size_t dictionary_bytes = unique_count == 0 ? 0 : static_cast<size_t>(offsets.back());
    const char* dictionary_data = reinterpret_cast<const char*>(column.data + pos);
    pos += dictionary_bytes;

	auto result = std::make_shared<StringColumn>();
	size_t rows_count = (column.size - pos) / sizeof(int16_t);
	size_t indices_pos = pos;
	size_t decoded_data_size = 0;
	for (size_t i = 0; i < rows_count; ++i) {
	    int16_t index_signed = 0;
	    memcpy(&index_signed , column.data + pos , sizeof(index_signed));
	    pos += sizeof(index_signed);
	    size_t index = static_cast<size_t>(index_signed);
	    size_t start = index == 0 ? 0 : static_cast<size_t>(offsets[index - 1]);
	    size_t end = static_cast<size_t>(offsets[index]);
	    decoded_data_size += end - start;
	}

	result->Reserve(rows_count);
	result->ReserveData(decoded_data_size);
	pos = indices_pos;
	for (size_t i = 0; i < rows_count; ++i) {
	    int16_t index_signed = 0;
	    memcpy(&index_signed , column.data + pos , sizeof(index_signed));
	    pos += sizeof(index_signed);
	    size_t index = static_cast<size_t>(index_signed);
	    size_t start = index == 0 ? 0 : static_cast<size_t>(offsets[index - 1]);
	    size_t end = static_cast<size_t>(offsets[index]);
        result->AppendFromString(dictionary_data + start , end - start);
    }
	return result;
}

inline uint64_t ReadBits(const uint8_t* data , size_t& bit_offset , uint8_t bits) {
    const size_t byte_offset = bit_offset / 8;
    const uint8_t bit_shift = static_cast<uint8_t>(bit_offset % 8);
    const size_t bytes_needed = (static_cast<size_t>(bit_shift) + bits + 7) / 8;

    __uint128_t chunk = 0;
    for (size_t byte = 0; byte < bytes_needed; ++byte) {
        chunk |= static_cast<__uint128_t>(data[byte_offset + byte]) << (byte * 8);
    }

    chunk >>= bit_shift;
    bit_offset += bits;
    if (bits == 64) {
        return static_cast<uint64_t>(chunk);
    }
    return static_cast<uint64_t>(chunk) & ((uint64_t(1) << bits) - 1);
}

inline int64_t SignExtend(uint64_t value , uint8_t bits) {
    if (bits == 64) {
        return static_cast<int64_t>(value);
    }
    uint64_t sign_bit = uint64_t(1) << (bits - 1);
    if ((value & sign_bit) == 0) {
        return static_cast<int64_t>(value);
    }
    uint64_t mask = ~((uint64_t(1) << bits) - 1);
    return static_cast<int64_t>(value | mask);
}

template <typename T , typename ColumnT>
inline std::shared_ptr<Column> DecodeBitPackFixed(EncodedColumnView column) {
    size_t rows_count = 0;
    size_t pos = 0;
    memcpy(&rows_count , column.data + pos , sizeof(rows_count));
    pos += sizeof(rows_count);
    uint8_t bits_per_value = column.data[pos];
    ++pos;
    if (rows_count == 0) {
        return std::make_shared<ColumnT>();
    }

    auto result = std::make_shared<ColumnT>();
    result->Resize(rows_count);
    size_t bit_offset = pos * 8;
    T* out = result->Data();
    for (size_t i = 0; i < rows_count; ++i) {
        out[i] = static_cast<T>(SignExtend(ReadBits(column.data , bit_offset , bits_per_value) , bits_per_value));
    }
    return result;
}

inline std::shared_ptr<Column> DecodeBitPack(EncodedColumnView column , ColumnType type) {
    switch(type) {
        case ColumnType::Int8:
            return DecodeBitPackFixed<int8_t , Int8Column>(column);
        case ColumnType::Int16:
            return DecodeBitPackFixed<int16_t , Int16Column>(column);
        case ColumnType::Int32:
            return DecodeBitPackFixed<int32_t , Int32Column>(column);
        case ColumnType::Int64:
            return DecodeBitPackFixed<int64_t , Int64Column>(column);
        case ColumnType::Date:
            return DecodeBitPackFixed<int64_t , DateColumn>(column);
        case ColumnType::Timestamp:
            return DecodeBitPackFixed<int64_t , TimeStampColumn>(column);
        default:
            throw std::runtime_error("BitPack codec is supported only for integer-like columns!");
    }
}

inline std::shared_ptr<Column> DecodeColumn(EncodedColumnView column , ColumnType type) {
    switch(column.codec) {
        case CodecType::Raw:
            return DecodeRaw(column , type);
        case CodecType::Diction:
            return DecodeDictionary(column , type);
        case CodecType::BitPack:
            return DecodeBitPack(column , type);
        case CodecType::Delta:
            throw std::runtime_error("Delta decoder is not implemented!");
    }
    throw std::runtime_error("Unknown codec type in DecodeColumn!");
}

inline std::shared_ptr<Column> DecodeColumn(const EncodedColumn& column , ColumnType type) {
    return DecodeColumn(EncodedColumnView{column.codec , column.data.data() , column.data.size()} , type);
}

} // Decoder
