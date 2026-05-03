#pragma once
#include "Batch.h"
#include "MetaData.h"
#include "Scheme.h"
#include <fstream>
#include <memory>
#include <vector>

class BelZReader {
public:
    BelZReader(const std::string& filePath);
    void ReadBatch(Batch& batch);
    void ScanBatch(size_t index , Batch& batch);
    std::shared_ptr<Column> ReadColumn(size_t size , ColumnType type , ssize_t need_offset = -1);
    std::shared_ptr<Column> ReadInt64Column(size_t size);
    std::shared_ptr<Column> ReadStringColumn(size_t size);
    void ReadMetaData();
    MetaData GetMetaData() const;
    Scheme GetScheme() const;
    std::shared_ptr<Column> ReadColumn(size_t batch_id , size_t column_id);
    bool Empty() const;
private:
    static constexpr size_t kStreamBufferSize = 512 * 1024 * 1024;
    MetaData meta_;
    Scheme scheme_;
    size_t batches_left_ = 0;
    size_t index_of_batch = 0;
    std::vector<char> stream_buffer_ = std::vector<char>(kStreamBufferSize);
    std::ifstream stream_;
};