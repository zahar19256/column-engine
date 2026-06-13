#include "CsvWriter.h"
#include "Column.h"
#include <array>
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <charconv>
#include <filesystem>
#include <limits>
#include <stdexcept>

// тут я когда подключал все запросы кодекса натравил добавить все типы колонок
// так 

namespace {

void AppendPadded(std::string& out, int value, size_t width) {
    std::array<char, 16> buffer{};
    auto [ptr, err] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (err != std::errc()) {
        throw std::runtime_error("Failed to format padded integer!");
    }
    const size_t size = static_cast<size_t>(ptr - buffer.data());
    if (size < width) {
        out.append(width - size, '0');
    }
    out.append(buffer.data(), size);
}

void AppendInt128(std::string& out, __int128_t value) {
    if (value == 0) {
        out.push_back('0');
        return;
    }

    if (value < 0) {
        out.push_back('-');
    }

    std::array<char, 48> buffer{};
    size_t size = 0;
    while (value != 0) {
        int digit = static_cast<int>(value % 10);
        if (digit < 0) {
            digit = -digit;
        }
        buffer[size++] = static_cast<char>('0' + digit);
        value /= 10;
    }
    while (size != 0) {
        out.push_back(buffer[--size]);
    }
}

void AppendDate(std::string& out, int64_t days) {
    days += 719468;
    const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned day_of_era = static_cast<unsigned>(days - era * 146097);
    const unsigned year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    int year = static_cast<int>(year_of_era) + static_cast<int>(era) * 400;
    const unsigned day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    const unsigned month_part = (5 * day_of_year + 2) / 153;
    const unsigned day = day_of_year - (153 * month_part + 2) / 5 + 1;
    const int month = static_cast<int>(month_part) + (month_part < 10 ? 3 : -9);
    year += month <= 2;

    if (year < 0) {
        out.push_back('-');
        year = -year;
    }
    AppendPadded(out, year, 4);
    out.push_back('-');
    AppendPadded(out, month, 2);
    out.push_back('-');
    AppendPadded(out, static_cast<int>(day), 2);
}

void AppendTimestamp(std::string& out, int64_t timestamp) {
    int64_t days = timestamp / 86400;
    int64_t seconds = timestamp % 86400;
    if (seconds < 0) {
        seconds += 86400;
        --days;
    }

    const int hours = static_cast<int>(seconds / 3600);
    seconds %= 3600;
    const int minutes = static_cast<int>(seconds / 60);
    seconds %= 60;

    AppendDate(out, days);
    out.push_back(' ');
    AppendPadded(out, hours, 2);
    out.push_back(':');
    AppendPadded(out, minutes, 2);
    out.push_back(':');
    AppendPadded(out, static_cast<int>(seconds), 2);
}

void CheckColumnSize(const std::shared_ptr<Column>& column, size_t rows) {
    if (!column) {
        throw std::runtime_error("CSVWriter can't write null column!");
    }
    if (column->Size() != rows) {
        throw std::runtime_error("CSVWriter column size mismatch!");
    }
}

} // namespace

CSVWriter::CSVWriter(const std::string& filePath)
    : CSVWriter(std::filesystem::path(filePath), PathMode::AddUpdSuffix) {}

CSVWriter::CSVWriter(const std::filesystem::path& filePath, PathMode mode) : CSVWriter(filePath, mode, false) {}

CSVWriter::CSVWriter(const std::filesystem::path& filePath, PathMode mode, bool write_header)
    : write_header_(write_header) {
    const std::filesystem::path output_path = MakeOutputPath(filePath, mode);
    fout_.open(output_path, std::ios::out | std::ios::binary);
    if (!fout_.is_open()) {
        throw std::runtime_error("Failed to create file: " + output_path.string());
    }
    buffer_.reserve(kFlushLimit + 4096);
}

CSVWriter::~CSVWriter() {
    Flush();
}

std::filesystem::path CSVWriter::MakeOutputPath(const std::filesystem::path& filePath, PathMode mode) {
    if (mode == PathMode::ExactPath) {
        return filePath;
    }
    return filePath.parent_path() / (filePath.stem().string() + "upd.csv");
}

void CSVWriter::Flush() {
    if (!buffer_.empty()) {
        fout_.write(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        buffer_.clear();
    }
}

size_t CSVWriter::Rows() const {
    return rows_;
}

void CSVWriter::WriteRaw(std::string_view data) {
    if (data.size() > kFlushLimit) {
        Flush();
        fout_.write(data.data(), static_cast<std::streamsize>(data.size()));
        return;
    }
    if (buffer_.size() + data.size() > kFlushLimit) {
        Flush();
    }
    buffer_.append(data);
}

void CSVWriter::WriteChar(char value) {
    if (buffer_.size() == kFlushLimit) {
        Flush();
    }
    buffer_.push_back(value);
}

void CSVWriter::WriteDelimetr(char delimetr) {
    WriteChar(delimetr);
}

void CSVWriter::WriteInt64(int64_t data) {
    std::array<char, 32> buffer{};
    auto [ptr, err] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), data);
    if (err != std::errc()) {
        throw std::runtime_error("Failed to format int64!");
    }
    WriteRaw(std::string_view(buffer.data(), static_cast<size_t>(ptr - buffer.data())));
}

void CSVWriter::WriteInt128(__int128_t data) {
    AppendInt128(buffer_, data);
    if (buffer_.size() > kFlushLimit) {
        Flush();
    }
}

void CSVWriter::WriteDouble(double data) {
    std::array<char, 64> buffer{};
    auto [ptr, err] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), data, std::chars_format::general,
                                    std::numeric_limits<double>::max_digits10);
    WriteRaw(std::string_view(buffer.data(), static_cast<size_t>(ptr - buffer.data())));
}

void CSVWriter::WriteString(std::string_view data) {
    bool needs_quotes = false;
    for (char item : data) {
        if (item == delimiter_ || item == '\n' || item == '"' || item == '\r') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) {
        WriteRaw(data);
        return;
    }

    WriteChar('"');
    size_t chunk_start = 0;
    for (size_t index = 0; index < data.size(); ++index) {
        if (data[index] != '"') {
            continue;
        }
        WriteRaw(data.substr(chunk_start, index - chunk_start));
        WriteRaw("\"\"");
        chunk_start = index + 1;
    }
    WriteRaw(data.substr(chunk_start));
    WriteChar('"');
}

void CSVWriter::WriteDate(int64_t days) {
    AppendDate(buffer_, days);
    if (buffer_.size() > kFlushLimit) {
        Flush();
    }
}

void CSVWriter::WriteTimestamp(int64_t timestamp) {
    AppendTimestamp(buffer_, timestamp);
    if (buffer_.size() > kFlushLimit) {
        Flush();
    }
}

void CSVWriter::WriteValue(const std::shared_ptr<Column>& column, ColumnType type, size_t row) {
    switch (type) {
    case ColumnType::Int8:
        WriteInt64(static_cast<int64_t>(static_cast<const Int8Column*>(column.get())->At(row)));
        return;
    case ColumnType::Int16:
        WriteInt64(static_cast<int64_t>(static_cast<const Int16Column*>(column.get())->At(row)));
        return;
    case ColumnType::Int32:
        WriteInt64(static_cast<int64_t>(static_cast<const Int32Column*>(column.get())->At(row)));
        return;
    case ColumnType::Int64:
        WriteInt64(static_cast<const Int64Column*>(column.get())->At(row));
        return;
    case ColumnType::Int128:
        WriteInt128(static_cast<const Int128Column*>(column.get())->At(row));
        return;
    case ColumnType::Double:
        WriteDouble(static_cast<const DoubleColumn*>(column.get())->At(row));
        return;
    case ColumnType::String:
        WriteString(static_cast<const StringColumn*>(column.get())->At_view(row));
        return;
    case ColumnType::FlatString:
        WriteString(static_cast<const FlatStringColumn*>(column.get())->At(row));
        return;
    case ColumnType::Date:
        WriteDate(static_cast<const DateColumn*>(column.get())->At(row));
        return;
    case ColumnType::Timestamp:
        WriteTimestamp(static_cast<const TimeStampColumn*>(column.get())->At(row));
        return;
    case ColumnType::Unknown:
        break;
    }
    throw std::runtime_error("Unknown column type in CSVWriter!");
}

void CSVWriter::WriteHeader(const Batch& batch) {
    if (header_written_) {
        return;
    }
    for (size_t column = 0; column < batch.Size(); ++column) {
        if (column != 0) {
            WriteChar(delimiter_);
        }
        WriteString(batch.GetScheme().GetName(column));
    }
    if (batch.Size() != 0) {
        WriteChar('\n');
    }
    header_written_ = true;
}

void CSVWriter::WriteBatchWithHeader(const Batch& batch) {
    if (!header_written_) {
        WriteHeader(batch);
    }
    WriteBatch(batch);
}

void CSVWriter::WriteBatch(const Batch& batch) {
    if (write_header_ && !header_written_) {
        WriteHeader(batch);
    }
    if (batch.Empty()) {
        return;
    }
    size_t columns = batch.Size();
    size_t rows = batch.GetRows();
    const boost::dynamic_bitset<>& mask = batch.GetMsk();
    bool use_mask = !mask.empty() && mask.count() != mask.size();
    std::vector<std::shared_ptr<Column>> storages;
    std::vector<ColumnType> types;
    storages.reserve(columns);
    types.reserve(columns);
    for (size_t column = 0; column < columns; ++column) {
        auto storage = batch.GetColumn(column);
        CheckColumnSize(storage, rows);
        storages.push_back(std::move(storage));
        types.push_back(batch.GetType(column));
    }
    for (size_t row = 0; row < rows; ++row) {
        if (use_mask && !mask[row]) {
            continue;
        }
        for (size_t column = 0; column < columns; ++column) {
            if (column != 0) {
                WriteChar(delimiter_);
            }
            WriteValue(storages[column], types[column], row);
        }
        WriteChar('\n');
        ++rows_;
    }
}
