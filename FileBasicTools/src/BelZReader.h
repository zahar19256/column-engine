#pragma once
#include "BaseReader.h"
#include "Batch.h"
#include "MetaData.h"
#include "Scheme.h"

class BelZReader : public BaseReader{
public:
    BelZReader(const std::string& filePath);
    void ReadBatch(Batch& batch);
    std::shared_ptr<Column> ReadColumn(size_t size , ColumnType type);
    std::shared_ptr<Column> ReadInt64Column(size_t size);
    std::shared_ptr<Column> ReadStringColumn(size_t size);
    void ReadMetaData();
    bool Empty() const;
private:
    MetaData meta_;
    Scheme scheme_;
    size_t batches_left_ = 0;
    size_t index_of_batch = 0;
};