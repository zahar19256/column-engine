#include "Batch.h"
#include "Column.h"
#include "Scheme.h"
#include <memory>
#include <stdexcept>
#include <string>

void Batch::ChunkToBatch(const std::vector<Row<std::string>>& chunk) {
    if (chunk.empty()) {
        return;
    }
    for (size_t column = 0; column < chunk[0].Size(); ++column) {
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
        for (size_t row = 0; row < chunk.size(); ++row) {
            storage->AppendFromString(chunk[row][column]);
        }
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