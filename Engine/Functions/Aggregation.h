#pragma once
#include "../../FileBasicTools/DataStructures/Column.h"
#include "../../FileBasicTools/DataStructures/Batch.h"
#include "../../FileBasicTools/DataStructures/Types.h"
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace Aggregation {
    enum class AggregationType : uint8_t {
        Sum = 0,
        Min,
        Max,
        Avg,
        Count,
        Distinct,
    };
    
    struct AggregationCall {
        AggregationType type;
        std::optional<std::string> column_name;
        std::string new_name;
        ColumnType input_type = ColumnType::Unknown;
        ColumnType output_type = ColumnType::Unknown;
    };

    template <typename T , typename ColumnType>
    __int128_t Sum(const ColumnType& column , const boost::dynamic_bitset<>& mask) {
        __int128_t sum = 0;
        const T* data = column->Data();
        for (size_t i = 0; i < column->Size(); ++i) {
            sum += data[i] * mask[i];
        }
        return sum;
    }

    template <typename T , typename ColumnType>
    std::optional<T> Min(const ColumnType& column , const boost::dynamic_bitset<>& mask) {
        if (mask.size() != column->Size()) {
            throw std::runtime_error("Min aggregation mask size mismatch!");
        }
        std::optional<T> result;
        const T* data = column->Data();
        for (size_t i = 0; i < column->Size(); ++i) {
            if (mask[i] && (!result.has_value() || data[i] < *result)) {
                result = data[i];
            }
        }
        return result;
    }
    
    template <typename T , typename ColumnType>
    std::optional<T> Max(const ColumnType& column , const boost::dynamic_bitset<>& mask) {
        if (mask.size() != column->Size()) {
            throw std::runtime_error("Max aggregation mask size mismatch!");
        }
        std::optional<T> result;
        const T* data = column->Data();
        for (size_t i = 0; i < column->Size(); ++i) {
            if (mask[i] && (!result.has_value() || data[i] > *result)) {
                result = data[i];
            }
        }
        return result;
    }

    template <typename T , typename ColumnType>
    std::pair <__int128_t , size_t> Avg(const ColumnType& column , const boost::dynamic_bitset<>& mask) {
        return {Sum<T>(column , mask) , mask.count()};
    }

    template <typename T , typename ColumnType>
    size_t Count(const ColumnType& column , const boost::dynamic_bitset<>& mask) {
        if (mask.size() != column.Size()) {
            throw std::runtime_error("Count aggregation mask size mismatch!");
        }
        return mask.count();
    }

    inline size_t Count(const boost::dynamic_bitset<>& mask) {
        return mask.count();
    }

    class AggregationState {
    public:
        virtual ~AggregationState() = default;
        virtual void UpdateState(const Batch& data) = 0;
        virtual void AppendResult(Column& out) const = 0;
        virtual ColumnType FinalType() const = 0;
    };

    template <typename T>
    class SumAggregationState : public AggregationState {
    public:
        explicit SumAggregationState(std::string column_name) : column_name_(std::move(column_name)) {
        }
        void UpdateState(const Batch& data) override {
            using ColumnT = typename Data::ColumnTraits<T>::ColumnT;
            const auto& column = As<ColumnT>(data.GetColumn(column_name_));
            if (!column) {
                throw std::runtime_error("Sum aggregation column type mismatch!");
            }
            result_ += Sum<T>(column , data.GetMsk());
        }
        void AppendResult(Column& out) const override {
            using OutColumnT = typename Data::ColumnTraits<T>::ColumnT;
            auto* result = dynamic_cast<OutColumnT*>(&out);
            if (result == nullptr) {
                throw std::runtime_error("Sum aggregation output column type mismatch!");
            }
            result->Push_Back(result_);
        }
        ColumnType FinalType() const override {
            return Data::ColumnTraits<T>::type;
        }

    private:
        std::string column_name_;
        __int128_t result_ = 0;
    };

    template <typename T>
    class MinAggregationState : public AggregationState {
    public:
        explicit MinAggregationState(std::string column_name) : column_name_(std::move(column_name)) {
        }
        void UpdateState(const Batch& data) override {
            using ColumnT = typename Data::ColumnTraits<T>::ColumnT;
            const auto& column = As<ColumnT>(data.GetColumn(column_name_));
            if (!column) {
                throw std::runtime_error("Min aggregation column type mismatch!");
            }
            auto result = Min<T>(column , data.GetMsk());
            if (result.has_value() && (!has_value_ || *result < result_)) {
                result_ = *result;
                has_value_ = true;
            }
        }
        void AppendResult(Column& out) const override {
            if (!has_value_) {
                throw std::runtime_error("Cant append Min result is empty!");
            }
            using OutColumnT = typename Data::ColumnTraits<T>::ColumnT;
            auto* result = dynamic_cast<OutColumnT*>(&out);
            if (result == nullptr) {
                throw std::runtime_error("Min aggregation output column type mismatch!");
            }
            result->Push_Back(result_);
        }
        ColumnType FinalType() const override {
            return Data::ColumnTraits<T>::type;
        }

    private:
        std::string column_name_;
        T result_{};
        bool has_value_ = false;
    };

    template <typename T>
    class MaxAggregationState : public AggregationState {
    public:
        explicit MaxAggregationState(std::string column_name) : column_name_(std::move(column_name)) {
        }
        void UpdateState(const Batch& data) override {
            using ColumnT = typename Data::ColumnTraits<T>::ColumnT;
            const auto& column = As<ColumnT>(data.GetColumn(column_name_));
            if (!column) {
                throw std::runtime_error("Max aggregation column type mismatch!");
            }
            auto result = Max<T>(column , data.GetMsk());
            if (result.has_value() && (!has_value_ || *result > result_)) {
                result_ = *result;
                has_value_ = true;
            }
        }
        void AppendResult(Column& out) const override {
            if (!has_value_) {
                throw std::runtime_error("Cant append Max result is empty!");
            }
            using OutColumnT = typename Data::ColumnTraits<T>::ColumnT;
            auto* result = dynamic_cast<OutColumnT*>(&out);
            if (result == nullptr) {
                throw std::runtime_error("Max aggregation output column type mismatch!");
            }
            result->Push_Back(result_);
        }
        ColumnType FinalType() const override {
            return Data::ColumnTraits<T>::type;
        }

    private:
        std::string column_name_;
        T result_{};
        bool has_value_ = false;
    };

    template <typename T>
    class AvgAggregationState : public AggregationState {
    public:
        explicit AvgAggregationState(std::string column_name)
            : column_name_(std::move(column_name))
        {
        }
        void UpdateState(const Batch& data) override {
            using ColumnT = typename Data::ColumnTraits<T>::ColumnT;
            const auto& column = As<ColumnT>(data.GetColumn(column_name_));
            if (!column) {
                throw std::runtime_error("Avg aggregation column type mismatch!");
            }
            auto [batch_sum , batch_count] = Avg<T>(column , data.GetMsk());
            sum_ += batch_sum;
            count_ += batch_count;
        }
        void AppendResult(Column& out) const override {
            if (count_ == 0) {
                throw std::runtime_error("Cant append Avg result is empty!");
            }
            using OutColumnT = typename Data::ColumnTraits<T>::ColumnT;
            auto* typed_out = dynamic_cast<OutColumnT*>(&out);
            if (typed_out == nullptr) {
                throw std::runtime_error("Avg aggregation output column type mismatch!");
            }
            typed_out->Push_Back(static_cast<T>(sum_ / count_));
        }
        ColumnType FinalType() const override {
            return Data::ColumnTraits<T>::type;
        }

    private:
        std::string column_name_;
        __int128_t sum_ = 0;
        size_t count_ = 0;
        
    };

    inline std::unique_ptr<AggregationState> MakeAggregationState(const AggregationCall& call) {
        switch (call.type) {
            case AggregationType::Sum:
                if (!call.column_name.has_value()) {
                    throw std::runtime_error("SUM aggregation requires column name!");
                }
                switch(call.input_type) {
                    case ColumnType::Int8:
                        return std::make_unique<SumAggregationState<int8_t>>(*call.column_name);
                    case ColumnType::Int16:
                        return std::make_unique<SumAggregationState<int16_t>>(*call.column_name);
                    case ColumnType::Int32:
                        return std::make_unique<SumAggregationState<int32_t>>(*call.column_name);
                    case ColumnType::Int64:
                        return std::make_unique<SumAggregationState<int64_t>>(*call.column_name);
                    default:
                        throw std::runtime_error("Unkown type for SUM Aggregation!");
                }
                break;
            case AggregationType::Max:
                if (!call.column_name.has_value()) {
                    throw std::runtime_error("MAX aggregation requires column name!");
                }
                switch(call.input_type) {
                    case ColumnType::Int8:
                        return std::make_unique<MaxAggregationState<int8_t>>(*call.column_name);
                    case ColumnType::Int16:
                        return std::make_unique<MaxAggregationState<int16_t>>(*call.column_name);
                    case ColumnType::Int32:
                        return std::make_unique<MaxAggregationState<int32_t>>(*call.column_name);
                    case ColumnType::Int64:
                        return std::make_unique<MaxAggregationState<int64_t>>(*call.column_name);
                    default:
                        throw std::runtime_error("Unknown type for MAX Aggregation!");
                }
                break;

            case AggregationType::Min:
                if (!call.column_name.has_value()) {
                    throw std::runtime_error("MIN aggregation requires column name!");
                }
                switch(call.input_type) {
                    case ColumnType::Int8:
                        return std::make_unique<MinAggregationState<int8_t>>(*call.column_name);
                    case ColumnType::Int16:
                        return std::make_unique<MinAggregationState<int16_t>>(*call.column_name);
                    case ColumnType::Int32:
                        return std::make_unique<MinAggregationState<int32_t>>(*call.column_name);
                    case ColumnType::Int64:
                        return std::make_unique<MinAggregationState<int64_t>>(*call.column_name);
                    default:
                        throw std::runtime_error("Unknown type for MIN Aggregation!");
                }
                break;
            default:
                break;
        }

        throw std::runtime_error("Unsupported aggregation state!");
    }
}
