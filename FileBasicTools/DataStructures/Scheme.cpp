#include "Scheme.h"
#include <stdexcept>

ColumnType DatumConvertor(const std::string& type) {
    if (type == "String") {
        return  ColumnType::String;
    }
    if (type == "Int64") {
        return  ColumnType::Int64;
    }
    return ColumnType::Unknown;
}

void Scheme::Push_Back(const Node& value) {
    columns_.push_back(value);
}

void Scheme::Push_Back(Node&& value) {
    columns_.push_back(std::move(value));
}

const std::vector<Node>& Scheme::GetScheme() const {
    return columns_;
}

const std::string& Scheme::GetName(size_t index) const {
    if (index >= columns_.size()) {
        throw std::runtime_error("Index of GetName is out of range!");
    }
    return columns_[index].name;
}

ColumnType Scheme::GetType(size_t index) const {
    if (index >= columns_.size()) {
        throw std::runtime_error("Index of GetType is out of range!");
    }
    return columns_[index].type;
}

const Node& Scheme::GetInfo(size_t index) const {
    if (index >= columns_.size()) {
        throw std::runtime_error("Index of GetInfo is out of range!");
    }
    return columns_[index];
}

size_t Scheme::Size() const {
    return columns_.size();
}