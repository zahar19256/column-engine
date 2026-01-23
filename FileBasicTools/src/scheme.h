#pragma once
#include <string>
#include <vector>
#include "CSVReader.h"

enum class ColumnType : int8_t {
    Int64 = 1,
    String = 2,
    Unknow = 3,
};

ColumnType DatumConvertor(const std::string& type);

class Scheme {
public:
    struct Node {
        std::string name;
        ColumnType data;
    };
    void ReadScheme(const std::string& filePath);
    std::vector<Node> GetScheme();
private:
    std::vector<Node> colomns_;
};