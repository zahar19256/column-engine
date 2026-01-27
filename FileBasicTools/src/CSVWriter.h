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
    void WriteData(const T& data , ColumnType type);
    void WriteBatch(const Batch& batch);
private:
    std::ofstream fout_;
};