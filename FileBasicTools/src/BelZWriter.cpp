#include "BelZWriter.h"
#include "CSVReader.h"
#include "Column.h"
#include "Scheme.h"
#include <charconv>
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
    fout_.rdbuf()->pubsetbuf(stream_buffer_.data(), static_cast<std::streamsize>(stream_buffer_.size()));
    fout_.open(dest_path , std::ios::binary | std::ios::out);
    buf_.resize(STANDART_BUCKET_SIZE * 2);
    if (!fout_.is_open()) {
        throw std::runtime_error("Failed to create file: " + dest_path.string());
    }
}

uint64_t BelZWriter::GetOffSet() {
    return fout_.tellp();
}

void BelZWriter::Append(const char* data , size_t size , ColumnType type) {
    //std::cerr << "APPEND" << std::endl;
    if (type == ColumnType::String) {
        AppendString(data , size);
        return;
    }
    if (type == ColumnType::Int64) {
        AppendInt64(data , size);
        //std::cerr << "APINT " << buf_ << std::endl;
        return;
    }
    throw std::runtime_error("Try to Write to BelZFormat unknown type: " + std::to_string(uint8_t(type)));
}

void BelZWriter::AppendColumn(std::shared_ptr<Column> column , ColumnType type) {
    if (type == ColumnType::Int64) {
        size_t sz = As<Int64Column>(column)->Size();
        EnsureCapacity(sizeof(int64_t) * sz);
        memcpy(buf_.data() + offset_ , As<Int64Column>(column)->Data() , sizeof(int64_t) * sz);
        offset_ += sizeof(int64_t) * sz;
        return;
    }
    if (type == ColumnType::String) {
        size_t data_sz = As<StringColumn>(column)->GetDataSize();
        size_t offset_sz = As<StringColumn>(column)->Size();
        EnsureCapacity(offset_sz * sizeof(size_t) + data_sz);
        memcpy(buf_.data() + offset_ , As<StringColumn>(column)->GetDataPointer() , data_sz);
        offset_ += data_sz;
        memcpy(buf_.data() + offset_ , As<StringColumn>(column)->GetOffsetPointer() , offset_sz * sizeof(size_t));
        offset_ += offset_sz * sizeof(size_t);
        return;
    }
    throw std::runtime_error("Try to write in Belz unknown type of Column!");
}

inline void BelZWriter::AppendInt64(const char* data , size_t size) {
    int64_t val = 0;
    std::from_chars(data , data + size , val);
    EnsureCapacity(sizeof(val));
    memcpy(buf_.data() + offset_ , reinterpret_cast<char*>(&val) , sizeof(val));
    offset_ += sizeof(val);
}

inline void BelZWriter::AppendString(const char* data , size_t size) {
    EnsureCapacity(size + sizeof(size));
    memcpy(buf_.data() + offset_ , reinterpret_cast<char*>(&size) , sizeof(size));
    offset_ += sizeof(size);
    memcpy(buf_.data() + offset_ , data , size);
    offset_ += size;
}


void BelZWriter::WriteData(const char* data , size_t size , ColumnType type) {
    if (type == ColumnType::Int64) {
        WriteInt64(data , size);
        return;
    }
    if (type == ColumnType::String) {
        WriteString(data , size);
        return;
    }
    throw std::runtime_error("Try to Write to BelZFormat unknown type: " + std::to_string(uint8_t(type)));
}

void BelZWriter::WriteInt64(const char* data , size_t size) {
    int64_t val = 0;
    std::from_chars(data , data + size , val);
    fout_.write(reinterpret_cast<char*>(&val) , sizeof(val));
}

void BelZWriter::WriteString(const char* data , size_t size) {
    fout_.write(reinterpret_cast<char*>(&size) , sizeof(size));
    fout_.write(data , size);
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
