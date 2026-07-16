#pragma once
#include "GermanString.h"
#include "Scheme.h"
#include "Types.h"
#include "Utility.h"
#include <cstring>
#include <memory.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Герман стыдно за этот файлик
// тут короче да колонки и копирование кода много раз

class Column {
public:
    virtual void Reserve(size_t n) = 0;
    virtual void Resize(size_t n) = 0;
    virtual void Clear() = 0;
    virtual void AppendFromString(const char* start, size_t size) = 0;

    virtual size_t Size() const = 0;
    virtual ColumnType GetType() const = 0;
    virtual Utility::ScalarValue GetScalarValue(size_t index) const = 0;

    virtual ~Column() = default;
};

class StringColumn : public Column {
public:
    using ValueType = std::string;
    explicit StringColumn(Utility::StringArena* arena) : arena_(arena) {}
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    void Resize(size_t n) override {
        data_.resize(n, {});
    }
    ColumnType GetType() const override {
        return ColumnType::String;
    }
    void AppendFromString(const char* data, size_t size) override {
        if (size <= 12) {
            data_.emplace_back(data, size);
            return;
        }
        std::string_view now = arena_->Add(std::string_view(data, size));
        data_.emplace_back(now.data(), now.size());
    }
    Utility::ScalarValue GetScalarValue(size_t index) const override {
        return At(index);
    }
    void Push_Back(GermanStr&& value) {
        data_.push_back(value);
    }
    void Push_Back(const GermanStr& value) {
        data_.push_back(value);
    }
    template <typename... Args> GermanStr& Emplace_Back(Args&&... args) {
        return data_.emplace_back(std::forward<Args>(args)...);
    }

    void Assign(size_t n, const GermanStr& value) {
        Clear();
        data_.assign(n, value);
    }
    GermanStr* Data() {
        return data_.data();
    }
    const GermanStr* Data() const {
        return data_.data();
    }
    GermanStr At(size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of StringColumn range!");
        }
        return data_[index];
    }
    std::string_view At_view(size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of StringColumn range!");
        }
        return data_[index].View();
    }
    GermanStr operator[](size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of StringColumn range");
        }
        return data_[index];
    }
    size_t Size() const noexcept override {
        return data_.size();
    }
    void Clear() noexcept override {
        data_.clear();
    }

private:
    std::vector<GermanStr> data_;
    Utility::StringArena* arena_;
};

class FlatStringColumn : public Column {
public:
    using ValueType = std::string;
    void Reserve(size_t n) override {
        offsets_.reserve(n);
    }
    void Resize(size_t n) override {
        const size_t old_size = offsets_.size();
        offsets_.resize(n);
        for (size_t i = old_size; i < n; ++i) {
            offsets_[i] = data_.size();
        }
    }
    Utility::ScalarValue GetScalarValue(size_t index) const override {
        std::string_view now = At(index);
        return GermanStr(now.data(), now.size());
    }
    ColumnType GetType() const override {
        return ColumnType::FlatString;
    }
    void ReserveOffset(size_t n) {
        offsets_.reserve(n);
    }
    void ResizeOffset(size_t n) {
        offsets_.resize(n);
    }
    void ReserveData(size_t n) {
        data_.reserve(n);
    }
    void ResizeData(size_t n) {
        data_.resize(n);
    }
    void AppendFromString(const char* start, size_t size) override {
        data_.append(start, size);
        offsets_.push_back(data_.size());
    }
    void AppendFromString(const std::string& val) {
        data_.append(val);
        offsets_.push_back(data_.size());
    }
    void AppendFromRow(const char* data, size_t size) {
        data_.append(data, size);
        offsets_.push_back(data_.size());
    }
    void Push_Back(std::string&& value) {
        data_.append(std::move(value));
        offsets_.push_back(data_.size());
    }
    void Push_Back(const std::string& value) {
        data_.append(value);
        offsets_.push_back(data_.size());
    }
    void Assign(size_t n, const std::string& value) {
        Clear();
        data_.reserve(value.size() * n);
        offsets_.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            Push_Back(value);
        }
    }
    char* GetDataPointer() {
        return data_.data();
    }
    const char* GetDataPointer() const {
        return data_.data();
    }
    const std::string& GetData() const {
        return data_;
    }
    size_t GetDataSize() const {
        return data_.size();
    }
    std::string_view At(size_t index) const {
        if (index >= offsets_.size()) {
            throw std::runtime_error("Index is out of StringColumn range!");
        }
        size_t start = index == 0 ? 0 : offsets_[index - 1];
        size_t end = offsets_[index];
        return std::string_view(data_.data() + start, end - start);
    }
    std::string operator[](size_t index) const {
        if (index >= offsets_.size()) {
            throw std::runtime_error("Index is out of StringColumn range");
        }
        size_t size = offsets_[index];
        size_t start = 0;
        if (index > 0) {
            size -= offsets_[index - 1];
            start = offsets_[index - 1];
        }
        return data_.substr(start, size);
    }
    size_t* GetOffsetPointer() {
        return offsets_.data();
    }
    const size_t* GetOffsetPointer() const {
        return offsets_.data();
    }
    const char* GetStringPointer(size_t index) const {
        if (index >= offsets_.size()) {
            throw std::runtime_error("Index is out of StringColumn range");
        }
        size_t start = 0;
        if (index > 0) {
            start = offsets_[index - 1];
        }
        return data_.data() + start;
    }
    size_t GetStringSize(size_t index) const {
        if (index >= offsets_.size()) {
            throw std::runtime_error("Index is out of StringColumn range");
        }
        size_t size = offsets_[index];
        if (index > 0) {
            size -= offsets_[index - 1];
        }
        return size;
    }
    size_t Size() const noexcept override {
        return offsets_.size();
    }
    void Clear() noexcept override {
        data_.clear();
        offsets_.clear();
    }

private:
    std::string data_;
    std::vector<size_t> offsets_;
};

class Int64Column : public Column {
public:
    using ValueType = int64_t;
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    ColumnType GetType() const override {
        return ColumnType::Int64;
    }
    Utility::ScalarValue GetScalarValue(size_t index) const override {
        return At(index);
    }
    void Resize(size_t n) override {
        data_.resize(n);
    }
    void AppendFromString(const char* start, size_t size) override {
        data_.push_back(Data::ParseInteger<int64_t>(start, size));
    }
    void AppendFromString(std::string val) {
        data_.push_back(Data::ParseInteger<int64_t>(val));
    }
    void Push_Back(int64_t value) {
        data_.push_back(value);
    }
    void Assign(size_t n, int64_t value) {
        data_.assign(n, value);
    }
    int64_t* Data() noexcept {
        return data_.data();
    }
    const int64_t* Data() const noexcept {
        return data_.data();
    }
    int64_t At(size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    int64_t operator[](size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    size_t Size() const noexcept override {
        return data_.size();
    }
    void Clear() noexcept override {
        data_.clear();
    }

private:
    std::vector<int64_t> data_;
};

class DoubleColumn : public Column {
public:
    using ValueType = double;
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    ColumnType GetType() const override {
        return ColumnType::Double;
    }
    Utility::ScalarValue GetScalarValue(size_t index) const override {
        return At(index);
    }
    void Resize(size_t n) override {
        data_.resize(n);
    }
    void AppendFromString(const char* start, size_t size) override {
        std::string now;
        now.resize(size);
        memcpy(now.data(), start, size);
        data_.push_back(std::stold(now));
    }
    void AppendFromString(std::string val) {
        data_.push_back(std::stold(val));
    }
    void Push_Back(double value) {
        data_.push_back(value);
    }
    void Assign(size_t n, double value) {
        data_.assign(n, value);
    }
    double* Data() noexcept {
        return data_.data();
    }
    const double* Data() const noexcept {
        return data_.data();
    }
    double At(size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    double operator[](size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    size_t Size() const noexcept override {
        return data_.size();
    }
    void Clear() noexcept override {
        data_.clear();
    }

private:
    std::vector<double> data_;
};

// TODO переделать это на что-то шаблонное но потом после запросов!
class Int32Column : public Column {
public:
    using ValueType = int32_t;
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    ColumnType GetType() const override {
        return ColumnType::Int32;
    }
    Utility::ScalarValue GetScalarValue(size_t index) const override {
        return static_cast<int64_t>(At(index));
    }
    void Resize(size_t n) override {
        data_.resize(n);
    }
    void AppendFromString(const char* start, size_t size) override {
        data_.push_back(Data::ParseInteger<int32_t>(start, size));
    }
    void AppendFromString(std::string val) {
        data_.push_back(Data::ParseInteger<int32_t>(val));
    }
    void Push_Back(int32_t value) {
        data_.push_back(value);
    }
    void Assign(size_t n, int32_t value) {
        data_.assign(n, value);
    }
    int32_t* Data() noexcept {
        return data_.data();
    }
    const int32_t* Data() const noexcept {
        return data_.data();
    }
    int32_t At(size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    int32_t operator[](size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    size_t Size() const noexcept override {
        return data_.size();
    }
    void Clear() noexcept override {
        data_.clear();
    }

private:
    std::vector<int32_t> data_;
};

class Int16Column : public Column {
public:
    using ValueType = int16_t;
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    ColumnType GetType() const override {
        return ColumnType::Int16;
    }
    Utility::ScalarValue GetScalarValue(size_t index) const override {
        return static_cast<int64_t>(At(index));
    }
    void Resize(size_t n) override {
        data_.resize(n);
    }
    void AppendFromString(const char* start, size_t size) override {
        data_.push_back(Data::ParseInteger<int16_t>(start, size));
    }
    void AppendFromString(std::string val) {
        data_.push_back(Data::ParseInteger<int16_t>(val));
    }
    void Push_Back(int16_t value) {
        data_.push_back(value);
    }
    void Assign(size_t n, int16_t value) {
        data_.assign(n, value);
    }
    int16_t* Data() noexcept {
        return data_.data();
    }
    const int16_t* Data() const noexcept {
        return data_.data();
    }
    int16_t At(size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    int16_t operator[](size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    size_t Size() const noexcept override {
        return data_.size();
    }
    void Clear() noexcept override {
        data_.clear();
    }

private:
    std::vector<int16_t> data_;
};

class Int8Column : public Column {
public:
    using ValueType = int8_t;
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    ColumnType GetType() const override {
        return ColumnType::Int8;
    }
    Utility::ScalarValue GetScalarValue(size_t index) const override {
        return static_cast<int64_t>(At(index));
    }
    void Resize(size_t n) override {
        data_.resize(n);
    }
    void AppendFromString(const char* start, size_t size) override {
        data_.push_back(Data::ParseInteger<int8_t>(start, size));
    }
    void AppendFromString(std::string val) {
        data_.push_back(Data::ParseInteger<int8_t>(val));
    }
    void Push_Back(int8_t value) {
        data_.push_back(value);
    }
    void Assign(size_t n, int8_t value) {
        data_.assign(n, value);
    }
    int8_t* Data() noexcept {
        return data_.data();
    }
    const int8_t* Data() const noexcept {
        return data_.data();
    }
    int8_t At(size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    int8_t operator[](size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    size_t Size() const noexcept override {
        return data_.size();
    }
    void Clear() noexcept override {
        data_.clear();
    }

private:
    std::vector<int8_t> data_;
};

class Int128Column : public Column {
public:
    using ValueType = __int128_t;
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    ColumnType GetType() const override {
        return ColumnType::Int128;
    }
    Utility::ScalarValue GetScalarValue(size_t index) const override {
        return At(index);
    }
    void Resize(size_t n) override {
        data_.resize(n);
    }
    void AppendFromString(const char* start, size_t size) override {
        std::string now;
        now.resize(size);
        memcpy(now.data(), start, size);
        data_.push_back(std::stoll(now)); // TODO СДЕЛАТЬ РУЧНОЙ ПАРСЕР __INT128
    }
    void AppendFromString(std::string val) {
        data_.push_back(std::stoi(val)); // TODO СДЕЛАТЬ РУЧНОЙ ПАРСЕР __INT128
    }
    void Push_Back(__int128_t value) {
        data_.push_back(value);
    }
    void Assign(size_t n, __int128_t value) {
        data_.assign(n, value);
    }
    __int128_t* Data() noexcept {
        return data_.data();
    }
    const __int128_t* Data() const noexcept {
        return data_.data();
    }
    __int128_t At(size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    __int128_t operator[](size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of Int64Column range");
        }
        return data_[index];
    }
    size_t Size() const noexcept override {
        return data_.size();
    }
    void Clear() noexcept override {
        data_.clear();
    }

private:
    std::vector<__int128_t> data_;
};

class DateColumn : public Column {
public:
    using ValueType = int64_t;
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    void Resize(size_t n) override {
        data_.resize(n);
    }
    ColumnType GetType() const override {
        return ColumnType::Date;
    }
    Utility::ScalarValue GetScalarValue(size_t index) const override {
        return At(index);
    }
    void AppendFromString(const char* start, size_t size) override {
        data_.push_back(Data::ParseDate(start, size));
    }
    void AppendFromString(std::string val) {
        data_.push_back(Data::ParseDate(val));
    }
    void Push_Back(int64_t value) {
        data_.push_back(value);
    }
    void Assign(size_t n, int64_t value) {
        data_.assign(n, value);
    }
    int64_t* Data() noexcept {
        return data_.data();
    }
    const int64_t* Data() const noexcept {
        return data_.data();
    }
    int64_t At(size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of DateColumn range");
        }
        return data_[index];
    }
    int64_t operator[](size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of DateColumn range");
        }
        return data_[index];
    }
    size_t Size() const noexcept override {
        return data_.size();
    }
    void Clear() noexcept override {
        data_.clear();
    }

private:
    std::vector<int64_t> data_;
};

class TimeStampColumn : public Column {
public:
    using ValueType = int64_t;
    void Reserve(size_t n) override {
        data_.reserve(n);
    }
    void Resize(size_t n) override {
        data_.resize(n);
    }
    ColumnType GetType() const override {
        return ColumnType::Timestamp;
    }
    Utility::ScalarValue GetScalarValue(size_t index) const override {
        return At(index);
    }
    void AppendFromString(const char* start, size_t size) override {
        std::string now;
        now.resize(size);
        memcpy(now.data(), start, size);
        data_.push_back(Data::ParseTimestamp(now));
    }
    void AppendFromString(std::string val) {
        data_.push_back(Data::ParseTimestamp(val));
    }
    void Push_Back(int64_t value) {
        data_.push_back(value);
    }
    void Assign(size_t n, int64_t value) {
        data_.assign(n, value);
    }
    int64_t* Data() noexcept {
        return data_.data();
    }
    const int64_t* Data() const noexcept {
        return data_.data();
    }
    int64_t At(size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of DateColumn range");
        }
        return data_[index];
    }
    int64_t operator[](size_t index) const {
        if (index >= data_.size()) {
            throw std::runtime_error("Index is out of DateColumn range");
        }
        return data_[index];
    }
    size_t Size() const noexcept override {
        return data_.size();
    }
    void Clear() noexcept override {
        data_.clear();
    }

private:
    std::vector<int64_t> data_;
};

inline std::shared_ptr<Column> MakeColumn(ColumnType type, std::optional<Utility::StringArena*> arena) {
    switch (type) {
    case ColumnType::Int8:
        return std::make_shared<Int8Column>();
    case ColumnType::Int16:
        return std::make_shared<Int16Column>();
    case ColumnType::Int32:
        return std::make_shared<Int32Column>();
    case ColumnType::Int64:
        return std::make_shared<Int64Column>();
    case ColumnType::Int128:
        return std::make_shared<Int128Column>();
    case ColumnType::String:
        return std::make_shared<StringColumn>(arena.value());
    case ColumnType::FlatString:
        return std::make_shared<FlatStringColumn>();
    case ColumnType::Double:
        return std::make_shared<DoubleColumn>();
    case ColumnType::Date:
        return std::make_shared<DateColumn>();
    case ColumnType::Timestamp:
        return std::make_shared<TimeStampColumn>();
    case ColumnType::Unknown:
        throw std::runtime_error("Unknown output column type!");
    }
    throw std::runtime_error("Unsupported output column type!");
}

template <typename T> inline std::shared_ptr<T> As(const std::shared_ptr<Column>& obj) {
    return std::dynamic_pointer_cast<T>(obj);
}

template <typename T> inline bool Is(const std::shared_ptr<Column>& obj) {
    return dynamic_cast<T*>(obj.get()) != nullptr;
}
