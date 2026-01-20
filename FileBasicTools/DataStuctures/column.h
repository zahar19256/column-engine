#pragma "once"
#include "sheme.h"
#include <vector>
#include <string>

class Column {
public:
    virtual ~Column() = default;
};

class StringColumn : public Column{
public:
private:
    std::vector<std::string> data_;
};

class Int64Column : public Column{
public:
private:
    std::vector<int64_t> data_;
};