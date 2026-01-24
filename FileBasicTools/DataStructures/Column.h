#pragma once
#include <memory>
#include <vector>
#include <string>

template <typename T>
void DataConvertor(std::vector<T>& dst , const std::vector<std::string>& data ) {
    // TODO
}

class Column : public std::enable_shared_from_this<Column> {
public:
    
    virtual ~Column() = default;
};

class StringColumn : public Column{
public:
private:
    std::vector<std::string> data_;
};

class Int64Column : public Column{
public:
private:
    std::vector<int64_t> data_;
};

template <typename T>
std::shared_ptr<T> As(const std::shared_ptr<Column>& obj) {
    return std::dynamic_pointer_cast<T>(obj);
}

template <typename T>
bool Is(const std::shared_ptr<Column>& obj) {
    return static_cast<bool>(std::dynamic_pointer_cast<T>(obj));
}