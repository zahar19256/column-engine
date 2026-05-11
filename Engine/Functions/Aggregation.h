#pragma once
#include "../../FileBasicTools/DataStructures/Column.h"
#include "../../FileBasicTools/DataStructures/Batch.h"
#include "../../FileBasicTools/DataStructures/Types.h"
#include "../Operators/BasicOperators.h"
#include <cstdint>
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
        std::optional<std::string> new_name;
        ColumnType input_type = ColumnType::Unknown;
        ColumnType output_type = ColumnType::Unknown;
    };

    class AggregationState {
    public:
        virtual ~AggregationState() = default;
        virtual void UpdateBatch(const Batch& data) = 0;
        virtual void UpdateRow(const Batch& data , size_t index_row) = 0;
        virtual void AppendResult(Column& out) const = 0;
        virtual ColumnType FinalType() const = 0;
    };

    template <typename T , template <typename> typename OperatorT>
    class TypedAggregationState : public AggregationState {
    public:
        explicit TypedAggregationState(std::string column_name)
            : column_name_(std::move(column_name)), state_(Operator::Init()) {
        }

        void UpdateBatch(const Batch& data) override {
            using ColumnT = typename Data::ColumnTraits<T>::ColumnT;
            const auto& column = As<ColumnT>(data.GetColumn(column_name_));
            if (!column) {
                throw std::runtime_error("Aggregation column type mismatch!");
            }
            const auto& mask = data.GetMsk();
            if (mask.empty() || mask.count() == mask.size()) {
                Operator::UpdateFull(state_, column);
            } else {
                Operator::UpdateFiltered(state_, column, mask);
            }
        }

        void UpdateRow(const Batch& data , size_t index_row) override {
            using ColumnT = typename Data::ColumnTraits<T>::ColumnT;
            const auto& column = As<ColumnT>(data.GetColumn(column_name_));
            if (!column) {
                throw std::runtime_error("Aggregation column type mismatch!");
            }
            Operator::UpdateRow(state_, column, index_row);
        }

        void AppendResult(Column& out) const override {
            using OutColumnT = typename Data::ColumnTraits<typename Operator::ResultType>::ColumnT;
            auto* result = dynamic_cast<OutColumnT*>(&out);
            if (result == nullptr) {
                throw std::runtime_error("Aggregation output column type mismatch!");
            }
            result->Push_Back(Operator::Finalize(state_));
        }

        ColumnType FinalType() const override {
            return Operator::FinalType();
        }

    private:
        using Operator = OperatorT<T>;
        std::string column_name_;
        typename Operator::StateType state_;
    };

    class CountAggregationState : public AggregationState {
    public:
        explicit CountAggregationState(std::optional<std::string> column_name)
            : column_name_(std::move(column_name)), state_(CountOperator::Init()) {
        }

        void UpdateBatch(const Batch& data) override {
            const auto& mask = data.GetMsk();
            if (column_name_.has_value()) {
                auto column = data.GetColumn(*column_name_);
                if (mask.empty() || mask.count() == mask.size()) {
                    CountOperator::UpdateFull(state_, column);
                } else {
                    CountOperator::UpdateFiltered(state_, column, mask);
                }
                return;
            }
            if (!mask.empty() && mask.size() != data.GetRows()) {
                throw std::runtime_error("Count aggregation mask size mismatch!");
            }
            state_ += mask.empty() ? data.GetRows() : mask.count();
        }

        void UpdateRow(const Batch& data , size_t index_row) override {
            if (column_name_.has_value()) {
                auto column = data.GetColumn(*column_name_);
                CountOperator::UpdateRow(state_, column, index_row);
                return;
            }
            (void)data;
            (void)index_row;
            ++state_;
        }

        void AppendResult(Column& out) const override {
            auto* result = dynamic_cast<Int64Column*>(&out);
            if (result == nullptr) {
                throw std::runtime_error("Count aggregation output column type mismatch!");
            }
            result->Push_Back(CountOperator::Finalize(state_));
        }

        ColumnType FinalType() const override {
            return CountOperator::FinalType();
        }

    private:
        std::optional<std::string> column_name_;
        CountOperator::StateType state_;
    };

    std::unique_ptr<AggregationState> MakeAggregationState(const AggregationCall& call);
}
