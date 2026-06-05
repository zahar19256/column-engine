#include <GermanString.h>

size_t GermanStr::Size() const noexcept {
    return (high_ & ((1ll << 32) - 1));
}

const char* GermanStr::Data() const noexcept {
    if (Size() <= 12) {
        return reinterpret_cast<const char*>(this) + 4;
    } else {
        return reinterpret_cast<char*>(low_);
    }
}

uint32_t GermanStr::GetPref() const noexcept {
    return high_ >> 32;
}


