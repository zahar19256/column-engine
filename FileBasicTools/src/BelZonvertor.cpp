#include "BelZConvertor.h"
#include "BelZReader.h"
#include "CSVWriter.h"

bool BelZConvertor::GetBatch(BelZReader& scan_) {
    if (scan_.Empty()) {
        return false;
    }
    scan_.ReadBatch(batch_);
    return true;
}

void BelZConvertor::MakeCSV(const std::string& filePath) {
    BelZReader scan_(filePath);
    CSVWriter writer_(filePath);
    while (GetBatch(scan_)) {
        writer_.WriteBatch(batch_);
    }
}