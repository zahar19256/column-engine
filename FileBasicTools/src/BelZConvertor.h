#pragma once
#include "BelZReader.h"
class BelZConvertor {
public:
    bool GetBatch(BelZReader& scan_);
    void MakeCSV(const std::string& BelZFilePath);
private:
    Batch batch_;
};