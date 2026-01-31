#pragma once
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

    template <typename T>
    void WriteMeta(T&& meta_) {
        uint64_t meta_start_offset = static_cast<uint64_t>(fout_.tellp());
        uint64_t batches_count = meta_.Size();
        uint64_t col_count = meta_.GetScheme().Size();
        fout_.write(reinterpret_cast<const char*>(&col_count) , sizeof(col_count));
        WriteScheme(meta_.GetScheme());
        fout_.write(reinterpret_cast<const char*>(&batches_count) , sizeof(batches_count));
        fout_.write(reinterpret_cast<const char*>(meta_.GetOffsets().data()) , meta_.Size() * sizeof(size_t));
        fout_.write(reinterpret_cast<const char*>(meta_.GetRows().data()) , meta_.Size() * sizeof(size_t));
        fout_.write(reinterpret_cast<const char*>(&meta_start_offset) , sizeof(meta_start_offset));
    }
private:
    std::ofstream fout_;
};