#pragma once
#include <vector>
#include <cstdint>

class MetaData {
public:
    void AddBytes(std::vector<uint8_t> bytes);
    void AddBytes(const uint8_t* ptr, size_t size);
    const std::vector<uint8_t>& GetData() const;
    size_t Size() const;
    void Clear();
private:
    std::vector <uint8_t> data_;
};