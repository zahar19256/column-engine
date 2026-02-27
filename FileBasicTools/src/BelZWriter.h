#pragma once
#include "Scheme.h"
#include <fstream>
#include <cstdint>

class BelZWriter {
public:
    BelZWriter(const std::string& CSVFilePath);
    void EnsureCapacity(size_t add_size);
    uint64_t GetOffSet();
    void WriteData(const char* data , size_t size , ColumnType type);
    void WriteInt64(const char* data , size_t size);
    void WriteString(const char* data , size_t size);
    void WriteScheme(const Scheme& scheme);
    void Append(const char* data , size_t size , ColumnType type);
    void AppendInt64(const char* data , size_t size);
    void AppendString(const char* data , size_t size);
    void Flush() {
        fout_.write(buf_.data() , offset_);
        offset_ = 0;
    }

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
    std::string buf_;
    size_t offset_ = 0;
};