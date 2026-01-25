#include "CSVConvertor.h"
#include "Scheme.h"
#include <filesystem>
#include <fstream>
#include <iostream>

void CSVConvertor::GetScheme(const std::string& SchemeFilePath) {
    CSVReader tmp_scan(SchemeFilePath);
    std::vector<std::vector<std::string>> table = tmp_scan.ReadFullTable();
    for (auto row : table) {
        if (row.size() != 2) {
            throw std::runtime_error("Scheme table has more than two fields in one row!");
        }
        scheme_.Push_Back(SchemeNode(row[0] , DatumConvertor(row[1])));
    }
}

bool CSVConvertor::GetChunk(CSVReader& scan_) {
    chunk_ = scan_.ReadChunk();
    if (chunk_.empty()) {
        return false;
    }
    return true;
}

void CSVConvertor::MakeBelZFormat(const std::string& CSVFilePath, const std::string& SchemeFilePath) {
    CSVReader scan_(CSVFilePath);
    GetScheme(SchemeFilePath);
    std::filesystem::path src_path(CSVFilePath);
    std::filesystem::path dest_path = src_path;
    dest_path.replace_extension(".belZ");
    if (std::filesystem::exists(dest_path)) {
        std::cerr << "File " << dest_path << " already exists. Skip!!!" << std::endl;
        return;
    }
    std::ofstream fout(dest_path , std::ios::binary | std::ios::out);
    if (!fout.is_open()) {
        throw std::runtime_error("Failed to create file: " + dest_path.string());
    }
    size_t col_count = scheme_.Size();
    while (GetChunk(scan_)) {
        size_t rows_in_chunk = chunk_.size();
        meta_.AddOffset(fout.tellp());
        meta_.AddRows(rows_in_chunk);
        meta_.AddCodec(0);
        for (size_t column_idx = 0; column_idx < col_count; ++column_idx) {
            for (size_t row_idx = 0; row_idx < rows_in_chunk; ++row_idx) {
                std::string raw_value = chunk_[row_idx][column_idx];
                ColumnType type = scheme_.GetType(column_idx);
                // TODO Переписать это на что-то вида ReadColumn в BelZReader
                if (type == ColumnType::Int64) {
                    try {
                        int64_t val = std::stoll(raw_value);
                        fout.write(reinterpret_cast<const char*>(&val) , sizeof(val));
                    } catch (...) {
                        int64_t val = 0; 
                        fout.write(reinterpret_cast<const char*>(&val) , sizeof(val));
                    }
                } 
                else {
                    if (type == ColumnType::String) {
                        size_t len = raw_value.size();
                        fout.write(reinterpret_cast<const char*>(&len) , sizeof(len));
                        fout.write(raw_value.data(), len);
                    }
                }
            }
        }
    }
    uint64_t meta_start_offset = static_cast<uint64_t>(fout.tellp());
    size_t batches_count = meta_.Size();
    fout.write(reinterpret_cast<const char*>(&col_count) , sizeof(col_count));
    fout.write(reinterpret_cast<const char*>(scheme_.GetScheme().data()) , col_count * sizeof(SchemeNode));
    fout.write(reinterpret_cast<const char*>(&batches_count) , sizeof(batches_count));
    fout.write(reinterpret_cast<const char*>(meta_.GetOffsets().data()) , meta_.Size() * sizeof(size_t));
    fout.write(reinterpret_cast<const char*>(meta_.GetRows().data()) , meta_.Size() * sizeof(size_t));
    fout.write(reinterpret_cast<const char*>(&meta_start_offset) , sizeof(meta_start_offset));
    fout.close();
}