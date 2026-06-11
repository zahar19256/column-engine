#pragma once
#include "CSVReader.h"
#include "MetaData.h"
#include "Scheme.h"
#include <string>

class CSVConvertor {
public:
    void Reset();
    bool GetChunk(CSVReader& scan_);
    void SetScheme(const Scheme& scheme);
    void GetScheme(const std::string& SchemeFilePath);
    void MakeBelZFormat(const std::string& CSVFilePath, const std::string& SchemeFilePath);
    void MakeBelZFormat(const std::string& CSVFilePath, const std::string& SchemeFilePath,
                        const std::string& outputFilePath);

private:
    Scheme scheme_;
    Batch chunk_;
    MetaData meta_;
};
