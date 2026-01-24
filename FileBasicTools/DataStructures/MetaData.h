#pragma once
#include <cstddef>
#include <vector>

class MetaData {
public:
    void AddOffset(size_t offset);
    void AddCodec(size_t codec);
    void AddRows(size_t count);
    const std::vector<size_t>& GetOffests() const;
    const std::vector<size_t>& GetRows() const;
    size_t Size() const;
    void Clear();
private:
    std::vector <size_t> offsets_;
    std::vector <size_t> codec_;
    std::vector <size_t> rows_;
};