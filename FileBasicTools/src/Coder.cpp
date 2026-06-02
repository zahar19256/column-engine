#include "Coder.h"
#include <unordered_map>

void Dictionary::Apply(std::shared_ptr<StringColumn> col, EncodedColumn &result) {
    if (!col || col->Size() == 0) {
        throw std::runtime_error("Empty/Null column in Dictionary::Apply!");
    }
    size_t last = 0;
    std::unordered_map<std::string_view , size_t> dictionary;
    dictionary.reserve(col->Size());
    std::vector<int32_t> offset;
    std::vector<int16_t> index;
    std::vector<std::string_view> unique_string;
    unique_string.reserve(col->Size());
    index.reserve(col->Size());
    for (size_t i = 0; i < col->Size(); ++i) {
        if (dictionary.count(col->At(i))) {
            index.push_back(dictionary[col->At(i)]);
            continue;
        }
        index.push_back(last);
        unique_string.push_back(col->At(i));
        dictionary[col->At(i)] = last++;
    }
    offset.resize(last);
    offset[0] = unique_string[0].size();
    for (size_t i = 1; i < last; ++i) {
        offset[i] = offset[i - 1] + unique_string[i].size();
    }
    std::vector<uint8_t> data;
    int16_t offset_sz = unique_string.size();
    size_t total_size = sizeof(int16_t) + sizeof(int32_t) * offset.size() + offset.back() + sizeof(int16_t) * index.size();
    data.resize(total_size);
    size_t pos = 0;
    memcpy(data.data() + pos , &offset_sz , sizeof(int16_t));
    pos += sizeof(int16_t);
    memcpy(data.data() + pos , offset.data() , sizeof(int32_t) * last);
    pos += last * sizeof(int32_t);
    for (size_t i = 0; i < last; ++i) {
        memcpy(data.data() + pos , unique_string[i].data() , unique_string[i].size());
        pos += unique_string[i].size();
    }
    memcpy(data.data() + pos , index.data() , sizeof(int16_t) * index.size());
    result.data = std::move(data);
    result.codec = CodecType::Diction;
}


EncodedColumn GetBestCompression(std::shared_ptr<Column> col) {
    switch(col->GetType()) {
        case ColumnType::String: {
            EncodedColumn raw_encode;
            EncodedColumn dic_encode;
            Raw::Apply<StringColumn>(As<StringColumn>(col) , raw_encode);
            Dictionary::Apply(As<StringColumn>(col) , dic_encode);
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