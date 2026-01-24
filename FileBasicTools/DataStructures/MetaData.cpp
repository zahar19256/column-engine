#include "MetaData.h"

void MetaData::AddOffset(size_t offset) {
    offsets_.push_back(offset);
}

void MetaData::AddRows(size_t count) {
    rows_.push_back(count);
}

void MetaData::AddCodec(size_t codec) {
    codec_.push_back(codec);
}

const std::vector<size_t>& MetaData::GetOffests() const {
    return offsets_;
}

const std::vector<size_t>& MetaData::GetRows() const {
    return rows_;
}

size_t MetaData::Size() const {
    return offsets_.size();
}

void MetaData::Clear() {
    offsets_.clear();
    codec_.clear();
}