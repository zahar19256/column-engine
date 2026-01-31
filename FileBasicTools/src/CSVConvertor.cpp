#include "CSVConvertor.h"
#include "BelZWriter.h"
#include "Scheme.h"

void CSVConvertor::Reset() {
    chunk_.clear();
    meta_.Clear();
    scheme_.Clear();
}

void CSVConvertor::SetScheme(const Scheme& scheme) {
    scheme_ = scheme;
}

void CSVConvertor::GetScheme(const std::string& SchemeFilePath) {
    if (SchemeFilePath == "BENCHTIME.GO") { // TODO пока что это костыль для мини бенча к КТ 1 потом будет убрано 
        return;
    }
    CSVReader tmp_scan(SchemeFilePath);
    std::vector<std::vector<std::string>> table = tmp_scan.ReadFullTable();
    for (auto row : table) {
        if (row.size() != 2) {
            throw std::runtime_error("Scheme table has more than two fields in one row!");
        }
        scheme_.Push_Back(SchemeNode(row[0] , DatumConvertor(row[1])));
    }
}

bool CSVConvertor::GetChunk(CSVReader& scan_) {
    scan_.ReadChunk(chunk_);
    if (chunk_.empty()) {
        return false;
    }
    return true;
}

void CSVConvertor::MakeBelZFormat(const std::string& CSVFilePath, const std::string& SchemeFilePath) {
    Reset();

    CSVReader scan_(CSVFilePath);
    GetScheme(SchemeFilePath);

    BelZWriter writer(CSVFilePath);
    size_t col_count = scheme_.Size();
    while (GetChunk(scan_)) {
        size_t rows_in_chunk = chunk_.size();
        meta_.AddOffset(writer.GetOffSet());
        meta_.AddRows(rows_in_chunk);
        meta_.AddCodec(0);
        for (size_t column_idx = 0; column_idx < col_count; ++column_idx) {
            for (size_t row_idx = 0; row_idx < rows_in_chunk; ++row_idx) {
                writer.WriteData(chunk_[row_idx][column_idx], scheme_.GetType(column_idx));
            }
        }
    }
    meta_.SetScheme(std::move(scheme_));
    writer.WriteMeta(std::move(meta_));
}