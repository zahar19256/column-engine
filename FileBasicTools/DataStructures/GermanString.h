#pragma once
#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <string>
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

    inline int Compare(std::string_view other) const noexcept {
        size_t size = Size();
        size_t other_size = other.size();
        size_t mn_size = std::min(size, other_size);
        if (mn_size >= 4) {
            uint32_t prefix = GetPref();
            uint32_t other_prefix;
            std::memcpy(&other_prefix, other.data(), 4);
            if (prefix != other_prefix) {
                return __builtin_bswap32(prefix) < __builtin_bswap32(other_prefix) ? -1 : 1;
            }
            if (mn_size > 4) {
                int compare_result = std::memcmp(Data() + 4, other.data() + 4, mn_size - 4);
                if (compare_result != 0) {
                    return compare_result;
                }
            }
        } else if (mn_size > 0) {
            int compare_result = std::memcmp(Data(), other.data(), mn_size);
            if (compare_result != 0) {
                return compare_result;
            }
        }
        if (size < other_size) {
            return -1;
        }
        if (size > other_size) {
            return 1;
        }
        return 0;
    }

    inline bool operator<(std::string_view other) const noexcept {
        return Compare(other) < 0;
    }
    inline bool operator>(std::string_view other) const noexcept {
        return Compare(other) > 0;
    }
    inline bool operator<=(std::string_view other) const noexcept {
        return Compare(other) <= 0;
    }
    inline bool operator>=(std::string_view other) const noexcept {
        return Compare(other) >= 0;
    }

    inline bool operator==(std::string_view other) const noexcept {
        size_t sz = Size();
        if (sz != other.size()) {
            return false;
        }
        if (sz >= 4) {
            uint32_t prefix = GetPref();
            uint32_t other_prefix;
            std::memcpy(&other_prefix, other.data(), 4);
            if (prefix != other_prefix) {
                return false;
            }
            return std::memcmp(Data() + 4, other.data() + 4, sz - 4) == 0;
        }
        return std::memcmp(Data(), other.data(), sz) == 0;
    }

    inline bool operator!=(std::string_view other) const noexcept {
        return !(*this == other);
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

inline bool operator<(std::string_view lhs, const GermanStr& rhs) noexcept {
    return rhs.Compare(lhs) > 0;
}
inline bool operator>(std::string_view lhs, const GermanStr& rhs) noexcept {
    return rhs.Compare(lhs) < 0;
}
inline bool operator<=(std::string_view lhs, const GermanStr& rhs) noexcept {
    return rhs.Compare(lhs) >= 0;
}
inline bool operator>=(std::string_view lhs, const GermanStr& rhs) noexcept {
    return rhs.Compare(lhs) <= 0;
}
inline bool operator==(std::string_view lhs, const GermanStr& rhs) noexcept {
    return rhs == lhs;
}
inline bool operator!=(std::string_view lhs, const GermanStr& rhs) noexcept {
    return !(rhs == lhs);
}
