#include "MetaData.h"
#include <stdexcept>

void MetaData::AddBatchOffset(size_t offset) {
    batch_offsets_.push_back(offset);
}

void MetaData::AddColumnOffset(size_t offset) {
    column_offsets_.push_back(offset);
}

void MetaData::AddRows(size_t count) {
    rows_.push_back(count);
}

void MetaData::AddCodec(size_t codec) {
    codec_.push_back(codec);
}

const std::vector<size_t>& MetaData::GetBatchOffsets() const {
    return batch_offsets_;
}

const std::vector<size_t>& MetaData::GetColumnOffsets() const {
    return column_offsets_;
}

const std::vector<size_t>& MetaData::GetRows() const {
    return rows_;
}

size_t MetaData::GetBatchOffset(size_t index) const {
    if (index >= batch_offsets_.size()) {
        throw std::runtime_error("Batch offset index is out of range!");
    }
    return batch_offsets_[index];
}

size_t MetaData::GetColumnOffset(size_t batch_index, size_t index) const {
    if (batch_index >= batch_offsets_.size()) {
        throw std::runtime_error("Batch index is out of range, can't take Column offset!");
    }
    if (index < scheme_.Size() && batch_index * scheme_.Size() + index >= column_offsets_.size()) {
        throw std::runtime_error("Column offset index is out of range!");
    }
    return column_offsets_[batch_index * scheme_.Size() + index];
}

size_t MetaData::GetRow(size_t index) const {
    if (index >= rows_.size()) {
        throw std::runtime_error("Metadata row index is out of range!");
    }
    return rows_[index];
}

size_t MetaData::BatchesCount() const {
    return batch_offsets_.size();
}

size_t MetaData::ColumnsCount() const {
    return column_offsets_.size();
}

void MetaData::Clear() {
    batch_offsets_.clear();
    codec_.clear();
    rows_.clear();
    scheme_.Clear();
    column_offsets_.clear();
}

const Scheme& MetaData::GetScheme() const {
    return scheme_;
}
