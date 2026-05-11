#include "Scheme.h"
#include <stdexcept>

ColumnType DatumConvertor(const std::string& type) {
    if (type == TypeString) {
        return  ColumnType::String;
    }
    if (type == TypeInt64) {
        return  ColumnType::Int64;
    }
    if (type == TypeInt128) {
        return  ColumnType::Int128;
    }
    if (type == TypeInt32) {
        return  ColumnType::Int32;
    }
    if (type == TypeInt16) {
        return  ColumnType::Int16;
    }
    if (type == TypeInt8) {
        return  ColumnType::Int8;
    }
    if (type == TypeDate) {
        return  ColumnType::Date;
    }
    if (type == TypeTimestamp) {
        return  ColumnType::Timestamp;
    }
    if (type == TypeDouble) {
        return  ColumnType::Double;
    }
    return ColumnType::Unknown;
}

void Scheme::Push_Back(const SchemeNode& value) {
    columns_id_[value.name] = columns_.size();
    columns_.push_back(value);
}

void Scheme::Push_Back(SchemeNode&& value) {
    columns_id_[value.name] = columns_.size();
    columns_.push_back(std::move(value));
}

const SchemeNode* Scheme::GetData() const {
    return columns_.data();
}


const std::string& Scheme::GetName(size_t index) const {
    if (index >= columns_.size()) {
        throw std::runtime_error("Index of GetName is out of range!");
    }
    return columns_[index].name;
}

size_t Scheme::GetIndex(const std::string& column_name) const {
    auto it = columns_id_.find(column_name);
    if (it == columns_id_.end()) {
        throw std::runtime_error("No such column: \"" + column_name + "\" in batch!");
    }
    return it->second;
}

ColumnType Scheme::GetType(size_t index) const {
    if (index >= columns_.size()) {
        throw std::runtime_error("Index of GetType is out of range!");
    }
    return columns_[index].type;
}

const SchemeNode& Scheme::GetInfo(size_t index) const {
    if (index >= columns_.size()) {
        throw std::runtime_error("Index of GetInfo is out of range!");
    }
    return columns_[index];
}

size_t Scheme::Size() const {
    return columns_.size();
}

void Scheme::Clear() {
    columns_.clear();
    columns_id_.clear();
}
