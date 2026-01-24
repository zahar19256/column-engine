#pragma once
#include <string>
#include <vector>

enum class ColumnType : int8_t {
    Int64 = 1,
    String = 2,
    Unknow = 3,
};

struct Node {
    std::string name;
    ColumnType data;
};

ColumnType DatumConvertor(const std::string& type);

class Scheme {
public:
    void Push_Back(const Node& value);
    void Push_Back(Node&& value);
    const std::vector<Node>& GetScheme();
private:
    std::vector<Node> colomns_;
};