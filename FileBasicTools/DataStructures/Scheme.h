#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

static std::string TypeInt8 = "int8";
static std::string TypeInt16 = "int16";
static std::string TypeInt32 = "int32";
static std::string TypeInt64 = "int64";
static std::string TypeString = "string";
static std::string TypeDate = "date";
static std::string TypeTimestamp = "timestamp";
static std::string TypeDouble = "double";

enum class ColumnType : int8_t {
    Int8 = 0,
    Int16,
    Int32,
    Int64,
    Double,
    String,
    Date,
    Timestamp,
    Unknown,
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
    const SchemeNode* GetData() const;
    const std::string& GetName(size_t index) const;
    size_t GetIndex(const std::string& column_name) const;
    ColumnType GetType(size_t index) const;
    const SchemeNode& GetInfo(size_t index) const;
    size_t Size() const;
    void Clear();
private:
    std::vector<SchemeNode> columns_;
    std::unordered_map<std::string , size_t> columns_id_;
};