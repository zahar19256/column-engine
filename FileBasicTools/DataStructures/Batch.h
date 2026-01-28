#pragma once
#include "Scheme.h"
#include "Column.h"
#include "Row.h"
#include <vector>
#include <memory>

class Batch {
public:
    Batch() = default;
    Batch(const std::vector<Row<std::string>>& chunk , const Scheme& scheme) : scheme_(scheme) {
        ChunkToBatch(chunk);
    }
    void AddColumn(std::shared_ptr<Column> column);
    void SetScheme(const Scheme& scheme);
    ColumnType GetType(size_t index) const;
    std::shared_ptr<Column> GetColumn(size_t index) const;
    size_t Size() const;
    bool Empty() const;
    void Clear();
private:
    void ChunkToBatch(const std::vector<Row<std::string>>& chunk);
    Scheme scheme_;
    std::vector <std::shared_ptr<Column>> data_;
};