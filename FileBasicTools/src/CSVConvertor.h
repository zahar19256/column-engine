#pragma once
#include "Scheme.h"
#include "Batch.h"
#include "MetaData.h"
#include <string>

class CSVConvertor {
public:
    void GetBatches(const std::string& CSVFilePath);
    void GetScheme(const std::string& SchemeFilePath);
    void MakeBelZFormat();
private:
    Scheme scheme_;
    std::vector <Batch> batches_;
    MetaData meta_;
};