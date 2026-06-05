#pragma once 
#include <vector>
#include <string>
#include <memory>
#include <memory.h>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <variant>
#include "GermanString.h"

namespace Utility {

using Element_view = std::variant<int64_t , std::string_view , double>;
using ScalarValue = std::variant<int64_t , GermanStr , double , __int128_t>;
using GroupKey = std::vector<ScalarValue>;

class StringArena {
public:
    StringArena(size_t block_size = (1 << 20)) : block_size_(block_size) {

    }
    StringArena(const StringArena&) = delete;
    StringArena& operator=(const StringArena&) = delete;
    StringArena(StringArena&&) noexcept = default;
    StringArena& operator=(StringArena&&) noexcept = default;

    std::string_view Add(std::string_view s) {
        if (s.empty()) {
            return std::string_view(empty_string_ptr_, 0);
        }
        char* ptr = Allocate(s.size());
        memcpy(ptr, s.data(), s.size());
        return std::string_view(ptr, s.size());
    }
    size_t MemoryUsed() const {
        return memory_used_;
    }
    void Clear() {
        blocks_.clear();
        current_block_ = nullptr;
        current_capacity_ = 0;
        current_offset_ = 0;
        memory_used_ = 0;
    }

private:
    char* Allocate(size_t size) {
        if (current_block_ == nullptr || current_offset_ + size > current_capacity_) {
            AllocateNewBlock(size);
        }
        char* ptr = current_block_ + current_offset_;
        current_offset_ += size;
        return ptr;
    }

    void AllocateNewBlock(size_t required_size) {
        size_t new_block_size = std::max(block_size_, required_size);
        auto block = std::make_unique<char[]>(new_block_size);
        current_block_ = block.get();
        current_capacity_ = new_block_size;
        current_offset_ = 0;
        blocks_.push_back(std::move(block));
        memory_used_ += new_block_size;
    }
    static inline char empty_string_storage_ = '\0';
    static inline char* empty_string_ptr_ = &empty_string_storage_;
    size_t block_size_;
    std::vector<std::unique_ptr<char[]>> blocks_;
    char* current_block_ = nullptr;
    size_t current_capacity_ = 0;
    size_t current_offset_ = 0;
    size_t memory_used_ = 0;
};

class GroupHash {
public:
    size_t operator()(const GroupKey& key) const {
        uint64_t hash = 0;
        for (const auto& value : key) {
            AddElement(hash , value);
        }
        return static_cast<size_t>(hash);
    }

private:
    static void AddElement(uint64_t& hash , const ScalarValue& value) { // TODO аккуратно тут копирование строк, потом надо будет привязать арену строк
        std::visit([&hash](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                size_t value_hash = std::hash<int64_t>{}(item);
                Combine(hash , value_hash ^ hash_integer_state_);
            } else if constexpr (std::is_same_v<T, std::string>) {
                size_t value_hash = std::hash<std::string>{}(item);
                Combine(hash , value_hash ^ hash_string_state_);
            } else if constexpr (std::is_same_v<T, double>) {
                Combine(hash , HashDouble(item) ^ hash_double_state_);
            } else if constexpr (std::is_same_v<T, __int128_t>) {
                Combine(hash , HashInt128(item) ^ hash_int128_state_);
            }
        }, value);
    }
    static inline uint64_t HashDouble(double x) {
        if (x == 0.0) {
            x = 0.0;
        }
        uint64_t bits;
        memcpy(&bits, &x, sizeof(x));
        return std::hash<uint64_t>{}(bits);
    }
    static uint64_t HashInt128(__int128_t value) {
        uint64_t low = static_cast<uint64_t>(value);
        uint64_t high = static_cast<uint64_t>(value >> 64);
        uint64_t hash = std::hash<uint64_t>{}(low);
        Combine(hash , std::hash<uint64_t>{}(high));
        return hash;
    }
    static void Combine(uint64_t& prev_hash , uint64_t current_hash) {
        prev_hash ^= current_hash + 0x9e3779b97f4a7c15ULL + (prev_hash << 6) + (prev_hash >> 2);
    }
    static constexpr uint64_t hash_string_state_  = 0x243f6a8885a308d3ULL;
    static constexpr uint64_t hash_integer_state_ = 0x13198a2e03707344ULL;
    static constexpr uint64_t hash_double_state_  = 0xa4093822299f31d0ULL;
    static constexpr uint64_t hash_int128_state_  = 0x082efa98ec4e6c89ULL;
};

} // namespace Utility
