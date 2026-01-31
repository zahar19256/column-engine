#pragma once
#include <vector>

template <typename T>
class Row {
public:
    Row() = default;
    Row(const Row& other) = default;
    Row(Row&& other) = default;
    Row& operator=(const Row& other) = default;
    Row& operator=(Row&& other) = default;
    ~Row() = default;

    void Add(const T& value) {
        data_.push_back(value);
    }
    void Add(T&& value) {
        data_.push_back(std::move(value));
    }
    size_t Size() const {
        return data_.size();
    }
    bool Empty() const {
        return data_.empty();
    }
    void Clear() {
        data_.clear();
    }
    std::vector<T> ExtractData() {
        return std::move(data_);
    }
    const std::vector<T> GetData() const {
        return data_;
    }
    const T& operator[](size_t index) const {
        return data_[index];
    }
    T& operator[](size_t index) {
        return data_[index];
    }
private:
    std::vector<T> data_;
};