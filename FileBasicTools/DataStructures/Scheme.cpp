#include "Scheme.h"

ColumnType DatumConvertor(const std::string& type) {
    if (type == "String") {
        return  ColumnType::String;
    }
    if (type == "Int64") {
        return  ColumnType::Int64;
    }
    return ColumnType::Unknow;
}

void Scheme::Push_Back(const Node& value) {
    colomns_.push_back(value);
}

void Scheme::Push_Back(Node&& value) {
    colomns_.push_back(std::move(value));
}

const std::vector<Node>& Scheme::GetScheme() {
    return colomns_;
}