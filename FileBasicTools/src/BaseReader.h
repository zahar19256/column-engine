#pragma once
#include "row.h"
#include <fstream>

// THIS CLASS CAN READ A ROW FROM FILE TILL \N 
// That's will help with our my format reader and CSVReader

class BaseReader {
public:
    void ReadRow(Row<std::string>& data , size_t& bytes , char delimetr = ',');
protected:
    std::ifstream stream_;
};