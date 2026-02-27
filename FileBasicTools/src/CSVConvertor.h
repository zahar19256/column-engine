#pragma once
#include "Row.h"
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
    StringBacket chunk_;
    MetaData meta_;
};