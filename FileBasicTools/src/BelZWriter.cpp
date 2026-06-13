#include "BelZWriter.h"
#include "CsvReader.h"
#include "Coder.h"
#include "Column.h"
#include "Scheme.h"
#include <charconv>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory.h>
#include <memory>
#include <stdexcept>
#include <string>

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

BelZWriter::BelZWriter(const std::string& CSVFilePath)
    : BelZWriter(CSVFilePath, [](const std::string& path) {
          std::filesystem::path dest_path(path);
          dest_path.replace_extension(".belZ");
          return dest_path.string();
      }(CSVFilePath)) {}

BelZWriter::BelZWriter(const std::string& CSVFilePath, const std::string& outputFilePath) {
    (void)CSVFilePath;
    std::filesystem::path dest_path(outputFilePath);
    if (dest_path.has_parent_path()) {
        std::filesystem::create_directories(dest_path.parent_path());
    }
    fout_.open(dest_path, std::ios::binary | std::ios::out);
    buf_.resize(STANDART_BUCKET_SIZE * 2);
    if (!fout_.is_open()) {
        throw std::runtime_error("Failed to create file: " + dest_path.string());
    }
}

uint64_t BelZWriter::GetOffSet() const {
    return static_cast<uint64_t>(fout_.tellp()) + offset_;
}

void BelZWriter::Append(const char* data, size_t size, ColumnType type) {
    // std::cerr << "APPEND" << std::endl;
    if (type == ColumnType::String) {
        AppendString(data, size);
        return;
    }
    if (type == ColumnType::Int8) {
        AppendNumber<int8_t>(data, size);
        // std::cerr << "APINT " << buf_ << std::endl;
        return;
    }
    if (type == ColumnType::Int16) {
        AppendNumber<int16_t>(data, size);
        // std::cerr << "APINT " << buf_ << std::endl;
        return;
    }
    if (type == ColumnType::Int32) {
        AppendNumber<int32_t>(data, size);
        // std::cerr << "APINT " << buf_ << std::endl;
        return;
    }
    if (type == ColumnType::Int64) {
        AppendNumber<int64_t>(data, size);
        // std::cerr << "APINT " << buf_ << std::endl;
        return;
    }
    if (type == ColumnType::Double) {
        AppendNumber<double>(data, size);
        // std::cerr << "APINT " << buf_ << std::endl;
        return;
    }
    throw std::runtime_error("Try to Write to BelZFormat unknown type: " + std::to_string(uint8_t(type)));
}

void BelZWriter::AppendColumn(std::shared_ptr<Column> column, ColumnType type) {
    EncodedColumn result = GetBestCompression(column);
    size_t payload_size = result.data.size();
    EnsureCapacity(sizeof(result.codec) + sizeof(payload_size) + payload_size);
    memcpy(buf_.data() + offset_, &result.codec, sizeof(result.codec));
    offset_ += sizeof(result.codec);
    memcpy(buf_.data() + offset_, &payload_size, sizeof(payload_size));
    offset_ += sizeof(payload_size);
    memcpy(buf_.data() + offset_, result.data.data(), payload_size);
    offset_ += payload_size;
}

inline void BelZWriter::AppendString(const char* data, size_t size) {
    EnsureCapacity(size + sizeof(size));
    memcpy(buf_.data() + offset_, reinterpret_cast<char*>(&size), sizeof(size));
    offset_ += sizeof(size);
    memcpy(buf_.data() + offset_, data, size);
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
