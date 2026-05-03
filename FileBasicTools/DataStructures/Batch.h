#pragma once
#include "Scheme.h"
#include "Column.h"
#include "Row.h"
#include <vector>
#include <memory>

class Batch {
public:
    Batch() = default;
    Batch(const StringBacket& chunk , const Scheme& scheme) : scheme_(scheme) {
        ChunkToBatch(chunk);
    }
    void Init(const Scheme& scheme);
    void Init();
    void AddColumn(std::shared_ptr<Column> column);
    void AddRowFromCSV(const StringBacket& val);
    void SetScheme(const Scheme& scheme);
    ColumnType GetType(size_t index) const;
    size_t GetRows() const;
    std::shared_ptr<Column> GetColumn(size_t index) const;
    size_t Size() const;
    bool Empty() const;
    void Clear();
    void Reset();
private:
    void ChunkToBatch(const StringBacket& chunk);
    Scheme scheme_;
    std::vector <std::shared_ptr<Column>> data_;
    size_t rows_ = 0;
};