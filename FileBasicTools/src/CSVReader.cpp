#include "CSVReader.h"
#include <cstdint>
#include <error.h>
#include <stdexcept>
#include <string>

CSVReader::CSVReader(const std::string& filePath , ssize_t bucket_size) : filePath_(filePath) , bucket_(bucket_size) {
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

void CSVReader::ReadRowCSV(Row<std::string>& data , size_t& bytes , char delimiter) {
    std::string current_field = "";
    enum class CURSOR_STATE {
        IN_QOUTE,
        NOT_IN_QOUTE,
    };
    CURSOR_STATE state = CURSOR_STATE::NOT_IN_QOUTE;
    char current;
    while (stream_.get(current)) {
        ++bytes;
        if (state == CURSOR_STATE::IN_QOUTE) {
            if (current == '"') {
                if (stream_.peek() == '"') {
                    current_field += current;
                    stream_.get();
                    ++bytes;
                } else {
                    state = CURSOR_STATE::NOT_IN_QOUTE;
                }
            } else {
                current_field += current;
            }
            continue;
        }
        if (current == delimiter) {
            data.Add(current_field);
            current_field.clear();
            continue;
        }
        if (current == '\r') {
            if (stream_.peek() == '\n') {
                stream_.get();
                data.Add(current_field);
                current_field.clear();
                ++bytes;
                break;
            }
        }
        if (current == '\n') {
            data.Add(current_field);
            current_field.clear();
            break;
        }
        if (current == '"') {
            state = CURSOR_STATE::IN_QOUTE;
            continue;
        }
        current_field += current;
    }
    if (!current_field.empty()) {
        data.Add(current_field);
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
        ReadRowCSV(row , bytes , delimiter);
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
        ReadRowCSV(row , current_size , delimiter);
        if (row.Empty()) {
            break;
        }
        table.push_back(std::move(row));
    }
    return table;
}
