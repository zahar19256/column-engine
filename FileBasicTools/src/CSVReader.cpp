#include "CSVReader.h"
#include <cstdint>
#include <error.h>
#include <stdexcept>
#include <filesystem>
#include <string>

CSVReader::CSVReader(const std::string& filePath , ssize_t bucket_size) : filePath_(filePath) , bucket_(bucket_size) {
    if (!FileExists(filePath_)) {
        throw std::invalid_argument("File not found: " + filePath_);
    }
    stream_ = std::ifstream(filePath_ , std::ios::binary);
    if (!stream_.is_open() || !stream_) {
        throw std::runtime_error("Failed to open CSV for reading: " + filePath);
    }
}

void CSVReader::BOMHelper() {
    uint8_t b0 = stream_.peek();
    if (b0 == 0xEF) {
        char bom[3];
        stream_.read(bom, 3);
        if (!(static_cast<unsigned char>(bom[0]) == 0xEF && static_cast<unsigned char>(bom[1]) == 0xBB && static_cast<unsigned char>(bom[2]) == 0xBF)) {
            stream_.clear();
            stream_.seekg(0, std::ios::beg);
        }
    }
}

std::vector<uint8_t> CSVReader::ReadFileData() {
    std::vector<uint8_t> data;
    char byte;
    while (stream_.get(byte)) {
        data.push_back(byte);
    }
    return data;
}

std::vector<std::vector<std::string>> CSVReader::ReadFullTable(char delimiter) {
    BOMHelper();
    std::vector<std::vector<std::string>> table;
    Row<std::string> row;
    while (true) {
        size_t bytes = 0;
        ReadRow(row , bytes);
        if (bytes == 0 || row.Empty()) {
            break;
        }
        table.push_back(row.ExtractData());
    }
    return table;
}

std::vector<Row<std::string>> CSVReader::ReadChunk(char delimiter) {
    if (initial_chunk_) {
        BOMHelper();
        initial_chunk_ = false;
    }
    std::vector<Row<std::string>> table;
    Row<std::string> row;
    size_t current_size = 0;
    while (current_size < bucket_) {
        ReadRow(row , current_size);
        if (row.Empty()) {
            break;
        }
        table.push_back(std::move(row));
    }
    return table;
}

bool FileExists(const std::string& filePath) {
    return std::filesystem::exists(filePath);
}

void WriteBytesInFile(const std::string& destPath, const std::vector<uint8_t>& data) {
    if (!FileExists(destPath)) {
        throw std::invalid_argument("Wrong destination path, can't find file!");
    }
    std::ofstream fout(destPath, std::ios::binary | std::ios::app);
    fout.write(reinterpret_cast<const char*>(data.data()) , data.size());
}