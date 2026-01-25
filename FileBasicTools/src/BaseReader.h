#pragma once
#include "Row.h"
#include <fstream>
#include <cstdint>

// THIS CLASS CAN READ A ROW FROM FILE TILL \N 
// That's will help with our my format reader and CSVReader

class BaseReader {
public:
    void ReadRowCSV(Row<std::string>& data , size_t& bytes , char delimetr = ',');
    void ReadBytes(std::vector<uint8_t>& data , size_t size);
protected:
    std::ifstream stream_;
};

bool FileExists(const std::string &filePath);