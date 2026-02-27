#include "CSVReader.h"
#include "Row.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <iostream>

CSVReader::CSVReader(const std::string& filePath , size_t bucket_size) : filePath_(filePath) , bucket_size_(bucket_size) {
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

void CSVReader::ReadRowCSV(StringBacket& data, size_t& bytes, char delimiter) {
    CURSOR_STATE state = CURSOR_STATE::NOT_IN_QUOTE;
    size_t current_len = data.data_.size();
    std::string field;
    while (true) {
        if (!std::getline(stream_, field)) {
            break;
        }
        bool has_delim = !stream_.eof();
        bytes += field.size();
        if (has_delim) {
            ++bytes;
        }
        const char* buf = field.data();
        size_t n = field.size();
        size_t offset = 0;
        for (size_t i = 0; i < n; ++i) {
            char cur = buf[i];
            if (state == CURSOR_STATE::IN_QUOTE) {
                if (cur == '"') {
                    if (i + 1 < n && buf[i + 1] == '"') {
                        if (i > offset) {
                            data.data_.append(buf + offset, i - offset);
                            current_len += i - offset;
                        }
                        data.data_.push_back('"');
                        ++current_len;
                        ++i;
                        offset = i + 1;
                    } else {
                        if (i > offset) {
                            data.data_.append(buf + offset, i - offset);
                            current_len += i - offset;
                        }
                        offset = i + 1;
                        state = CURSOR_STATE::NOT_IN_QUOTE;
                    }
                }
            } else {
                if (cur == delimiter) {
                    if (i > offset) {
                        data.data_.append(buf + offset, i - offset);
                        current_len += i - offset;
                    }
                    data.PushOffset(current_len);
                    offset = i + 1;
                } else {
                    if (cur == '"') {
                        if (i > offset) {
                            data.data_.append(buf + offset, i - offset);
                            current_len += i - offset;
                        }
                        offset = i + 1;
                        state = CURSOR_STATE::IN_QUOTE;
                    }
                }
            }
        }
        if (n > offset) {
            data.data_.append(buf + offset, n - offset);
            current_len += n - offset;
        }
        if (state == CURSOR_STATE::IN_QUOTE) {
            if (has_delim) {
                data.data_.push_back('\n');
                ++current_len;
            }
            continue;
        } else {
            if (has_delim) {
                data.PushOffset(current_len);
                break;
            } else {
                break;
            }
        }
    }
    size_t prev = data.LastOffset();
    if (current_len != prev) {
        data.PushOffset(current_len);
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

std::vector<StringBacket> CSVReader::ReadFullTable(char delimiter) {
    BOMHelper();
    StringBacket row;
    std::vector<StringBacket> table;
    while (true) {
        size_t bytes = 0;
        ReadRowCSV(row , bytes , delimiter);
        if (bytes == 0) {
            break;
        }
        table.push_back(std::move(row));
    }
    return table;
}
