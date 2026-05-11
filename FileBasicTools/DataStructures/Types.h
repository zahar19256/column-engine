#pragma once
#include "Scheme.h"
#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

class Int8Column;
class Int16Column;
class Int32Column;
class Int64Column;
class StringColumn;

namespace Data {
    using FilterValue = std::variant<int64_t , double , std::string>;

    template <typename T>
    struct ColumnTraits;

    template <>
    struct ColumnTraits<int8_t> {
        using ColumnT = Int8Column;
        static constexpr ColumnType type = ColumnType::Int8;
    };

    template <>
    struct ColumnTraits<int16_t> {
        using ColumnT = Int16Column;
        static constexpr ColumnType type = ColumnType::Int16;
    };

    template <>
    struct ColumnTraits<int32_t> {
        using ColumnT = Int32Column;
        static constexpr ColumnType type = ColumnType::Int32;
    };

    template <>
    struct ColumnTraits<int64_t> {
        using ColumnT = Int64Column;
        static constexpr ColumnType type = ColumnType::Int64;
    };

    template <>
    struct ColumnTraits<std::string> {
        using ColumnT = StringColumn;
        static constexpr ColumnType type = ColumnType::String;
    };

    template <typename T>
    ColumnType GetColumnType() {
        if constexpr (std::is_same_v<T, int8_t>) {
            return ColumnType::Int8;
        }
        if constexpr (std::is_same_v<T, int16_t>) {
            return ColumnType::Int16;
        }
        if constexpr (std::is_same_v<T, int32_t>) {
            return ColumnType::Int32;
        }
        if constexpr (std::is_same_v<T, int64_t>) {
            return ColumnType::Int64;
        }
        if constexpr (std::is_same_v<T, double>) {
            return ColumnType::Double;
        }
        if constexpr (std::is_same_v<T, std::string>) {
            return ColumnType::String;
        }
        return ColumnType::Unknown;
    }

    inline int64_t ParseInt64(const std::string& val) {
        int64_t result = 0;
        auto [ptr , err] = std::from_chars(val.data(), val.data() + val.size(), result);
        if (err != std::errc() || ptr != val.data() + val.size()) {
            throw std::runtime_error("Cannot parse int64 from string!");
        }
        return result;
    }

    inline double ParseDouble(const std::string& val) {
        double result = 0;
        auto [ptr , err] = std::from_chars(val.data(), val.data() + val.size(), result);
        if (err != std::errc() || ptr != val.data() + val.size()) {
            throw std::runtime_error("Cannot parse double from string!");
        }
        return result;
    }

    inline int ParseSmallNumber(const std::string& val , size_t start , size_t size) {
        int result = 0;
        const char* begin = val.data() + start;
        const char* end = begin + size;
        auto [ptr , err] = std::from_chars(begin, end, result);
        if (err != std::errc() || ptr != end) {
            throw std::runtime_error("Cannot parse date/time from string!");
        }
        return result;
    }

    inline int64_t DaysFromZeroDate(int year , int month , int day) {
        year -= month <= 2;
        const int era = (year >= 0 ? year : year - 399) / 400;
        const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
        const unsigned day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
        return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(day_of_era);
    }

    inline int64_t ParseDate(const std::string& val) {
        if (val.size() != 10 || val[4] != '-' || val[7] != '-') {
            throw std::runtime_error("Date format must be YYYY-MM-DD!");
        }
        int year = ParseSmallNumber(val , 0 , 4);
        int month = ParseSmallNumber(val , 5 , 2);
        int day = ParseSmallNumber(val , 8 , 2);
        if (month < 1 || month > 12 || day < 1 || day > 31) {
            throw std::runtime_error("Date value is out of range!");
        }
        return DaysFromZeroDate(year , month , day) - DaysFromZeroDate(1970 , 1 , 1);
    }

    inline int64_t ParseDate(const std::string& start , size_t size) {
        if (size != 10 || start[4] != '-' || start[7] != '-') {
            throw std::runtime_error("Date format must be YYYY-MM-DD!");
        }
        int year = ParseSmallNumber(start , 0 , 4);
        int month = ParseSmallNumber(start , 5 , 2);
        int day = ParseSmallNumber(start , 8 , 2);
        if (month < 1 || month > 12 || day < 1 || day > 31) {
            throw std::runtime_error("Date value is out of range!");
        }
        return DaysFromZeroDate(year , month , day) - DaysFromZeroDate(1970 , 1 , 1);
    }

    inline int64_t ParseTimestamp(const std::string& val) {
        if (val.size() != 19 || val[10] != ' ' || val[13] != ':' || val[16] != ':') {
            throw std::runtime_error("Timestamp format must be YYYY-MM-DD HH:MM:SS!");
        }
        int64_t days = ParseDate(val.substr(0 , 10));
        int hours = ParseSmallNumber(val , 11 , 2);
        int minutes = ParseSmallNumber(val , 14 , 2);
        int seconds = ParseSmallNumber(val , 17 , 2);
        if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59 || seconds < 0 || seconds > 59) {
            throw std::runtime_error("Timestamp value is out of range!");
        }
        return days * 86400 + hours * 3600 + minutes * 60 + seconds;
    }

    template <typename T> 
    T ConvertDataFromString(const std::string& val) {
        if constexpr (std::is_same_v<T , std::string>) {
            return val;
        }
        if constexpr (std::is_same_v<T , double>) {
            return ParseDouble(val);
        }
        if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(ParseInt64(val));
        }
        throw std::runtime_error("Unsupported type for conversion from string");
    }

    inline FilterValue ConvertFilterValue(ColumnType type , const std::string& val) {
        switch (type) {
            case ColumnType::Int8:
            case ColumnType::Int16:
            case ColumnType::Int32:
            case ColumnType::Int64:
                return ParseInt64(val);
            case ColumnType::Int128:
                throw std::runtime_error("Int128 filter values are not supported yet!");
            case ColumnType::Double:
                return ParseDouble(val);
            case ColumnType::String:
                return std::string(val);
            case ColumnType::Date:
                return ParseDate(val);
            case ColumnType::Timestamp:
                return ParseTimestamp(val);
            case ColumnType::Unknown:
                throw std::runtime_error("Unknown type for filter value!");
        }
        throw std::runtime_error("Unsupported type for filter value!");
    }

}
