#include "BelZReader.h"
#include "Column.h"
#include "Scheme.h"
#include <filesystem>
#include <stdexcept>
#include <string>

BelZReader::BelZReader(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        throw std::runtime_error("File not found: " + filePath);
    }
    stream_ = std::ifstream(filePath , std::ios::binary);
    if (!stream_.is_open() || !stream_) {
        throw std::runtime_error("Failed to open CSV for reading: " + filePath);
    }
    ReadMetaData();
}

bool BelZReader::Empty() const {
    return batches_left_ == 0;
}

std::shared_ptr<Column> BelZReader::ReadInt64Column(size_t size) {
    auto result = std::make_shared<Int64Column>();
    std::vector<int64_t> buffer(size);
    if (size > 0) {
        stream_.read(reinterpret_cast<char*>(buffer.data()), size * sizeof(int64_t));
        if (!stream_) throw std::runtime_error("Failed to read Int64 block");
    }
    for (int64_t val : buffer) {
        result->Push_Back(val);
    }
    return result;
}

std::shared_ptr<Column> BelZReader::ReadStringColumn(size_t size) {
    auto result = std::make_shared<StringColumn>();
    for (size_t i = 0; i < size; ++i) {
        size_t len;
        stream_.read(reinterpret_cast<char*>(&len) , sizeof(len));
        std::string str;
        str.resize(len);
        if (len > 0) {
            stream_.read(&str[0], static_cast<std::streamsize>(len));
            if (!stream_) {
                throw std::runtime_error("Failed to read string data");
            }
        }
        result->Push_Back(std::move(str));
    }
    return result;
}

std::shared_ptr<Column> BelZReader::ReadColumn(size_t size , ColumnType type) {
    if (type == ColumnType::Int64) {
        return ReadInt64Column(size);
    }
    if (type == ColumnType::String) {
        return ReadStringColumn(size);
    }
    throw std::runtime_error("Try to read from .belz unknown ColumnType:" + std::to_string(static_cast<int8_t>(type)));
}

void BelZReader::ReadMetaData() {
    stream_.seekg(-8, std::ios::end);
    uint64_t meta_offset;
    stream_.read(reinterpret_cast<char*>(&meta_offset) , sizeof(meta_offset));
    stream_.seekg(meta_offset , std::ios::beg);
    size_t col_count;
    stream_.read(reinterpret_cast<char*>(&col_count) , sizeof(col_count));
    scheme_.Clear(); 
    for (size_t i = 0; i < col_count; ++i) {
        size_t len;
        stream_.read(reinterpret_cast<char*>(&len) , sizeof(len));
        std::string name;
        name.resize(len);
        if (len > 0) {
            stream_.read(&name[0], len);
        }
        uint8_t type;
        stream_.read(reinterpret_cast<char*>(&type) , sizeof(type));
        scheme_.Push_Back(SchemeNode{name, static_cast<ColumnType>(type)});
    }
    stream_.read(reinterpret_cast<char*>(&batches_left_) , sizeof(batches_left_));
    for (size_t i = 0; i < batches_left_; ++i) {
        size_t offset;
        stream_.read(reinterpret_cast<char*>(&offset) , sizeof(offset));
        meta_.AddOffset(offset);
    }
    for (size_t i = 0; i < batches_left_; ++i) {
        size_t rows;
        stream_.read(reinterpret_cast<char*>(&rows) , sizeof(rows));
        meta_.AddRows(rows);
    }
}

void BelZReader::ReadBatch(Batch& batch) {
    if (batches_left_ == 0) {
        throw std::runtime_error("Try to read Batch after end of file!");
    }
    batch.Clear(); 
    size_t current_offset = meta_.GetOffset(index_of_batch);
    stream_.seekg(current_offset, std::ios::beg);
    batch.SetScheme(scheme_); 
    size_t rows_count = meta_.GetRow(index_of_batch);

    for (size_t column = 0; column < scheme_.Size(); ++column) {
        auto col_ptr = ReadColumn(rows_count, scheme_.GetType(column));
        batch.AddColumn(col_ptr);
    }
    --batches_left_;
    ++index_of_batch;
}