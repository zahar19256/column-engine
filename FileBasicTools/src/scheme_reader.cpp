#include "scheme_reader.h"
#include <memory>
#include <stdexcept>

std::shared_ptr<Datum> DatumConvertor(const std::string& type) {
    if (type == "int64") {
        return std::make_shared<Int64>();
    }
    if (type == "string") {
        return std::make_shared<String>();
    }
    throw std::runtime_error("Unknown type of Data");
}

void SchemeReader::ReadScheme(const std::string& filePath) {
    FileReader scan(filePath);
    std::vector<std::vector<std::string>> table = scan.ReadFullTable();
    for (auto row : table) {
        if (row.size() != 2) {
            throw std::runtime_error("Scheme table has more than two fields in one row!");
        }
        colomns_.push_back(Node(row[0] , DatumConvertor(row[1])));
    }
}