#include "BelZWriter.h"
#include <stdexcept>
#include <filesystem>
#include <iostream>
#include <string>

BelZWriter::BelZWriter(const std::string& CSVFilePath) {
    std::filesystem::path src_path(CSVFilePath);
    std::filesystem::path dest_path = src_path;
    dest_path.replace_extension(".belZ");
    fout_.open(dest_path , std::ios::binary | std::ios::out);
    if (!fout_.is_open()) {
        throw std::runtime_error("Failed to create file: " + dest_path.string());
    }
}

uint64_t BelZWriter::GetOffSet() {
    return fout_.tellp();
}

void BelZWriter::WriteData(const std::string& data , ColumnType type) {
    if (type == ColumnType::Int64) {
        WriteInt64(data);
        return;
    }
    if (type == ColumnType::String) {
        WriteString(data);
        return;
    }
    throw std::runtime_error("Try to Write to BelZFormat unknown type: " + std::to_string(uint8_t(type)));
}

void BelZWriter::WriteInt64(const std::string& data) {
    try {
        int64_t val = std::stoll(data);
        fout_.write(reinterpret_cast<const char*>(&val) , sizeof(val));
    } catch (...) {
        int64_t val = 0; 
        fout_.write(reinterpret_cast<const char*>(&val) , sizeof(val));
    }
}

void BelZWriter::WriteString(const std::string& data) {
    size_t len = data.size();
    fout_.write(reinterpret_cast<const char*>(&len) , sizeof(len));
    fout_.write(data.data(), len);
}

void BelZWriter::WriteMeta(const MetaData& meta_) {
    uint64_t meta_start_offset = static_cast<uint64_t>(fout_.tellp());
    size_t batches_count = meta_.Size();
    size_t col_count = meta_.GetScheme().Size();
    fout_.write(reinterpret_cast<const char*>(&col_count) , sizeof(col_count));
    fout_.write(reinterpret_cast<const char*>((meta_.GetScheme()).GetData()) , col_count * sizeof(SchemeNode));
    fout_.write(reinterpret_cast<const char*>(&batches_count) , sizeof(batches_count));
    fout_.write(reinterpret_cast<const char*>(meta_.GetOffsets().data()) , meta_.Size() * sizeof(size_t));
    fout_.write(reinterpret_cast<const char*>(meta_.GetRows().data()) , meta_.Size() * sizeof(size_t));
    fout_.write(reinterpret_cast<const char*>(&meta_start_offset) , sizeof(meta_start_offset));
}