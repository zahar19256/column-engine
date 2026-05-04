#pragma once
#include "../../FileBasicTools/src/BelZReader.h"

#include 
class Scaner {
public:
    Scaner(const std::string& table_name , const std::vector<std::string_view>& column_names);
private:
    BelZReader reader_;
};