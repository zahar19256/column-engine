#pragma once
#include <vector>
#include <string>
#include <memory.h>
#include <stdexcept>
#include <error.h>
#include <iostream>
class StringBacket {
public:
    void SetOffsets(std::vector<size_t>& source_offset) {
        offset_.reserve(offset_.size() + source_offset.size());
        offset_.insert(offset_.end() , source_offset.begin() , source_offset.end());
    }
    void AppendString(const char* start , size_t size) {
        int now = 0;
        if (!offset_.empty()) {
            now = offset_.back();
        }
        if (data_.capacity() - now >= size) {
            
        }
        throw std::runtime_error("NOT IMPLEMETED Append");
    }
    void Add(std::string val) {
        data_.append(val);
        offset_.push_back(data_.size());
    }
    char* GetData() {
        return data_.data();
    }
    bool Empty() const {
        return offset_.empty();
    }
    void Clear() {
        data_.clear();
        offset_.clear();
        columns_.clear();
        rows_ = 0;
    }
    size_t LastOffset() {
        if (offset_.empty()) {
            return 0;
        }
        return offset_.back();
    }
    void PushOffset(size_t size) {
        offset_.push_back(size);
    }
    size_t GetSize(size_t row_index , size_t column_index) const {
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
    const char* GetString(size_t row_index , size_t column_index) const {
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
        return data_.substr(start , offset_[index] - start);
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
    std::vector <size_t> offset_;
    std::vector <size_t> columns_;
    size_t rows_ = 0;
};

using Storage = StringBacket;