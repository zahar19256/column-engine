#include "CSVConvertor.h"
#include "BelZWriter.h"
#include "Row.h"
#include "Scheme.h"
#include <cstring>
#include <iostream>
#include <memory.h>

void CSVConvertor::Reset() {
    chunk_.Clear();
    meta_.Clear();
    //scheme_.Clear();
}

void CSVConvertor::SetScheme(const Scheme& scheme) {
    scheme_ = scheme;
}

void CSVConvertor::GetScheme(const std::string& SchemeFilePath) {
    if (SchemeFilePath == "BENCHTIME.GO") { // TODO пока что это костыль для мини бенча к КТ 1 потом будет убрано 
        return;
    }
    CSVReader tmp_scan(SchemeFilePath);
    std::vector<StringBacket> table = tmp_scan.ReadFullTable();
    for (auto row : table) {
        if (row.Size() != 2) {
            throw std::runtime_error("Scheme table has more than two fields in one row!");
        }
        scheme_.Push_Back(SchemeNode(row[0] , DatumConvertor(row[1])));
    }
}

bool CSVConvertor::GetChunk(CSVReader& scan_) {
    chunk_.Clear();
    scan_.ReadChunk(chunk_);
    if (chunk_.Empty()) {
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
        size_t rows_in_chunk = chunk_.GetRows();
        //std::cerr << "!!!" << rows_in_chunk << ' ' << col_count << std::endl;
        meta_.AddOffset(writer.GetOffSet());
        meta_.AddRows(rows_in_chunk);
        meta_.AddCodec(0);
        for (size_t column_idx = 0; column_idx < col_count; ++column_idx) {
            for (size_t row_idx = 0; row_idx < rows_in_chunk; ++row_idx) {
                //std::cerr << row_idx << ' ' << column_idx << std::endl;
                writer.Append(chunk_.GetString(row_idx , column_idx) , chunk_.GetSize(row_idx, column_idx) , scheme_.GetType(column_idx));
            }
            writer.Flush();
        }
    }
    meta_.SetScheme(std::move(scheme_));
    writer.WriteMeta(std::move(meta_));
}