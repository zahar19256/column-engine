#pragma once
#include <memory>
#include <vector>
#include <string>

template <typename T>
class Column : public std::enable_shared_from_this<Column<T>> {
public:
    void Clear() noexcept {
        data_.clear();
    }
    bool Empty() const noexcept {
        return data_.empty();
    }
    size_t Size() const noexcept {
        return data_.size();
    }
    size_t Capacity() const noexcept {
        return data_.capacity();
    }
    void Reserve(size_t size) {
        data_.reserve(size);
    }
    void PushBack(const T& value) {
        data_.push_back(value);
    }
    void PushBack(T&& value) {
        data_.push_back(std::move(value));
    }
    T* Data() noexcept {
        return data_.data();
    }
    virtual ~Column() = default;
protected:
    std::vector<T> data_;
};

class StringColumn : public Column<std::string>{
public:
private:
    std::vector<std::string> data_;
};

class Int64Column : public Column<int64_t>{
public:
private:
    std::vector<int64_t> data_;
};

template <typename Derived , typename Value>
std::shared_ptr<Derived> As(const std::shared_ptr<Column<Value>>& obj) {
    return std::dynamic_pointer_cast<Derived>(obj);
}

template <typename Derived , typename Value>
bool Is(const std::shared_ptr<Column<Value>>& obj) {
    return static_cast<bool>(std::dynamic_pointer_cast<Derived>(obj));
}