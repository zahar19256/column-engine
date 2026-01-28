#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include "Row.h"

const size_t STANDART_BUCKET = 4096;

class CSVReader {
public:
    CSVReader(const std::string& filePath , ssize_t bucket_size = STANDART_BUCKET);

    void BOMHelper();

    std::vector<uint8_t> ReadFileData();

    void ReadRowCSV(Row<std::string>& data , size_t& bytes , char delimetr = ',');

    std::vector<std::vector<std::string>> ReadFullTable(char delimiter = ',');

    std::vector<Row<std::string>> ReadChunk(char delimiter = ',');

    bool ReadBatch(char delimiter = ',');

private:
    const std::string& filePath_;
    size_t bucket_;
    bool initial_chunk_ = true;
    std::ifstream stream_;
};