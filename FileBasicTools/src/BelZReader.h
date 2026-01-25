#pragma once
#include "BaseReader.h"
#include "Batch.h"
#include "MetaData.h"
#include "Scheme.h"
#include <fstream>

class BelZReader : public BaseReader{
public:
    BelZReader(const std::string& filePath);
    void ReadBatch(Batch& batch);
    bool Empty() const;
private:
    void ReadMetaData();
    std::shared_ptr<Column> ReadColumn(size_t size , ColumnType type);
    std::shared_ptr<Column> ReadInt64Column(size_t size);
    std::shared_ptr<Column> ReadStringColumn(size_t size);
    MetaData meta_;
    Scheme scheme_;
    size_t batches_left_ = 0;
    size_t index_of_batch = 0;
};