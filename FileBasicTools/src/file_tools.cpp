#include "file_tools.h"
#include <cstdint>
#include <error.h>
#include <stdexcept>

FileReader::FileReader(const std::string& filePath , ssize_t bucket_size) : filePath_(filePath) , bucket_(bucket_size) {
    if (!FileExists(filePath_)) {
        throw std::invalid_argument("File not found: " + filePath_);
    }
    stream_ = std::ifstream(filePath_ , std::ios::binary);
    if (!stream_.is_open() || !stream_) {
        throw std::runtime_error("Failed to open CSV for reading: " + filePath);
    }
}

std::vector<uint8_t> FileReader::ReadFileData() {
    std::vector<uint8_t> data;
    char byte;
    while (stream_.get(byte)) {
        data.push_back(byte);
    }
    return data;
}

std::vector<std::vector<std::string>> FileReader::ReadFullTable(char delimiter) {
    uint8_t b0 = stream_.peek();
    if (b0 == 0xEF) {
        char bom[3];
        stream_.read(bom, 3);
        if (!(static_cast<unsigned char>(bom[0]) == 0xEF && static_cast<unsigned char>(bom[1]) == 0xBB && static_cast<unsigned char>(bom[2]) == 0xBF)) {
            stream_.clear();
            stream_.seekg(0, std::ios::beg);
        }
    }
    std::vector<std::vector<std::string>> table;
    std::vector<std::string> current_row;
    std::string current_field;
    enum class CURSOR_STATE {
        IN_QOUTE,
        NOT_IN_QOUTE,
    };
    CURSOR_STATE state = CURSOR_STATE::NOT_IN_QOUTE;

    auto Reset = [&]() {
        current_field.clear();
        current_row.clear();
    };
    char current;
    while (stream_.get(current)) {
        if (state == CURSOR_STATE::IN_QOUTE) {
            if (current == '"') {
                if (stream_.peek() == '"') {
                    current_field += current;
                    stream_.get();
                } else {
                    state = CURSOR_STATE::NOT_IN_QOUTE;
                }
            } else {
                current_field += current;
            }
            continue;
        }
        if (current == delimiter) {
            current_row.push_back(current_field);
            current_field.clear();
            continue;
        }
        if (current == '\r') {
            if (stream_.peek() == '\n') {
                stream_.get();
                current_row.push_back(current_field);
                table.push_back(current_row);
                Reset();
                continue;
            }
        }
        if (current == '\n') {
            current_row.push_back(current_field);
            table.push_back(current_row);
            Reset();
            continue;
        }
        if (current == '"') {
            state = CURSOR_STATE::IN_QOUTE;
            continue;
        }
        current_field += current;
    }
    if (!current_row.empty()) {
        if (!current_field.empty()) {
            current_row.push_back(current_field);
        }
        table.push_back(current_row);
    }
    return table;
}

std::vector<std::vector<std::string>> FileReader::ReadChunk(char delimiter) {
    if (initial_chunk_ == true) {
        uint8_t b0 = stream_.peek();
        if (b0 == 0xEF) {
            char bom[3];
            stream_.read(bom, 3);
            if (!(static_cast<uint8_t>(bom[0]) == 0xEF && static_cast<uint8_t>(bom[1]) == 0xBB && static_cast<uint8_t>(bom[2]) == 0xBF)) {
                stream_.clear();
                stream_.seekg(0, std::ios::beg);
            }
        }
        initial_chunk_ = false;
    }
    std::vector<std::vector<std::string>> table;
    std::vector<std::string> current_row;
    std::string current_field;
    enum class CURSOR_STATE {
        IN_QOUTE,
        NOT_IN_QOUTE,
    };
    CURSOR_STATE state = CURSOR_STATE::NOT_IN_QOUTE;
    char current;
    size_t current_size = 0;

    auto Reset = [&]() {
        current_field.clear();
        current_row.clear();
    };
    bool filled = false;
    while (stream_.get(current) && !filled) {
        ++current_size;
        if (state == CURSOR_STATE::IN_QOUTE) {
            if (current == '"') {
                if (stream_.peek() == '"') {
                    current_field += current;
                    stream_.get();
                    ++current_size;
                } else {
                    state = CURSOR_STATE::NOT_IN_QOUTE;
                }
            } else {
                current_field += current;
            }
            continue;
        }
        if (current == delimiter) {
            current_row.push_back(std::move(current_field));
            current_field.clear();
            continue;
        }
        if (current == '\r') {
            if (stream_.peek() == '\n') {
                stream_.get();
                ++current_size;
                current_row.push_back(std::move(current_field));
                table.push_back(std::move(current_row));
                if (current_size > bucket_) {
                    filled = true;
                }
                Reset();
                continue;
            }
        }
        if (current == '\n') {
            current_row.push_back(std::move(current_field));
            table.push_back(std::move(current_row));
            if (current_size > bucket_) {
                filled = true;
            }
            Reset();
            continue;
        }
        if (current == '"') {
            state = CURSOR_STATE::IN_QOUTE;
            continue;
        }
        current_field += current;
    }
    if (!current_row.empty()) {
        if (!current_field.empty()) {
            current_row.push_back(std::move(current_field));
        }
        table.push_back(std::move(current_row));
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