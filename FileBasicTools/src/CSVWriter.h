#pragma once
#include "Batch.h"
#include <fstream>

class CSVWriter {
public:
    CSVWriter(const std::string& filePath);
    void WriteDelimetr(char delimetr = ',');
    void WriteInt64(int64_t data);
    void WriteString(const std::string& data);
    template <typename T>
    void WriteData(const T& data , ColumnType type) {
        if (type == ColumnType::Int64) {
            WriteInt64(data);
            return;
        }
        if (type == ColumnType::String) {
            WriteString(data);
            return;
        }
        throw std::runtime_error("Try to write unknown type"+ std::to_string(static_cast<uint8_t>(type)));
    }

    void WriteBatch(const Batch& batch);
private:
    std::ofstream fout_;
};