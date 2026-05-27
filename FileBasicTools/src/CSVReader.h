#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include "Row.h"
#include "Batch.h"

static size_t STANDART_BUCKET_SIZE = 1024 * 1024 * 8;
static size_t STANDART_ROWS_COUNT = 8192;

class CSVReader {
public:
    CSVReader(const std::string& filePath , size_t bucket_size = STANDART_BUCKET_SIZE);

    void BOMHelper();

    std::vector<uint8_t> ReadFileData();

    void ReadRowCSV(StringBacket& data , size_t& bytes , char delimetr = ',');

    std::vector<StringBacket> ReadFullTable(char delimiter = ',');

    void ReadChunk(Batch& data , char delimiter = ',') {
        data_.Clear();
        if (initial_chunk_) {
            BOMHelper();
            initial_chunk_ = false;
        }
        size_t current_size = 0;
        size_t old_size = 0;
        for (size_t num = 0; num < STANDART_ROWS_COUNT; ++num) {
            ReadRowCSV(data_ , current_size , delimiter);
            if (old_size == current_size) {
                break;
            }
            data_.EndRow();
            data.AddRowFromCSV(data_);
            old_size = current_size;
            data_.Clear();
        }
        // std::cerr << data.GetRows() << ' ' << data.Size() << std::endl;
    }

    void ReadChunk(std::vector<StringBacket>& data , char delimiter = ',') {
        data.clear();
        if (initial_chunk_) {
            BOMHelper();
            initial_chunk_ = false;
        }
        data.reserve(bucket_size_ * 2);
        size_t current_size = 0;
        size_t old_size = 0;
        StringBacket Row;
        while (current_size < bucket_size_) {
            ReadRowCSV(Row , current_size , delimiter);
            if (old_size == current_size) {
                break;
            }
            old_size = current_size;
            data.push_back(std::move(Row));
            Row.Clear();
        }
    }

private:
    enum class CURSOR_STATE {
        IN_QUOTE,
        NOT_IN_QUOTE,
    };
    const std::string& filePath_;
    size_t bucket_size_;
    bool initial_chunk_ = true;
    StringBacket data_;
    std::ifstream stream_;
};
