#include "BaseReader.h"
#include <filesystem>

void BaseReader::ReadRowCSV(Row<std::string>& data , size_t& bytes , char delimiter) {
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

void BaseReader::ReadBytes(std::vector<uint8_t>& data , size_t size) {
    stream_.read(reinterpret_cast<char*>(data.data()) , size);
}

bool FileExists(const std::string& filePath) {
    return std::filesystem::exists(filePath);
}