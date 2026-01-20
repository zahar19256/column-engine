#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <fstream>

const size_t STANDART_BUCKET = 4096;

class FileReader {
public:
    FileReader(const std::string& filePath , ssize_t bucket_size = STANDART_BUCKET);

    std::vector<uint8_t> ReadFileData();

    std::vector<std::vector<std::string>> ReadFullTable(char delimiter = ',');

    std::vector<std::vector<std::string>> ReadChunk(char delimiter = ',');

private:
    std::ifstream stream_;
    const std::string& filePath_;
    size_t bucket_;
    bool initial_chunk_ = true;
};

bool FileExists(const std::string& filePath);

void WriteBytesInFile(const std::string& destPath, const std::vector<uint8_t>& data);