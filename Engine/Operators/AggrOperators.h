#pragma once
#include "../../FileBasicTools/DataStructures/Utility.h"
#include "../../FileBasicTools/DataStructures/Types.h"
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include "Row.h"
#include "absl/container/flat_hash_set.h"

namespace Aggregation {

    template <typename T>
    concept StringType = std::same_as<T, std::string> ||
                     std::same_as<T, std::string_view> ||
                     std::same_as<T, const char*> ||
                     std::same_as<T, char*>;

    template <typename T>
    struct SumOperator {
        using StateType = __int128_t;
        using ResultType = int64_t;

        static StateType Init() {
            return 0;
        }

        template <typename ColumnType>
        static inline void UpdateFull(StateType& state , const ColumnType& column) {
            const T* data = column->Data();
            for (size_t i = 0; i < column->Size(); ++i) {
                state += data[i];
            }
        }

        template <typename ColumnType>
        static inline void UpdateFiltered(StateType& state , const ColumnType& column , const boost::dynamic_bitset<>& mask) {
            if (mask.size() != column->Size()) {
                throw std::runtime_error("SumOperator mask size mismatch!");
            }
            const T* data = column->Data();
            for (size_t i = 0; i < column->Size(); ++i) {
                state += data[i] * mask[i];
            }
        }

        template <typename ColumnType>
        static inline void UpdateRow(StateType& state , const ColumnType& column , size_t row) {
            state += column->At(row);
        }

        static inline ResultType Finalize(const StateType& state) {
            return static_cast<ResultType>(state);
        }

        static ColumnType FinalType() {
            return Data::ColumnTraits<T>::type;
        }
    };

    template <typename T>
    struct MinOperator {
        struct StateType {
            T result;
            bool has_value = false;
        };
        using ResultType = T;
        
        static StateType Init() {
            return {};
        }

        template <typename ColumnType>
        static inline void UpdateFull(StateType& state , const ColumnType& column) {
            const T* data = column->Data();
            for (size_t i = 0; i < column->Size(); ++i) {
                if (!state.has_value || (state.has_value && state.result > data[i])) {
                    state.has_value = true;
                    state.result = data[i];
                }
            }
        }

        template <typename ColumnType>
        static inline void UpdateFiltered(StateType& state , const ColumnType& column , const boost::dynamic_bitset<>& mask) {
            if (mask.size() != column->Size()) {
                throw std::runtime_error("MinOperator mask size mismatch!");
            }
            const T* data = column->Data();
            for (size_t i = 0; i < column->Size(); ++i) {
                if (mask[i] && (!state.has_value || (state.has_value && state.result > data[i]))) {
                    state.has_value = true;
                    state.result = data[i];
                }
            }
        }

        template <typename ColumnType>
        static inline void UpdateRow(StateType& state , const ColumnType& column , size_t row) {
            const T value = column->At(row);
            if (!state.has_value || state.result > value) {
                state.has_value = true;
                state.result = value;
            }
        }

        static inline ResultType Finalize(const StateType& state) {
            if (!state.has_value) {
                throw std::runtime_error("MinOperator dont have value to Finalize!");
            }
            return static_cast<ResultType>(state.result);
        }

        static ColumnType FinalType() {
            return Data::ColumnTraits<T>::type;
        }
    };

    template <StringType T>
    struct MinOperator<T> {
        struct StateType {
            T result;
            bool has_value = false;
        };
        using ResultType = T;
        
        static StateType Init() {
            return {};
        }

        template <typename ColumnType>
        static inline void UpdateFull(StateType& state , const ColumnType& column) {
            for (size_t i = 0; i < column->Size(); ++i) {
                if (!state.has_value || (state.has_value && state.result > column->At(i))) {
                    state.has_value = true;
                    state.result = column->At(i);
                }
            }
        }

        template <typename ColumnType>
        static inline void UpdateFiltered(StateType& state , const ColumnType& column , const boost::dynamic_bitset<>& mask) {
            if (mask.size() != column->Size()) {
                throw std::runtime_error("MinOperator mask size mismatch!");
            }
            for (size_t i = 0; i < column->Size(); ++i) {
                if (mask[i] && (!state.has_value || (state.has_value && state.result > column->At(i)))) {
                    state.has_value = true;
                    state.result = column->At(i);
                }
            }
        }

        template <typename ColumnType>
        static inline void UpdateRow(StateType& state , const ColumnType& column , size_t row) {
            const T value(column->At(row));
            if (!state.has_value || state.result > value) {
                state.has_value = true;
                state.result = value;
            }
        }

        static inline ResultType Finalize(const StateType& state) {
            if (!state.has_value) {
                throw std::runtime_error("MinOperator dont have value to Finalize!");
            }
            return static_cast<ResultType>(state.result);
        }

        static ColumnType FinalType() {
            return Data::ColumnTraits<T>::type;
        }
    };
    
    template <typename T , typename = void>
    struct MaxOperator {
        struct StateType {
            T result;
            bool has_value = false;
        };
        using ResultType = T;
        
        static StateType Init() {
            return {};
        }

        template <typename ColumnType>
        static inline void UpdateFull(StateType& state , const ColumnType& column) {
            const T* data = column->Data();
            for (size_t i = 0; i < column->Size(); ++i) {
                if (!state.has_value || (state.has_value && state.result < data[i])) {
                    state.has_value = true;
                    state.result = data[i];
                }
            }
        }

        template <typename ColumnType>
        static inline void UpdateFiltered(StateType& state , const ColumnType& column , const boost::dynamic_bitset<>& mask) {
            if (mask.size() != column->Size()) {
                throw std::runtime_error("MaxOperator mask size mismatch!");
            }
            const T* data = column->Data();
            for (size_t i = 0; i < column->Size(); ++i) {
                if (mask[i] && (!state.has_value || (state.has_value && state.result < data[i]))) {
                    state.has_value = true;
                    state.result = data[i];
                }
            }
        }

        template <typename ColumnType>
        static inline void UpdateRow(StateType& state , const ColumnType& column , size_t row) {
            const T value = column->At(row);
            if (!state.has_value || state.result < value) {
                state.has_value = true;
                state.result = value;
            }
        }

        static inline ResultType Finalize(const StateType& state) {
            if (!state.has_value) {
                throw std::runtime_error("MaxOperator dont have value to Finalize!");
            }
            return static_cast<ResultType>(state.result);
        }

        static ColumnType FinalType() {
            return Data::ColumnTraits<T>::type;
        }
    };
    template <StringType T>
    struct MaxOperator<T> {
        struct StateType {
            T result;
            bool has_value = false;
        };
        using ResultType = T;
        
        static StateType Init() {
            return {};
        }

        template <typename ColumnType>
        static inline void UpdateFull(StateType& state , const ColumnType& column) {
            for (size_t i = 0; i < column->Size(); ++i) {
                if (!state.has_value || (state.has_value && state.result < column->At(i))) {
                    state.has_value = true;
                    state.result = column->At(i);
                }
            }
        }

        template <typename ColumnType>
        static inline void UpdateFiltered(StateType& state , const ColumnType& column , const boost::dynamic_bitset<>& mask) {
            if (mask.size() != column->Size()) {
                throw std::runtime_error("MaxOperator mask size mismatch!");
            }
            for (size_t i = 0; i < column->Size(); ++i) {
                if (mask[i] && (!state.has_value || (state.has_value && state.result < column->At(i)))) {
                    state.has_value = true;
                    state.result = column->At(i);
                }
            }
        }

        template <typename ColumnType>
        static inline void UpdateRow(StateType& state , const ColumnType& column , size_t row) {
            const T value(column->At(row));
            if (!state.has_value || state.result < value) {
                state.has_value = true;
                state.result = value;
            }
        }

        static inline ResultType Finalize(const StateType& state) {
            if (!state.has_value) {
                throw std::runtime_error("MaxOperator dont have value to Finalize!");
            }
            return static_cast<ResultType>(state.result);
        }

        static ColumnType FinalType() {
            return Data::ColumnTraits<T>::type;
        }
    };

    template <typename T>
    struct AvgOperator {
        struct StateType {
            __int128_t result;
            size_t count = 0;
        };
        using ResultType = T;
        
        static StateType Init() {
            return {};
        }

        template <typename ColumnType>
        static inline void UpdateFull(StateType& state , const ColumnType& column) {
            const T* data = column->Data();
            for (size_t i = 0; i < column->Size(); ++i) {
                state.result += data[i];
            }
            state.count += column->Size();
        }

        template <typename ColumnType>
        static inline void UpdateFiltered(StateType& state , const ColumnType& column , const boost::dynamic_bitset<>& mask) {
            if (mask.size() != column->Size()) {
                throw std::runtime_error("AvgOperator mask size mismatch!");
            }
            const T* data = column->Data();
            for (size_t i = 0; i < column->Size(); ++i) {
                state.result += data[i] * mask[i];
            }
            state.count += mask.count();
        }

        template <typename ColumnType>
        static inline void UpdateRow(StateType& state , const ColumnType& column , size_t row) {
            state.result += column->At(row);
            ++state.count;
        }

        static inline ResultType Finalize(const StateType& state) {
            if (state.count == 0) {
                throw std::runtime_error("AvgOperator count is zero cant devide!");
            }
            return static_cast<ResultType>(state.result / state.count);
        }

        static ColumnType FinalType() {
            return Data::ColumnTraits<T>::type;
        }
    };

    template<typename T , typename = void>
    struct CountDistinctOperator {
        struct StateType {
            absl::flat_hash_set<T> storage;
        };
        using ResultType = int64_t;

        static StateType Init() {
            StateType result;
            return result;
        }

        template <typename ColumnType>
        static inline void UpdateFull(StateType& state , const ColumnType& column) {
            const T* data = column->Data();
            for (size_t i = 0; i < column->Size(); ++i) {
                state.storage.insert(data[i]);
            }
        }

        template <typename ColumnType>
        static inline void UpdateFiltered(StateType& state , const ColumnType& column , const boost::dynamic_bitset<>& mask) {
            if (mask.size() != column->Size()) {
                throw std::runtime_error("CountDistinctOperator mask size mismatch!");
            }
            const T* data = column->Data();
            for (size_t i = 0; i < column->Size(); ++i) {
                if (mask[i]) {
                    state.storage.insert(data[i]);
                }
            }
        }

        template <typename ColumnType>
        static inline void UpdateRow(StateType& state , const ColumnType& column , size_t row) {
            state.storage.insert(column->At(row));
        }

        static inline ResultType Finalize(const StateType& state) {
            return static_cast<int64_t>(state.storage.size());
        }

        static ColumnType FinalType() {
            return Data::ColumnTraits<int64_t>::type;
        }
    };
    template <StringType T>
    struct CountDistinctOperator<T> {
        struct StateType {
            absl::flat_hash_set<std::string_view> storage;
            Utility::StringArena arena;
        };
        using ResultType = int64_t;

        static StateType Init() {
            return {};
        }

        static inline void Insert(StateType& state , std::string_view value) {
            if (!state.storage.contains(value)) {
                state.storage.insert(state.arena.Add(value));
            }
        }

        template <typename ColumnType>
        static inline void UpdateFull(StateType& state , const ColumnType& column) {
            for (size_t i = 0; i < column->Size(); ++i) {
                Insert(state, column->At(i));
            }
        }

        template <typename ColumnType>
        static inline void UpdateFiltered(StateType& state , const ColumnType& column , const boost::dynamic_bitset<>& mask) {
            if (mask.size() != column->Size()) {
                throw std::runtime_error("CountDistinctOperator mask size mismatch!");
            }
            for (size_t i = 0; i < column->Size(); ++i) {
                if (mask[i]) {
                    Insert(state, column->At(i));
                }
            }
        }

        template <typename ColumnType>
        static inline void UpdateRow(StateType& state , const ColumnType& column , size_t row) {
            Insert(state, column->At(row));
        }

        static inline ResultType Finalize(const StateType& state) {
            return static_cast<int64_t>(state.storage.size());
        }

        static ColumnType FinalType() {
            return Data::ColumnTraits<int64_t>::type;
        }
    };

    struct CountOperator {
        using StateType = size_t;
        using ResultType = int64_t;

        static StateType Init() {
            return 0;
        }

        template <typename ColumnType>
        static inline void UpdateFull(StateType& state , const ColumnType& column) {
            state += column->Size();
        }

        template <typename ColumnType>
        static inline void UpdateFiltered(StateType& state , const ColumnType& column , const boost::dynamic_bitset<>& mask) {
            if (mask.size() != column->Size()) {
                throw std::runtime_error("CountOperator mask size mismatch!");
            }
            state += mask.count();
        }

        template <typename ColumnType>
        static inline void UpdateRow(StateType& state , const ColumnType& column , size_t row) {
            (void)column;
            (void)row;
            ++state;
        }

        static inline ResultType Finalize(const StateType& state) {
            return static_cast<ResultType>(state);
        }

        static ColumnType FinalType() {
            return Data::ColumnTraits<int64_t>::type;
        }
    };

    template <typename T , typename ColumnType>
    inline size_t Count(const ColumnType& column , const boost::dynamic_bitset<>& mask) {
        if (mask.size() != column->Size()) {
            throw std::runtime_error("Count aggregation mask size mismatch!");
        }
        return mask.count();
    }

    inline size_t Count(const boost::dynamic_bitset<>& mask) {
        return mask.count();
    }

} // namespace Aggregation
