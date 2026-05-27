#pragma once
#include "Scheme.h"
#include "Column.h"
#include <fstream>
#include <cstdint>
#include <vector>

class BelZWriter {
public:
    BelZWriter(const std::string& CSVFilePath);
    void EnsureCapacity(size_t add_size);
    uint64_t GetOffSet();
    void WriteScheme(const Scheme& scheme);
    void Append(const char* data , size_t size , ColumnType type);
    template <typename T>
    inline void AppendNumber(const char* data , size_t size) {
        T val = 0;
        std::from_chars(data , data + size , val);
        EnsureCapacity(sizeof(val));
        memcpy(buf_.data() + offset_ , reinterpret_cast<char*>(&val) , sizeof(val));
        offset_ += sizeof(val);
    }


    inline void AppendString(const char* data , size_t size);
    void AppendColumn(std::shared_ptr<Column> column , ColumnType type);
    void Flush() {
        fout_.write(buf_.data() , offset_);
        offset_ = 0;
    }

    template <typename T>
    void WriteMeta(T&& meta_) {
        if (offset_ != 0) {
            Flush();
        }
        uint64_t meta_start_offset = static_cast<uint64_t>(fout_.tellp());
        uint64_t batches_count = meta_.BatchesCount();
        uint64_t col_count = meta_.GetScheme().Size();
        fout_.write(reinterpret_cast<const char*>(&col_count) , sizeof(col_count));
        WriteScheme(meta_.GetScheme());
        fout_.write(reinterpret_cast<const char*>(&batches_count) , sizeof(batches_count));
        fout_.write(reinterpret_cast<const char*>(meta_.GetBatchOffsets().data()) , batches_count * sizeof(size_t));
        fout_.write(reinterpret_cast<const char*>(meta_.GetRows().data()) , batches_count * sizeof(size_t));
        fout_.write(reinterpret_cast<const char*>(meta_.GetColumnOffsets().data()) , col_count * batches_count * sizeof(size_t));
        fout_.write(reinterpret_cast<const char*>(&meta_start_offset) , sizeof(meta_start_offset));
    }
private:
    std::ofstream fout_;
    std::string buf_;
    size_t offset_ = 0;
};
