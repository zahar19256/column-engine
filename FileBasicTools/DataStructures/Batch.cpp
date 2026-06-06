#include "Batch.h"
#include "Column.h"
#include "Row.h"
#include "Scheme.h"
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <memory>
#include <stdexcept>
#include <string>

void Batch::ChunkToBatch(const StringBacket& chunk) {
    if (chunk.Empty()) {
        return;
    }
    for (size_t column = 0; column < scheme_.Size(); ++column) {
        std::shared_ptr<Column> storage = MakeColumn(scheme_.GetType(column) , &string_arena_);
        if (scheme_.GetType(column) == ColumnType::Unknown) {
            throw std::runtime_error("Unknown column " + std::to_string(column) + " type!");
        }
        for (size_t row = 0; row < chunk.GetRows(); ++row) {
            storage->AppendFromString(chunk.GetString(row , column) , chunk.GetSize(row , column));
        }
        AddColumn(storage);
        rows_ = chunk.GetRows();
    }
}

void Batch::Init() {
    rows_ = 0;
    for (size_t i = 0; i < scheme_.Size(); ++i) {
        std::shared_ptr<Column> storage = MakeColumn(scheme_.GetType(i) , &string_arena_);
        if (scheme_.GetType(i) == ColumnType::Unknown) {
            throw std::runtime_error("Unknown column " + std::to_string(i) + " type!");
        }
        AddColumn(storage);
    }
}

void Batch::Init(const Scheme& scheme , bool convert) {
    scheme_ = scheme;
    rows_ = 0;
    for (size_t i = 0; i < scheme_.Size(); ++i) {
        ColumnType type = scheme_.GetType(i);
        if (convert && type == ColumnType::String) {
            type = ColumnType::FlatString;
        }
        std::shared_ptr<Column> storage = MakeColumn(type , &string_arena_);
        if (scheme_.GetType(i) == ColumnType::Unknown) {
            throw std::runtime_error("Unknown column " + std::to_string(i) + " type!");
        }
        AddColumn(storage);
    }
}

void Batch::AddColumn(std::shared_ptr<Column> column) {
    data_.push_back(column);
    rows_ = column->Size();
}

void Batch::AddRowFromCSV(const StringBacket& val) {
    for (size_t i = 0; i < scheme_.Size(); ++i) {
        data_[i]->AppendFromString(val.GetString(0 , i), val.GetSize(0 , i));
    }
    ++rows_;
}

void Batch::SetScheme(const Scheme& scheme) {
    scheme_ = scheme;
}

void Batch::SetRows(size_t rows) {
    rows_ = rows;
}

void Batch::InitMsk() {
    mask_.resize(GetRows() , true);
}

void Batch::SetMsk(const boost::dynamic_bitset<>& mask) {
    mask_ = mask;
}

void Batch::SetMsk(boost::dynamic_bitset<>&& msk) {
    mask_ = std::move(msk);
}

void Batch::AddAlias(const std::string& current_name , const std::string& alias) {
    scheme_.AddAlias(current_name, alias);
}

void Batch::ApplyMsk(const boost::dynamic_bitset<>& mask) {
    if (!has_mask_) {
        mask_ = mask;
        has_mask_ = true;
    } else {
        mask_ &= mask;
    }
}

void Batch::MergeBatches(Batch& result , Batch& new_data) {

}

std::shared_ptr<Column> Batch::GetColumn(size_t index) const {
    if (index >= data_.size()) {
        throw std::runtime_error("Index of column is out of batch range");
    }
    return data_[index];
}

std::shared_ptr<Column> Batch::GetColumn(const std::string& column_name) const {
    size_t index = scheme_.GetIndex(column_name);
    if (index >= Size()) {
        throw std::runtime_error("Column id is bigger than batch size!");
    }
    return GetColumn(index); 
}

const boost::dynamic_bitset<>& Batch::GetMsk() const {
    return mask_;
}

Utility::StringArena* Batch::GetStringArena() {
    return &string_arena_;
}

const Scheme& Batch::GetScheme() const {
    return scheme_;
}

size_t Batch::Size() const {
    return data_.size();
}

bool Batch::Empty() const {
    return data_.empty();
}

void Batch::Clear() {
    data_.clear();
    string_arena_.Clear();
    scheme_.Clear();
    mask_.clear();
    has_mask_ = false;
    rows_ = 0;
}

void Batch::Reset() {
    for (size_t i = 0; i < scheme_.Size(); ++i) {
        data_[i]->Clear();
    }
    string_arena_.Clear();
    rows_ = 0;
}

size_t Batch::GetRows() const {
    return rows_;
}

ColumnType Batch::GetType(size_t index) const {
    return scheme_.GetType(index);
}
