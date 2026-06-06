#include "Coder.h"
#include <unordered_map>

void Dictionary::Apply(std::shared_ptr<FlatStringColumn> col, EncodedColumn &result) {
    const size_t col_size = col->Size();

    size_t last = 0;
    std::unordered_map<std::string_view, int16_t> dictionary;
    dictionary.reserve(col_size); 

    std::vector<int32_t> offset;
    std::vector<int16_t> index;
    std::vector<std::string_view> unique_string;

    index.reserve(col_size);
    size_t expected_unique = std::min<size_t>(8192, col_size); 
    unique_string.reserve(expected_unique);
    offset.reserve(expected_unique);

    size_t total_strings_length = 0;

    for (size_t i = 0; i < col_size; ++i) {
        std::string_view current_str = col->At(i); 
        auto [it, inserted] = dictionary.try_emplace(current_str, static_cast<int16_t>(last));
        if (inserted) {
            index.push_back(static_cast<int16_t>(last));
            unique_string.push_back(current_str);
            int32_t current_offset = (last == 0) ? current_str.size() : offset.back() + current_str.size();
            offset.push_back(current_offset);
            total_strings_length += current_str.size();
            last++;
        } else {
            index.push_back(it->second);
        }
    }

    int16_t offset_sz = static_cast<int16_t>(last);
    size_t total_size = sizeof(int16_t) + (sizeof(int32_t) * last) +  total_strings_length + (sizeof(int16_t) * col_size);

    std::vector<uint8_t> data;
    data.resize(total_size);
    uint8_t* ptr = data.data();
    memcpy(ptr, &offset_sz, sizeof(int16_t));
    ptr += sizeof(int16_t);
    memcpy(ptr, offset.data(), sizeof(int32_t) * last);
    ptr += sizeof(int32_t) * last;
    for (size_t i = 0; i < last; ++i) {
        size_t sz = unique_string[i].size();
        memcpy(ptr, unique_string[i].data(), sz);
        ptr += sz;
    }
    memcpy(ptr, index.data(), sizeof(int16_t) * col_size);
    result.data = std::move(data);
    result.codec = CodecType::Diction;
}

EncodedColumn GetBestCompression(std::shared_ptr<Column> col) {
    switch(col->GetType()) {
        case ColumnType::FlatString: {
            EncodedColumn raw_encode;
            EncodedColumn dic_encode;
            Raw::Apply<FlatStringColumn>(As<FlatStringColumn>(col) , raw_encode);
            Dictionary::Apply(As<FlatStringColumn>(col) , dic_encode);
            if (dic_encode.data.size() < raw_encode.data.size()) {
                return dic_encode;
            } else {
                return raw_encode;
            }
        };
	        default: {
	            EncodedColumn raw_encode;
	            Raw::Apply(col , raw_encode);
	            switch(col->GetType()) {
	                case ColumnType::Int8:
	                case ColumnType::Int16:
	                case ColumnType::Int32:
	                case ColumnType::Int64:
	                case ColumnType::Date:
	                case ColumnType::Timestamp:
	                    break;
	                default:
	                    return raw_encode;
	            }
	            if (BitPack::CountSize(col) * 2 <= raw_encode.data.size()) {
	                EncodedColumn bit_encode;
	                BitPack::Apply(col , bit_encode);
	                return bit_encode;
	            }
	            return raw_encode;
	        }
	    }
	}
