#pragma once
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
#include <string>
#include <memory.h>

class Column {
public:
    virtual void Reserve(size_t n) = 0;
    virtual void Clear() = 0;
    virtual void AppendFromString(const char* start , size_t size) = 0;
    virtual size_t Size() const = 0;
    virtual ~Column() = default;
};

class StringColumn final : public Column {
public:
    void Reserve(size_t n) override {
        data_.resize(n);
    }
    void ReserveOffset(size_t n) {
        offsets_.resize(n);
    }
    void AppendFromString(const char* start , size_t size) override {
        data_.append(start , size);
        offsets_.push_back(data_.size());
    }
    void AppendFromString(const std::string& val) {
        data_.append(val);
        offsets_.push_back(data_.size());
    }
    void AppendFromRow(const char* data , size_t size) {
        data_.append(data , size);
        offsets_.push_back(data_.size());
    }
    void Push_Back(std::string&& value) {
        data_.append(std::move(value));
        offsets_.push_back(data_.size());
    }
    void Push_Back(const std::string& value) {
        data_.append(value);
        offsets_.push_back(data_.size());
    }
    char* GetDataPointer() {
        return data_.data();
    }
    const std::string& GetData() {
        return data_;
    }
    size_t GetDataSize() {
        return data_.size();
    }

    std::string operator[](size_t index) {
        if (index >= offsets_.size()) {
            throw std::runtime_error("Index is out of StringColumn range");
        }
        size_t size = offsets_[index];
        size_t start = 0;
        if (index > 0) {
            size -= offsets_[index - 1];
            start = offsets_[index - 1];
        }
        return data_.substr(start , size);
    }
    size_t* GetOffsetPointer() {
        return offsets_.data();
    }
    const char* GetStringPointer(size_t index) {
        if (index >= offsets_.size()) {
            throw std::runtime_error("Index is out of StringColumn range");
        }
        size_t start = 0;
        if (index > 0) {
            start = offsets_[index - 1];
        }
        return data_.data() + start;
    }
    size_t GetStringSize(size_t index) {
        if (index >= offsets_.size()) {
            throw std::runtime_error("Index is out of StringColumn range");
        }
        size_t size = offsets_[index];
        if (index > 0) {
            size -= offsets_[index - 1];
        }
        return size;
    }
    size_t Size() const noexcept override {
        return offsets_.size();
    }
    void Clear() noexcept override {
        data_.clear();
        offsets_.clear();
    }
private:
    std::string data_;
    std::vector<size_t> offsets_;
};

class Int64Column final : public Column {
public:
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    void Resize(size_t n) {
        data_.resize(n);
    }
    void AppendFromString(const char* start , size_t size) override {
        std::string now;
        now.resize(size);
        memcpy(now.data(), start, size);
        data_.push_back(std::stoll(now));
    }
    void AppendFromString(std::string val) {
        data_.push_back(std::stoll(val));
    }
    void Push_Back(int64_t value) {
        data_.push_back(value);
    }
    int64_t* Data() noexcept {
        return data_.data();
    }
    const int64_t* Data() const noexcept {
        return data_.data();
    }
    int64_t operator[](size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    size_t Size() const noexcept override {
        return data_.size();
    }
    void Clear() noexcept override {
        data_.clear();
    }
private:
    std::vector<int64_t> data_;
};

template <typename T>
std::shared_ptr<T> As(const std::shared_ptr<Column>& obj) {
    return std::dynamic_pointer_cast<T>(obj);
}

template <typename T>
bool Is(const std::shared_ptr<Column>& obj) {
    return dynamic_cast<T*>(obj.get()) != nullptr;
}