#pragma once
#include <string>
#include <vector>

enum class ColumnType : int8_t {
    Int64 = 1,
    String = 2,
    Unknown = 3,
};

struct Node {
    std::string name;
    ColumnType type;
};

ColumnType DatumConvertor(const std::string& type);

class Scheme {
public:
    void Push_Back(const Node& value);
    void Push_Back(Node&& value);
    const std::vector<Node>& GetScheme() const;
    const std::string& GetName(size_t index) const;
    ColumnType GetType(size_t index) const;
    const Node& GetInfo(size_t index) const;
private:
    std::vector<Node> columns_;
};