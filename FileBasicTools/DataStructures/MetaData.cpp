#include "MetaData.h"
#include <stdexcept>

void MetaData::AddOffset(size_t offset) {
    offsets_.push_back(offset);
}

void MetaData::AddRows(size_t count) {
    rows_.push_back(count);
}

void MetaData::AddCodec(size_t codec) {
    codec_.push_back(codec);
}

const std::vector<size_t>& MetaData::GetOffsets() const {
    return offsets_;
}

const std::vector<size_t>& MetaData::GetRows() const {
    return rows_;
}

size_t MetaData::GetOffset(size_t index) const {
    if (index >= offsets_.size()) {
        throw std::runtime_error("Offset index is out of range!");
    }
    return offsets_[index];
}

size_t MetaData::GetRow(size_t index) const {
    if (index >= rows_.size()) {
        throw std::runtime_error("Metadata row index is out of range!");
    }
    return rows_[index];
}

size_t MetaData::Size() const {
    return offsets_.size();
}

void MetaData::Clear() {
    offsets_.clear();
    codec_.clear();
}

const Scheme& MetaData::GetScheme() const {
    return scheme_;
}