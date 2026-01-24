#include "MetaData.h"

void MetaData::AddBytes(std::vector<uint8_t> bytes) {
    if (bytes.empty()) {
        return;
    }
    if (data_.empty()) {
        data_.swap(bytes);
        return;
    }
    data_.insert(data_.end(), bytes.begin(), bytes.end());
}

void MetaData::AddBytes(const uint8_t* ptr, size_t size) {
    if (!ptr || size == 0) {
        return;
    }
    data_.insert(data_.end(), ptr, ptr + size);
}

const std::vector<uint8_t>& MetaData::GetData() const {
    return data_;
}

size_t MetaData::Size() const {
    return data_.size();
}

void MetaData::Clear() {
    data_.clear();
}