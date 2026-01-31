#pragma once
#include "Scheme.h"
#include "MetaData.h"
#include "CSVReader.h"
#include <string>

class CSVConvertor {
public:
    void Reset();
    bool GetChunk(CSVReader& scan_);
    void SetScheme(const Scheme& scheme);
    void GetScheme(const std::string& SchemeFilePath);
    void MakeBelZFormat(const std::string& CSVFilePath , const std::string& SchemeFilePath);
private:
    Scheme scheme_;
    std::vector <Row<std::string>> chunk_;
    MetaData meta_;
};