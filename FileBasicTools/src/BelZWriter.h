#pragma once
#include "MetaData.h"
#include "Scheme.h"
#include <fstream>
#include <cstdint>

class BelZWriter {
public:
    BelZWriter(const std::string& CSVFilePath);
    uint64_t GetOffSet();
    void WriteData(const std::string& data , ColumnType type);
    void WriteInt64(const std::string& data);
    void WriteString(const std::string& data);
    void WriteScheme(const Scheme& scheme);
    void WriteMeta(const MetaData& meta);
private:
    std::ofstream fout_;
};