#include "BelZReader.h"
#include "Column.h"
#include "Decoder.h"
#include "MetaData.h"
#include "Scheme.h"
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

template <typename ValueT , typename ColumnT>
std::shared_ptr<Column> ReadRawFixedPayload(const uint8_t* payload , size_t rows_count , size_t payload_size) {
    auto result = std::make_shared<ColumnT>();
    result->Resize(rows_count);
    if (payload_size != 0) {
        memcpy(result->Data() , payload , payload_size);
    }
    return result;
}

std::shared_ptr<Column> ReadRawStringPayload(const uint8_t* payload , size_t rows_count , size_t payload_size) {
    if (payload_size < 2 * sizeof(size_t)) {
        throw std::runtime_error("Invalid raw string payload header!");
    }
    auto result = std::make_shared<StringColumn>();
    size_t data_size = 0;
    size_t offsets_count = 0;
    size_t pos = 0;
    memcpy(&data_size , payload + pos , sizeof(data_size));
    pos += sizeof(data_size);
    memcpy(&offsets_count , payload + pos , sizeof(offsets_count));
    pos += sizeof(offsets_count);
    result->ResizeData(data_size);
    result->ResizeOffset(offsets_count);
    if (data_size != 0) {
        memcpy(result->GetDataPointer() , payload + pos , data_size);
        pos += data_size;
    }
    const size_t offsets_bytes = offsets_count * sizeof(size_t);
    if (offsets_bytes != 0) {
        memcpy(result->GetOffsetPointer() , payload + pos , offsets_bytes);
    }
    return result;
}

std::shared_ptr<Column> ReadRawPayload(const uint8_t* payload , size_t rows_count , ColumnType type , size_t payload_size) {
    switch(type) {
        case ColumnType::Int8:
            return ReadRawFixedPayload<int8_t , Int8Column>(payload , rows_count , payload_size);
        case ColumnType::Int16:
            return ReadRawFixedPayload<int16_t , Int16Column>(payload , rows_count , payload_size);
        case ColumnType::Int32:
            return ReadRawFixedPayload<int32_t , Int32Column>(payload , rows_count , payload_size);
        case ColumnType::Int64:
            return ReadRawFixedPayload<int64_t , Int64Column>(payload , rows_count , payload_size);
        case ColumnType::Double:
            return ReadRawFixedPayload<double , DoubleColumn>(payload , rows_count , payload_size);
        case ColumnType::Date:
            return ReadRawFixedPayload<int64_t , DateColumn>(payload , rows_count , payload_size);
        case ColumnType::Timestamp:
            return ReadRawFixedPayload<int64_t , TimeStampColumn>(payload , rows_count , payload_size);
        case ColumnType::Int128:
            return ReadRawFixedPayload<__int128_t , Int128Column>(payload , rows_count , payload_size);
        case ColumnType::String:
            return ReadRawStringPayload(payload , rows_count , payload_size);
        case ColumnType::Unknown:
            break;
    }
    throw std::runtime_error("Unknown column type in raw reader!");
}

} // namespace

BelZReader::BelZReader(const std::string& filePath) {
    OpenMmap(filePath);
    ReadMetaData();
}

BelZReader::~BelZReader() {
    CloseMmap();
}

BelZReader::BelZReader(BelZReader&& other) noexcept
    : meta_(std::move(other.meta_)) ,
      scheme_(std::move(other.scheme_)) ,
      batches_left_(other.batches_left_) ,
      index_of_batch(other.index_of_batch) ,
      fd_(other.fd_) ,
      mapped_data_(other.mapped_data_) ,
      mapped_size_(other.mapped_size_) ,
      offset_(other.offset_) {
    other.fd_ = -1;
    other.mapped_data_ = nullptr;
    other.mapped_size_ = 0;
    other.offset_ = 0;
    other.batches_left_ = 0;
    other.index_of_batch = 0;
}

BelZReader& BelZReader::operator=(BelZReader&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    CloseMmap();
    meta_ = std::move(other.meta_);
    scheme_ = std::move(other.scheme_);
    batches_left_ = other.batches_left_;
    index_of_batch = other.index_of_batch;
    fd_ = other.fd_;
    mapped_data_ = other.mapped_data_;
    mapped_size_ = other.mapped_size_;
    offset_ = other.offset_;
    other.fd_ = -1;
    other.mapped_data_ = nullptr;
    other.mapped_size_ = 0;
    other.offset_ = 0;
    other.batches_left_ = 0;
    other.index_of_batch = 0;
    return *this;
}

void BelZReader::OpenMmap(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        throw std::runtime_error("File not found: " + filePath);
    }
    fd_ = open(filePath.c_str() , O_RDONLY);
    if (fd_ == -1) {
        throw std::runtime_error("Failed to open BelZ file: " + filePath + ": " + std::string(strerror(errno)));
    }

    struct stat st {};
    if (fstat(fd_ , &st) != 0) {
        const std::string error = strerror(errno);
        CloseMmap();
        throw std::runtime_error("Failed to stat BelZ file: " + filePath + ": " + error);
    }
    if (st.st_size <= 0) {
        CloseMmap();
        throw std::runtime_error("BelZ file is empty: " + filePath);
    }
    mapped_size_ = static_cast<size_t>(st.st_size);
    void* mapped = mmap(nullptr , mapped_size_ , PROT_READ , MAP_SHARED , fd_ , 0);
    if (mapped == MAP_FAILED) {
        const std::string error = strerror(errno);
        CloseMmap();
        throw std::runtime_error("Failed to mmap BelZ file: " + filePath + ": " + error);
    }
    mapped_data_ = static_cast<const uint8_t*>(mapped);
    offset_ = 0;
}

void BelZReader::CloseMmap() noexcept {
    if (mapped_data_ != nullptr) {
        munmap(const_cast<uint8_t*>(mapped_data_) , mapped_size_);
        mapped_data_ = nullptr;
    }
    mapped_size_ = 0;
    offset_ = 0;
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

void BelZReader::Seek(size_t offset) {
    offset_ = offset;
}

const uint8_t* BelZReader::ReadBytesView(size_t size) {
    const uint8_t* result = mapped_data_ + offset_;
    offset_ += size;
    return result;
}

void BelZReader::ReadBytes(void* out , size_t size) {
    if (size == 0) {
        return;
    }
    memcpy(out , ReadBytesView(size) , size);
}

bool BelZReader::Empty() const {
    return batches_left_ == 0;
}

std::shared_ptr<Column> BelZReader::ReadStringColumn(size_t size) {
    auto result = std::make_shared<StringColumn>();
    size_t data_sz = ReadPod<size_t>();
    size_t offset_sz = ReadPod<size_t>();
    result->ResizeData(data_sz);
    result->ResizeOffset(offset_sz);
    ReadBytes(result->GetDataPointer() , data_sz);
    ReadBytes(result->GetOffsetPointer() , offset_sz * sizeof(size_t));
    return result;
}

std::shared_ptr<Column> BelZReader::ReadDoubleColumn(size_t size) {
    return ReadFixedWidthColumn<double , DoubleColumn>(size);
}

std::shared_ptr<Column> BelZReader::ReadDateColumn(size_t size) {
    return ReadFixedWidthColumn<int64_t , DateColumn>(size);
}

std::shared_ptr<Column> BelZReader::ReadTimestampColumn(size_t size) {
    return ReadFixedWidthColumn<int64_t , TimeStampColumn>(size);
}

std::shared_ptr<Column> BelZReader::ReadColumn(size_t size , ColumnType type , ssize_t need_offset) {
    if (need_offset != -1) {
        Seek(static_cast<size_t>(need_offset));  
    }
    CodecType codec = ReadPod<CodecType>();
    size_t payload_size = ReadPod<size_t>();
    if (codec == CodecType::Raw) {
        const uint8_t* payload = ReadBytesView(payload_size);
        return ReadRawPayload(payload , size , type , payload_size);
    }
    const uint8_t* payload = ReadBytesView(payload_size);
    return Decoder::DecodeColumn(Decoder::EncodedColumnView{codec , payload , payload_size} , type);
}

void BelZReader::ReadMetaData() {
    if (mapped_size_ < sizeof(uint64_t)) {
        throw std::runtime_error("BelZ file is too small to contain metadata offset!");
    }
    Seek(mapped_size_ - sizeof(uint64_t));
    uint64_t meta_offset = ReadPod<uint64_t>();
    Seek(static_cast<size_t>(meta_offset));
    size_t col_count = ReadPod<size_t>();
    scheme_.Clear(); 
    meta_.Clear();
    for (size_t i = 0; i < col_count; ++i) {
        size_t len = ReadPod<size_t>();
        std::string name;
        name.resize(len);
        if (len > 0) {
            ReadBytes(name.data(), len);
        }
        uint8_t type = ReadPod<uint8_t>();
        scheme_.Push_Back(SchemeNode{name, static_cast<ColumnType>(type)});
    }
    meta_.SetScheme(Scheme(scheme_));
    batches_left_ = ReadPod<size_t>();
    for (size_t i = 0; i < batches_left_; ++i) {
        size_t offset = ReadPod<size_t>();
        meta_.AddBatchOffset(offset);
    }
    for (size_t i = 0; i < batches_left_; ++i) {
        size_t rows = ReadPod<size_t>();
        meta_.AddRows(rows);
    }
    for (size_t i = 0; i < batches_left_ * col_count; ++i) {
        size_t offset = ReadPod<size_t>();
        meta_.AddColumnOffset(offset);
    }
}

void BelZReader::ReadBatch(Batch& batch) {
    if (batches_left_ == 0) {
        throw std::runtime_error("Try to read Batch after end of file!");
    }
    batch.Clear();
    size_t current_offset = meta_.GetBatchOffset(index_of_batch);
    Seek(current_offset);
    batch.SetScheme(scheme_); 
    size_t rows_count = meta_.GetRow(index_of_batch);

    for (size_t column = 0; column < scheme_.Size(); ++column) {
        auto col_ptr = ReadColumn(rows_count, scheme_.GetType(column));
        batch.AddColumn(col_ptr);
    }
    --batches_left_;
    ++index_of_batch;
}

void BelZReader::ScanBatch(size_t index , Batch& batch) {
    if (index >= meta_.BatchesCount()) {
        throw std::runtime_error("Batch index out of range in ScanBatch: " + std::to_string(index));
    }
    batch.Clear();
    size_t current_offset = meta_.GetBatchOffset(index);
    Seek(current_offset);
    batch.SetScheme(scheme_); 
    size_t rows_count = meta_.GetRow(index);

    for (size_t column = 0; column < scheme_.Size(); ++column) {
        batch.AddColumn(ReadColumn(rows_count, scheme_.GetType(column)));
    }
}

void BelZReader::ReadBatch(Batch& batch , const std::vector<std::string>& column_names) {
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
        auto col_ptr = ReadColumn(index_of_batch , scheme_.GetIndex(column_names[column]));
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

std::shared_ptr<Column> BelZReader::ReadColumn(size_t batch_id , size_t column_id) {
    if (batch_id >= meta_.BatchesCount()) {
        throw std::runtime_error("Batch ID out of range in ReadColumn: " + std::to_string(batch_id));
    }
    if (column_id >= scheme_.Size()) {
        throw std::runtime_error("Column ID out of range in ReadColumn: " + std::to_string(column_id));
    }
    size_t offset = meta_.GetColumnOffset(batch_id, column_id);
    ColumnType type = scheme_.GetType(column_id);
    size_t rows = meta_.GetRow(batch_id);
    return ReadColumn(rows, type, offset);
}
