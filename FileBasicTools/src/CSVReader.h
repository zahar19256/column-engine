#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include "Row.h"

const size_t STANDART_BUCKET_SIZE = 1024 * 1024 * 4;

class CSVReader {
public:
    CSVReader(const std::string& filePath , size_t bucket_size = STANDART_BUCKET_SIZE);

    void BOMHelper();

    std::vector<uint8_t> ReadFileData();

    void ReadRowCSV(Row<std::string>& data , size_t& bytes , char delimetr = ',');

    std::vector<std::vector<std::string>> ReadFullTable(char delimiter = ',');

    template <typename OutputVector>
    void ReadChunk(OutputVector&& table , char delimiter = ',') {
        table.clear();
        if (initial_chunk_) {
            BOMHelper();
            initial_chunk_ = false;
        }
        Row<std::string> row;
        size_t current_size = 0;
        while (current_size < bucket_size_) {
            ReadRowCSV(row , current_size , delimiter);
            if (row.Empty()) {
                break;
            }
            table.push_back(std::move(row));
            row.Clear();
        }
    }

private:
    const std::string& filePath_;
    size_t bucket_size_;
    bool initial_chunk_ = true;
    std::ifstream stream_;
};