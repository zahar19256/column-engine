#include "scheme.h"
#include <stdexcept>

ColumnType DatumConvertor(const std::string& type) {
    if (type == "String") {
        return  ColumnType::String;
    }
    if (type == "Int64") {
        return  ColumnType::Int64;
    }
    return ColumnType::Unknow;
}

void Scheme::ReadScheme(const std::string& filePath) {
    CSVReader scan(filePath);
    std::vector<std::vector<std::string>> table = scan.ReadFullTable();
    for (auto row : table) {
        if (row.size() != 2) {
            throw std::runtime_error("Scheme table has more than two fields in one row!");
        }
        colomns_.push_back(Node(row[0] , DatumConvertor(row[1])));
    }
}