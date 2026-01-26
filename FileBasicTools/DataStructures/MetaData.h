#pragma once
#include "Scheme.h"
#include <cstddef>
#include <vector>

class MetaData {
public:
    void AddOffset(size_t offset);
    void AddCodec(size_t codec);
    void AddRows(size_t count);
    void SetScheme(Scheme&& scheme) {
        scheme_ = std::move(scheme);
    }
    const std::vector<size_t>& GetOffsets() const;
    const std::vector<size_t>& GetRows() const;
    const std::vector<size_t>& GetCodec() const; // TODO Impliment it
    size_t Size() const;
    void Clear();
    size_t GetOffset(size_t index) const;
    size_t GetRow(size_t index) const;
    size_t GetCodec(size_t index) const; // TODO Impliment it
    const Scheme& GetScheme() const;
private:
    std::vector <size_t> offsets_;
    std::vector <size_t> codec_;
    std::vector <size_t> rows_;
    Scheme scheme_;
};