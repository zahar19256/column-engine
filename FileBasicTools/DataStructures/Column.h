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
    virtual void AppendFromString(const char* start , size_t size) = 0;
    virtual size_t Size() const = 0;
    virtual ~Column() = default;
};

class StringColumn final : public Column {
public:
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    void AppendFromString(const char* start , size_t size) override {
        data_.push_back("");
        data_.back().resize(size);
        memcpy(data_.back().data() , start , size);
    }
    void AppendFromString(std::string val) {
        data_.push_back(val);
    }
    void AppendFromRow(const char* data , size_t size) {
        data_.push_back(data);
    }
    void Push_Back(std::string&& value) {
        data_.push_back(std::move(value));
    }
    void Push_Back(const std::string& value) {
        data_.push_back(value);
    }
    const std::string& operator[](size_t index) {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of StringColumn range");
        }
        return data_[index];
    }
    size_t Size() const noexcept override {
        return data_.size();
    }
private:
    std::vector<std::string> data_;
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
        data_.push_back(std::stoi(now));
    }
    void AppendFromString(std::string val) {
        data_.push_back(std::stoi(val));
    }
    void Push_Back(int64_t value) {
        data_.push_back(value);
    }
    int64_t* Data() noexcept {
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