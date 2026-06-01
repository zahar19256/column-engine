#pragma once
#include "Batch.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

class CSVWriter {
public:
    enum class PathMode {
        AddUpdSuffix,
        ExactPath,
    };

    explicit CSVWriter(const std::string& filePath);
    explicit CSVWriter(const std::filesystem::path& filePath, PathMode mode = PathMode::AddUpdSuffix);
    CSVWriter(const std::filesystem::path& filePath, PathMode mode, bool write_header);
    ~CSVWriter();

    CSVWriter(const CSVWriter&) = delete;
    CSVWriter& operator=(const CSVWriter&) = delete;

    void WriteDelimetr(char delimetr = ',');
    void WriteInt64(int64_t data);
    void WriteString(std::string_view data);
    template <typename T>
    void WriteData(const T& data , ColumnType type) {
        if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            if (type == ColumnType::Int8 || type == ColumnType::Int16 ||
                type == ColumnType::Int32 || type == ColumnType::Int64 ||
                type == ColumnType::Date || type == ColumnType::Timestamp) {
                WriteInt64(static_cast<int64_t>(data));
                return;
            }
        }
        if constexpr (std::is_same_v<T, __int128_t>) {
            if (type == ColumnType::Int128) {
                WriteInt128(data);
                return;
            }
        }
        if constexpr (std::is_floating_point_v<T>) {
            if (type == ColumnType::Double) {
                WriteDouble(static_cast<double>(data));
                return;
            }
        }
        if constexpr (std::is_convertible_v<T, std::string_view>) {
            if (type == ColumnType::String) {
                WriteString(std::string_view(data));
                return;
            }
        }
        throw std::runtime_error("Try to write unknown type" + std::to_string(static_cast<uint8_t>(type)));
    }

    void WriteDouble(double data);
    void WriteInt128(__int128_t data);
    void WriteBatch(const Batch& batch);
    void WriteBatchWithHeader(const Batch& batch);
    void WriteHeader(const Batch& batch);
    void Flush();
    size_t Rows() const;

private:
    static std::filesystem::path MakeOutputPath(const std::filesystem::path& filePath, PathMode mode);
    void WriteRaw(std::string_view data);
    void WriteChar(char value);
    void WriteValue(const std::shared_ptr<Column>& column, ColumnType type, size_t row);
    void WriteDate(int64_t days);
    void WriteTimestamp(int64_t timestamp);

    std::ofstream fout_;
    std::string buffer_;
    char delimiter_ = ',';
    bool write_header_ = false;
    bool header_written_ = false;
    size_t rows_ = 0;
    static constexpr size_t kFlushLimit = 1 << 20;
};
