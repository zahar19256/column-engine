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

    void Add(const T& value);
    void Add(T&& value);
    size_t Size() const;
    bool Empty() const;
    void Clear();
    std::vector<T> ExtractData();
    std::vector<T> GetData();
private:
    std::vector<T> data_;
};