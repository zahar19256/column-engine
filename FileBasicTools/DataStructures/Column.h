#pragma once
#include <memory>
#include <vector>
#include <string>

class Column {
public:
    virtual void Reserve(size_t n) = 0;
    virtual void AppendFromString(const std::string&) = 0;
    virtual size_t Size() const = 0;
    virtual ~Column() = default;
};

class StringColumn final : public Column {
public:
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    void AppendFromString(const std::string& data) override {
        data_.push_back(data);
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
    void AppendFromString(const std::string& s) override {
        data_.push_back(std::stoll(s));
    }
    const int64_t* Data() const noexcept {
        return data_.data();
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