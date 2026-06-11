#pragma once
#include "Scheme.h"
#include <cstddef>
#include <vector>

class MetaData {
public:
    void AddBatchOffset(size_t offset);
    void AddColumnOffset(size_t offset);
    void AddCodec(size_t codec);
    void AddRows(size_t count);
    void SetScheme(Scheme&& scheme) {
        scheme_ = std::move(scheme);
    }
    const std::vector<size_t>& GetBatchOffsets() const;
    const std::vector<size_t>& GetColumnOffsets() const;
    const std::vector<size_t>& GetRows() const;
    const std::vector<size_t>&
    GetCodec() const; // TODO Impliment it вероятно тут будет более сложная штука чем массив интов так как сжатие надо
                      // для каждой строки отдельно, а пока что у нас для одного батча один вид сжатия
    size_t BatchesCount() const;
    size_t ColumnsCount() const;
    void Clear();
    size_t GetBatchOffset(size_t batch_index) const;
    size_t GetColumnOffset(size_t batch_index, size_t column_index) const;
    size_t GetRow(size_t index) const;
    size_t GetCodec(size_t index) const; // TODO Impliment it
    const Scheme& GetScheme() const;

private:
    std::vector<size_t> batch_offsets_;
    std::vector<size_t> column_offsets_;
    std::vector<size_t> codec_;
    std::vector<size_t> rows_;
    Scheme scheme_;
};
