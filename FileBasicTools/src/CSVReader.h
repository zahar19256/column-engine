#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include "Row.h"

static size_t STANDART_BUCKET_SIZE = 1024 * 1024 * 4;

class CSVReader {
public:
    CSVReader(const std::string& filePath , size_t bucket_size = STANDART_BUCKET_SIZE);

    void BOMHelper();

    std::vector<uint8_t> ReadFileData();

    void ReadRowCSV(StringBacket& data , size_t& bytes , char delimetr = ',');

    std::vector<StringBacket> ReadFullTable(char delimiter = ',');

    void ReadChunk(StringBacket& data , char delimiter = ',') {
        data.Clear();
        if (initial_chunk_) {
            BOMHelper();
            initial_chunk_ = false;
        }
        size_t current_size = 0;
        size_t old_size = 0;
        while (current_size < bucket_size_) {
            ReadRowCSV(data , current_size , delimiter);
            if (old_size == current_size) {
                break;
            }
            old_size = current_size;
            data.EndRow();
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
    std::ifstream stream_;
};