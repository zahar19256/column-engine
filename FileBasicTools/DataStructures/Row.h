#pragma once
#include <cstring>
#include <error.h>
#include <memory.h>
#include <stdexcept>
#include <string>
#include <vector>
class StringBacket {
public:
    void SetOffsets(const std::vector<size_t>& source_offset) {
        offset_.reserve(offset_.size() + source_offset.size());
        offset_.insert(offset_.end(), source_offset.begin(), source_offset.end());
    }
    void Reserve(size_t size) {
        data_.resize(size);
    }
    void Push_Back(std::string val, size_t offset) {
        size_t size = val.size();
        const char* start = val.data();
        size_t new_size = data_.size();
        if (new_size < offset + size) {
            if (new_size == 0) {
                new_size = 4096;
            }
            while (offset + size > new_size) {
                new_size <<= 1;
            }
            data_.resize(new_size);
        }
        memcpy(data_.data() + offset, start, size);
    }
    void AppendString(const char* start, size_t size, size_t offset) {
        size_t new_size = data_.size();
        if (new_size < offset + size) {
            if (new_size == 0) {
                new_size = 4096;
            }
            while (offset + size > new_size) {
                new_size <<= 1;
            }
            data_.resize(new_size);
        }
        memcpy(data_.data() + offset, start, size);
    }
    void Add(std::string val) {
        data_.append(val);
        offset_.push_back(data_.size());
    }
    char* GetData() {
        return data_.data();
    }
    const char* GetData() const {
        return data_.data();
    }
    bool Empty() const {
        return offset_.empty();
    }
    void Clear() {
        offset_.clear();
        columns_.clear();
        rows_ = 0;
    }
    size_t LastOffset() const {
        if (offset_.empty()) {
            return 0;
        }
        return offset_.back();
    }
    void PushOffset(size_t size) {
        offset_.push_back(size);
    }
    size_t GetSize(size_t row_index, size_t column_index) const {
        if (row_index >= rows_) {
            throw std::runtime_error("Index is out of StringTable range");
        }
        size_t start = 0;
        size_t index = 0;
        if (row_index > 0) {
            index += columns_[row_index - 1];
        }
        index += column_index;
        if (index > 0) {
            start = offset_[index - 1];
        }
        return offset_[index] - start;
    }
    const char* GetString(size_t row_index, size_t column_index) const {
        if (row_index >= rows_) {
            throw std::runtime_error("Index is out of StringTable range");
        }
        size_t start = 0;
        size_t index = 0;
        if (row_index > 0) {
            index += columns_[row_index - 1];
        }
        index += column_index;
        if (index > offset_.size()) {
            throw std::runtime_error("FUCK THIS STRING");
        }
        if (index > 0) {
            start = offset_[index - 1];
        }
        return data_.data() + start;
    }
    std::string operator[](size_t index) const {
        if (index >= offset_.size()) {
            throw std::runtime_error("Index is out of StringRow range");
        }
        size_t start = 0;
        if (index > 0) {
            start = offset_[index - 1];
        }
        return data_.substr(start, offset_[index] - start);
    }
    size_t Size() const {
        return offset_.size();
    }
    size_t GetRows() const {
        return rows_;
    }
    void EndRow() {
        columns_.push_back(offset_.size());
        ++rows_;
    }
    std::string data_;

private:
    std::vector<size_t> offset_;
    std::vector<size_t> columns_;
    size_t rows_ = 0;
};

using Storage = StringBacket;
