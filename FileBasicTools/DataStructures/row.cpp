#include "row.h"

template <typename T>
void Row<T>::Add(const T& value) {
    data_.push_back(value);
}

template <typename T>
void Row<T>::Add(T&& value) {
    data_.push_back(std::move(value));
}

template <typename T>
size_t Row<T>::Size() const {
    return data_.size();
}

template <typename T>
bool Row<T>::Empty() const {
    return data_.empty();
}

template <typename T>
void Row<T>::Clear() {
    data_.clear();
}

template <typename T>
std::vector<T> Row<T>::GetData() {
    return data_;
}

template <typename T>
std::vector<T> Row<T>::ExtractData() {
    return std::move(data_);
}