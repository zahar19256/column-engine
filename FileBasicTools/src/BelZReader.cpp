#include "BelZReader.h"
#include "Column.h"
#include "Decoder.h"
#include "MetaData.h"
#include "Scheme.h"
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

BelZReader::BelZReader(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        throw std::runtime_error("File not found: " + filePath);
    }
    stream_.open(filePath, std::ios::binary);
    if (!stream_.is_open() || !stream_) {
        throw std::runtime_error("Failed to open CSV for reading in BelZReader constructor: " + filePath);
    }
    ReadMetaData();
}

bool BelZReader::Empty() const {
    return batches_left_ == 0;
}

std::shared_ptr<Column> BelZReader::ReadStringColumn(size_t size) {
    auto result = std::make_shared<FlatStringColumn>();
    size_t data_sz;
    size_t offset_sz;
    stream_.read(reinterpret_cast<char*>(&data_sz), sizeof(size_t));
    stream_.read(reinterpret_cast<char*>(&offset_sz), sizeof(size_t));
    if (!stream_) {
        throw std::runtime_error("Failed to read StringColumn header");
    }
    if (offset_sz != size) {
        throw std::runtime_error("StringColumn size mismatch: expected " + std::to_string(size) + ", got " +
                                 std::to_string(offset_sz));
    }
    result->ResizeData(data_sz);
    result->ResizeOffset(offset_sz);
    stream_.read((result->GetDataPointer()), data_sz);

    stream_.read(reinterpret_cast<char*>(result->GetOffsetPointer()), offset_sz * sizeof(size_t));
    return result;
}

std::shared_ptr<Column> BelZReader::ReadDoubleColumn(size_t size) {
    return ReadFixedWidthColumn<double, DoubleColumn>(size);
}

std::shared_ptr<Column> BelZReader::ReadDateColumn(size_t size) {
    return ReadFixedWidthColumn<int64_t, DateColumn>(size);
}

std::shared_ptr<Column> BelZReader::ReadTimestampColumn(size_t size) {
    return ReadFixedWidthColumn<int64_t, TimeStampColumn>(size);
}

std::shared_ptr<Column> BelZReader::ReadColumn(size_t size, ColumnType type, Utility::StringArena* arena,
                                               ssize_t need_offset) {
    if (need_offset != -1) {
        stream_.seekg(need_offset);
    }
    CodecType codec;
    size_t payload_size = 0;
    stream_.read(reinterpret_cast<char*>(&codec), sizeof(codec));
    stream_.read(reinterpret_cast<char*>(&payload_size), sizeof(payload_size));
    if (!stream_) {
        throw std::runtime_error("Failed to read encoded column header");
    }
    EncodedColumn encoded;
    encoded.codec = codec;
    encoded.data.resize(payload_size);
    if (payload_size != 0) {
        stream_.read(reinterpret_cast<char*>(encoded.data.data()), payload_size);
        if (!stream_) {
            throw std::runtime_error("Failed to read encoded column payload");
        }
    }
    auto result = Decoder::DecodeColumn(encoded, type, arena);
    if (result->Size() != size) {
        throw std::runtime_error("Decoded column size mismatch: expected " + std::to_string(size) + ", got " +
                                 std::to_string(result->Size()));
    }
    return result;
}

void BelZReader::ReadMetaData() {
    stream_.seekg(-8, std::ios::end);
    uint64_t meta_offset;
    stream_.read(reinterpret_cast<char*>(&meta_offset), sizeof(meta_offset));
    stream_.seekg(meta_offset, std::ios::beg);
    size_t col_count;
    stream_.read(reinterpret_cast<char*>(&col_count), sizeof(col_count));
    scheme_.Clear();
    meta_.Clear();
    for (size_t i = 0; i < col_count; ++i) {
        size_t len;
        stream_.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string name;
        name.resize(len);
        if (len > 0) {
            stream_.read(&name[0], len);
        }
        uint8_t type;
        stream_.read(reinterpret_cast<char*>(&type), sizeof(type));
        scheme_.Push_Back(SchemeNode{name, static_cast<ColumnType>(type)});
    }
    meta_.SetScheme(Scheme(scheme_));
    stream_.read(reinterpret_cast<char*>(&batches_left_), sizeof(batches_left_));
    for (size_t i = 0; i < batches_left_; ++i) {
        size_t offset;
        stream_.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        meta_.AddBatchOffset(offset);
    }
    for (size_t i = 0; i < batches_left_; ++i) {
        size_t rows;
        stream_.read(reinterpret_cast<char*>(&rows), sizeof(rows));
        meta_.AddRows(rows);
    }
    for (size_t i = 0; i < batches_left_ * col_count; ++i) {
        size_t offset;
        stream_.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        meta_.AddColumnOffset(offset);
    }
}

void BelZReader::ReadBatch(Batch& batch) {
    if (batches_left_ == 0) {
        throw std::runtime_error("Try to read Batch after end of file!");
    }
    batch.Clear();
    size_t current_offset = meta_.GetBatchOffset(index_of_batch);
    stream_.seekg(current_offset, std::ios::beg);
    batch.SetScheme(scheme_);
    size_t rows_count = meta_.GetRow(index_of_batch);

    for (size_t column = 0; column < scheme_.Size(); ++column) {
        auto col_ptr = ReadColumn(rows_count, scheme_.GetType(column), batch.GetStringArena());
        batch.AddColumn(col_ptr);
    }
    --batches_left_;
    ++index_of_batch;
}

void BelZReader::ReadBatch(Batch& batch, const std::vector<std::string>& column_names) {
    if (batches_left_ == 0) {
        throw std::runtime_error("Try to read Batch after end of file!");
    }
    batch.Clear();
    const size_t rows_count = meta_.GetRow(index_of_batch);
    Scheme projection_scheme;
    for (const auto& name : column_names) {
        size_t id = scheme_.GetIndex(name);
        projection_scheme.Push_Back(scheme_.GetInfo(id));
    }
    batch.SetScheme(projection_scheme);
    batch.SetRows(rows_count);
    for (size_t column = 0; column < column_names.size(); ++column) {
        auto col_ptr = ReadColumn(index_of_batch, scheme_.GetIndex(column_names[column]), batch.GetStringArena());
        batch.AddColumn(col_ptr);
    }
    --batches_left_;
    ++index_of_batch;
}

MetaData BelZReader::GetMetaData() const {
    return meta_;
}

const Scheme& BelZReader::GetScheme() const {
    return scheme_;
}

size_t BelZReader::RowsCount() const {
    size_t result = 0;
    for (size_t rows : meta_.GetRows()) {
        result += rows;
    }
    return result;
}

std::shared_ptr<Column> BelZReader::ReadColumn(size_t batch_id, size_t column_id) {
    return ReadColumn(batch_id, column_id, nullptr);
}

std::shared_ptr<Column> BelZReader::ReadColumn(size_t batch_id, size_t column_id, Utility::StringArena* arena) {
    if (batch_id >= meta_.BatchesCount()) {
        throw std::runtime_error("Batch ID out of range in ReadColumn: " + std::to_string(batch_id));
    }
    if (column_id >= scheme_.Size()) {
        throw std::runtime_error("Column ID out of range in ReadColumn: " + std::to_string(column_id));
    }
    size_t offset = meta_.GetColumnOffset(batch_id, column_id);
    ColumnType type = scheme_.GetType(column_id);
    size_t rows = meta_.GetRow(batch_id);
    return ReadColumn(rows, type, arena, offset);
}
