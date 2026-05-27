#include "BelZWriter.h"
#include "CSVReader.h"
#include "Column.h"
#include "Scheme.h"
#include <charconv>
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <filesystem>
#include <iostream>
#include <string>
#include <memory.h>

void BelZWriter::EnsureCapacity(size_t additional_size) {
    if (offset_ + additional_size > buf_.size()) {
        size_t new_capacity = buf_.size();
        if (new_capacity == 0) {
            new_capacity = 4096;
        }
        while (new_capacity < offset_ + additional_size) {
            new_capacity <<= 1;
        }
        buf_.resize(new_capacity);
    }
}

BelZWriter::BelZWriter(const std::string& CSVFilePath) {
    std::filesystem::path src_path(CSVFilePath);
    std::filesystem::path dest_path = src_path;
    dest_path.replace_extension(".belZ");
    fout_.open(dest_path , std::ios::binary | std::ios::out);
    buf_.resize(STANDART_BUCKET_SIZE * 2);
    if (!fout_.is_open()) {
        throw std::runtime_error("Failed to create file: " + dest_path.string());
    }
}

uint64_t BelZWriter::GetOffSet() {
    return static_cast<uint64_t>(fout_.tellp()) + offset_;
}

void BelZWriter::Append(const char* data , size_t size , ColumnType type) {
    //std::cerr << "APPEND" << std::endl;
    if (type == ColumnType::String) {
        AppendString(data , size);
        return;
    }
    if (type == ColumnType::Int8) {
        AppendNumber<int8_t>(data , size);
        //std::cerr << "APINT " << buf_ << std::endl;
        return;
    }
    if (type == ColumnType::Int16) {
        AppendNumber<int16_t>(data , size);
        //std::cerr << "APINT " << buf_ << std::endl;
        return;
    }
    if (type == ColumnType::Int32) {
        AppendNumber<int32_t>(data , size);
        //std::cerr << "APINT " << buf_ << std::endl;
        return;
    }
    if (type == ColumnType::Int64) {
        AppendNumber<int64_t>(data , size);
        //std::cerr << "APINT " << buf_ << std::endl;
        return;
    }
    if (type == ColumnType::Double) {
        AppendNumber<double>(data , size);
        //std::cerr << "APINT " << buf_ << std::endl;
        return;
    }
    throw std::runtime_error("Try to Write to BelZFormat unknown type: " + std::to_string(uint8_t(type)));
}

void BelZWriter::AppendColumn(std::shared_ptr<Column> column , ColumnType type) {
    auto append_fixed_width = [this, &column]<typename ColumnT, typename ValueT>() {
        auto typed_column = As<ColumnT>(column);
        if (!typed_column) {
            throw std::runtime_error("Column type mismatch in BelZWriter::AppendColumn!");
        }

        const size_t sz = typed_column->Size();
        const size_t bytes = sizeof(ValueT) * sz;
        EnsureCapacity(bytes);
        if (bytes != 0) {
            memcpy(buf_.data() + offset_ , typed_column->Data() , bytes);
            offset_ += bytes;
        }
    };

    switch(type) {
        case ColumnType::Int8:
            append_fixed_width.template operator()<Int8Column, int8_t>();
            return;
        case ColumnType::Int16:
            append_fixed_width.template operator()<Int16Column, int16_t>();
            return;
        case ColumnType::Int32:
            append_fixed_width.template operator()<Int32Column, int32_t>();
            return;
        case ColumnType::Int64:
            append_fixed_width.template operator()<Int64Column, int64_t>();
            return;
        case ColumnType::Int128:
            append_fixed_width.template operator()<Int128Column, __int128_t>();
            return;
        case ColumnType::Double:
            append_fixed_width.template operator()<DoubleColumn, double>();
            return;
        case ColumnType::Date:
            append_fixed_width.template operator()<DateColumn, int64_t>();
            return;
        case ColumnType::Timestamp:
            append_fixed_width.template operator()<TimeStampColumn, int64_t>();
            return;
        case ColumnType::String: {
            auto string_column = As<StringColumn>(column);
            if (!string_column) {
                throw std::runtime_error("Column type mismatch in BelZWriter::AppendColumn!");
            }
            size_t data_sz = string_column->GetDataSize();
            size_t offset_sz = string_column->Size();
            EnsureCapacity(offset_sz * sizeof(size_t) + data_sz + 2 * sizeof(size_t));
            memcpy(buf_.data() + offset_ , &data_sz , sizeof(data_sz));
            offset_ += sizeof(data_sz);
            memcpy(buf_.data() + offset_ , &offset_sz , sizeof(offset_sz));
            offset_ += sizeof(offset_sz);
            memcpy(buf_.data() + offset_ , string_column->GetDataPointer() , data_sz);
            offset_ += data_sz;
            memcpy(buf_.data() + offset_ , string_column->GetOffsetPointer() , offset_sz * sizeof(size_t));
            offset_ += offset_sz * sizeof(size_t);
            return;
        }
        default:
            throw std::runtime_error("Try to write in Belz unknown type of Column!");
    }
}

inline void BelZWriter::AppendString(const char* data , size_t size) {
    EnsureCapacity(size + sizeof(size));
    memcpy(buf_.data() + offset_ , reinterpret_cast<char*>(&size) , sizeof(size));
    offset_ += sizeof(size);
    memcpy(buf_.data() + offset_ , data , size);
    offset_ += size;
}

void BelZWriter::WriteScheme(const Scheme& scheme_) {
    for (size_t index = 0; index < scheme_.Size(); ++index) {
        std::string current_name = scheme_.GetName(index);
        size_t len = current_name.size();
        fout_.write(reinterpret_cast<char*>(&len), sizeof(len));
        fout_.write(current_name.data(), len);
        uint8_t type = static_cast<uint8_t>(scheme_.GetType(index));
        fout_.write(reinterpret_cast<char*>(&type), sizeof(type));
    }
}
