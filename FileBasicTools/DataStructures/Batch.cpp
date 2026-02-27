#include "Batch.h"
#include "Column.h"
#include "Row.h"
#include "Scheme.h"
#include <memory>
#include <stdexcept>
#include <string>

void Batch::ChunkToBatch(const StringBacket& chunk) {
    if (chunk.Empty()) {
        return;
    }
    for (size_t column = 0; column < scheme_.Size(); ++column) {
        std::shared_ptr<Column> storage;
        if (scheme_.GetType(column) == ColumnType::Int64) {
            storage = std::make_shared<Int64Column>();
        }
        if (scheme_.GetType(column) == ColumnType::String) {
            storage = std::make_shared<StringColumn>();
        }
        if (scheme_.GetType(column) == ColumnType::Unknown) {
            throw std::runtime_error("Unknown column " + std::to_string(column) + " type!");
        }
        for (size_t row = 0; row < chunk.GetRows(); ++row) {
            storage->AppendFromString(chunk.GetString(row , column) , chunk.GetSize(row , column));
        }
        AddColumn(storage);
    }
}

void Batch::AddColumn(std::shared_ptr<Column> column) {
    data_.push_back(column);
}

void Batch::SetScheme(const Scheme& scheme) {
    scheme_ = scheme;
}

std::shared_ptr<Column> Batch::GetColumn(size_t index) const {
    if (index >= data_.size()) {
        throw std::runtime_error("Index of column is out of batch range");
    }
    return data_[index];
}

size_t Batch::Size() const {
    return data_.size();
}

bool Batch::Empty() const {
    return data_.empty();
}

void Batch::Clear() {
    scheme_.Clear();
    data_.clear();
}

ColumnType Batch::GetType(size_t index) const {
    return scheme_.GetType(index);
}