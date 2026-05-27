#pragma once
#include "Batch.h"
#include "Column.h"
#include "MetaData.h"
#include "Scheme.h"
#include <cstddef>
#include <fstream>
#include <memory>
#include <vector>

class BelZReader {
public:
    BelZReader(const std::string& filePath);
    void ReadBatch(Batch& batch);
    void ReadBatch(Batch& batch , const std::vector<std::string>& column_names);
    void ScanBatch(size_t index , Batch& batch);
    std::shared_ptr<Column> ReadColumn(size_t size , ColumnType type , ssize_t need_offset = -1);

    template <typename ValueT , typename ColumnT>
    std::shared_ptr<Column> ReadFixedWidthColumn(size_t size) {
        auto result = std::make_shared<ColumnT>();
        if (size > 0) {
            result->Resize(size);
            stream_.read(reinterpret_cast<char*>(result->Data()), size * sizeof(ValueT));
            if (!stream_) {
                throw std::runtime_error("Failed to read fixed-width column block!");
            }
        }
        return result;
    }

    template <typename T>
    std::shared_ptr<Column> ReadIntergerColumn(size_t size) {
        using ColumnT = typename Data::ColumnTraits<T>::ColumnT;
        return ReadFixedWidthColumn<T , ColumnT>(size);
    }

    std::shared_ptr<Column> ReadStringColumn(size_t size);
    std::shared_ptr<Column> ReadDoubleColumn(size_t size);
    std::shared_ptr<Column> ReadDateColumn(size_t size);
    std::shared_ptr<Column> ReadTimestampColumn(size_t size);
    std::shared_ptr<Column> ReadColumn(size_t batch_id , size_t column_id);
    std::shared_ptr<Column> ReadColumn(size_t columnd_id);
    void ReadMetaData();
    MetaData GetMetaData() const;
    const Scheme& GetScheme() const;
    size_t RowsCount() const;
    bool Empty() const;
private:
    MetaData meta_;
    Scheme scheme_;
    size_t batches_left_ = 0;
    size_t index_of_batch = 0;
    std::ifstream stream_;
};
