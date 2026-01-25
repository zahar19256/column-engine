#pragma once
#include <string>
#include <vector>

enum class ColumnType : int8_t {
    Int64 = 1,
    String = 2,
    Unknown = 3,
};

struct SchemeNode {
    std::string name;
    ColumnType type;
};

ColumnType DatumConvertor(const std::string& type);

class Scheme {
public:
    void Push_Back(const SchemeNode& value);
    void Push_Back(SchemeNode&& value);
    const std::vector<SchemeNode>& GetScheme() const;
    const std::string& GetName(size_t index) const;
    ColumnType GetType(size_t index) const;
    const SchemeNode& GetInfo(size_t index) const;
    size_t Size() const;
    void Clear();
private:
    std::vector<SchemeNode> columns_;
};