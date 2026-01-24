#pragma once
#include "Row.h"
#include <fstream>
#include <cstdint>

// THIS CLASS CAN READ A ROW FROM FILE TILL \N 
// That's will help with our my format reader and CSVReader

class BaseReader {
public:
    void ReadRowCSV(Row<std::string>& data , size_t& bytes , char delimetr = ',');
    void ReadRowBytes(Row<uint8_t>& data , size_t& bytes , char delimetr = ',');
protected:
    std::ifstream stream_;
};