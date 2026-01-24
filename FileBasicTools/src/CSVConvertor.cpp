#include "CSVConvertor.h"
#include "CSVReader.h"

void CSVConvertor::GetScheme(const std::string& SchemeFilePath) {
    CSVReader scan(SchemeFilePath);
    std::vector<std::vector<std::string>> table = scan.ReadFullTable();
    for (auto row : table) {
        if (row.size() != 2) {
            throw std::runtime_error("Scheme table has more than two fields in one row!");
        }
        scheme_.Push_Back(Node(row[0] , DatumConvertor(row[1])));
    }
}

void CSVConvertor::GetBatches(const std::string& CSVFilePath) {
    CSVReader scan(CSVFilePath);
    while (true) {
        std::vector <Row<std::string>> chunk = scan.ReadChunk();
        batches_.push_back(Batch(chunk , scheme_));
    }
}