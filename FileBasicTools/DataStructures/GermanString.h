#pragma once
#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <string.h>
#include <string_view>

class GermanStr {
public:
    GermanStr() {
        low_ = 0 , high_ = 0;
    }
    GermanStr(const char* start , size_t size) {
        if (size <= 12) {
            high_ = static_cast<uint32_t>(size);
            low_ = 0;
            memcpy(reinterpret_cast<char*>(this) + 4 , start , size);
        } else {
            uint32_t prefix = 0;
            memcpy(&prefix, start, 4);
            high_ = static_cast<uint32_t>(size) | (static_cast<uint64_t>(prefix) << 32);
            low_ = reinterpret_cast<uint64_t>(start);
        }
    }

    GermanStr(GermanStr&& other) : high_(std::move(other.high_)) , low_(std::move(other.low_)) {
    }
    GermanStr(const GermanStr& other) = default;

    GermanStr& operator=(const GermanStr& other) = default;

    inline bool operator>(const GermanStr& other) const noexcept {
        size_t size = Size();
        size_t other_size = other.Size();
        size_t mn_size = std::min(size , other_size);
        if (mn_size >= 4) {
            uint32_t prefix = GetPref();
            uint32_t other_prefix = other.GetPref();
            if (prefix != other_prefix) {
                return __builtin_bswap32(prefix) > __builtin_bswap32(other_prefix);
            }
            mn_size -= 4;
            if (mn_size != 0) {
                int compare_result = memcmp(Data() + 4 , other.Data() + 4 , mn_size);
                if (compare_result != 0) {
                    return compare_result > 0;
                }
            }
            return size > other_size;
        }
        if (mn_size != 0) {
            int compare_result = memcmp(Data() , other.Data() , mn_size);
            if (compare_result != 0) {
                return compare_result > 0;
            }
        }
        return size > other_size;
    }
    inline bool operator<=(const GermanStr& other) const noexcept {
        size_t size = Size();
        size_t other_size = other.Size();
        size_t mn_size = std::min(size , other_size);
        if (mn_size >= 4) {
            uint32_t prefix = GetPref();
            uint32_t other_prefix = other.GetPref();
            if (prefix != other_prefix) {
                return __builtin_bswap32(prefix) < __builtin_bswap32(other_prefix);
            }
            mn_size -= 4;
            if (mn_size != 0) {
                int compare_result = memcmp(Data() + 4 , other.Data() + 4 , mn_size);
                if (compare_result != 0) {
                    return compare_result < 0;
                }
            }
            return size <= other_size;
        }
        if (mn_size != 0) {
            int compare_result = memcmp(Data() , other.Data() , mn_size);
            if (compare_result != 0) {
                return compare_result < 0;
            }
        }
        return size <= other_size;
    }
    inline bool operator>=(const GermanStr& other) const noexcept {
        size_t size = Size();
        size_t other_size = other.Size();
        size_t mn_size = std::min(size , other_size);
        if (mn_size >= 4) {
            uint32_t prefix = GetPref();
            uint32_t other_prefix = other.GetPref();
            if (prefix != other_prefix) {
                return __builtin_bswap32(prefix) > __builtin_bswap32(other_prefix);
            }
            mn_size -= 4;
            if (mn_size != 0) {
                int compare_result = memcmp(Data() + 4 , other.Data() + 4 , mn_size);
                if (compare_result != 0) {
                    return compare_result > 0;
                }
            }
            return size >= other_size;
        }
        if (mn_size != 0) {
            int compare_result = memcmp(Data() , other.Data() , mn_size);
            if (compare_result != 0) {
                return compare_result > 0;
            }
        }
        return size >= other_size;
    }
    inline bool operator==(const GermanStr& other) const noexcept {
        if (high_ != other.high_) {
            return false;
        }
        size_t sz = Size();
        if (sz <= 12) {
            return low_ == other.low_;
        }
        if (GetPref() != other.GetPref()) {
            return false;
        }
        return memcmp(Data() , other.Data() , sz) == 0;
    }
    inline bool operator<(const GermanStr& other) const noexcept {
        size_t size = Size();
        size_t other_size = other.Size();
        size_t mn_size = std::min(size , other_size);
        if (mn_size >= 4) {
            uint32_t prefix = GetPref();
            uint32_t other_prefix = other.GetPref();
            if (prefix != other_prefix) {
                return __builtin_bswap32(prefix) < __builtin_bswap32(other_prefix);
            }
            mn_size -= 4;
            if (mn_size != 0) {
                int compare_result = memcmp(Data() + 4 , other.Data() + 4 , mn_size);
                if (compare_result != 0) {
                    return compare_result < 0;
                }
            }
            return size < other_size;
        }
        if (mn_size != 0) {
            int compare_result = memcmp(Data() , other.Data() , mn_size);
            if (compare_result != 0) {
                return compare_result < 0;
            }
        }
        return size < other_size;
    }

    size_t Size() const noexcept;
    const char* Data() const noexcept;
    uint32_t GetPref() const noexcept;

    std::string_view View() const {
        size_t sz = Size();
        if (sz <= 12) {
            return std::string_view(reinterpret_cast<const char*>(this) + 4 , sz);
        } else {
            return std::string_view(reinterpret_cast<const char*>(low_) , sz);
        }
    }

private:
    uint64_t high_;
    uint64_t low_;
};
