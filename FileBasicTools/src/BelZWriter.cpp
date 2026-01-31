#include "BelZWriter.h"
#include "Scheme.h"
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
    int64_t val = std::stoll(data);
    fout_.write(reinterpret_cast<char*>(&val) , sizeof(val));
}

void BelZWriter::WriteString(const std::string& data) {
    size_t len = data.size();
    fout_.write(reinterpret_cast<char*>(&len) , sizeof(len));
    fout_.write(data.data(), len);
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
