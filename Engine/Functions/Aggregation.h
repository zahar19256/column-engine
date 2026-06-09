#pragma once
#include "../../FileBasicTools/DataStructures/Column.h"
#include "../../FileBasicTools/DataStructures/Types.h"
#include "../Functions/Expression.h"
#include "../Operators/AggrOperators.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace Aggregation {
    template <typename ColumnT>
    constexpr ColumnType StaticColumnType() {
        if constexpr (std::is_same_v<ColumnT , Int8Column>) {
            return ColumnType::Int8;
        } else if constexpr (std::is_same_v<ColumnT , Int16Column>) {
            return ColumnType::Int16;
        } else if constexpr (std::is_same_v<ColumnT , Int32Column>) {
            return ColumnType::Int32;
        } else if constexpr (std::is_same_v<ColumnT , Int64Column>) {
            return ColumnType::Int64;
        } else if constexpr (std::is_same_v<ColumnT , Int128Column>) {
            return ColumnType::Int128;
        } else if constexpr (std::is_same_v<ColumnT , DoubleColumn>) {
            return ColumnType::Double;
        } else if constexpr (std::is_same_v<ColumnT , DateColumn>) {
            return ColumnType::Date;
        } else if constexpr (std::is_same_v<ColumnT , TimeStampColumn>) {
            return ColumnType::Timestamp;
        } else if constexpr (std::is_same_v<ColumnT , StringColumn>) {
            return ColumnType::String;
        } else if constexpr (std::is_same_v<ColumnT , FlatStringColumn>) {
            return ColumnType::FlatString;
        } else {
            return ColumnType::Unknown;
        }
    }

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
        std::shared_ptr<ScalarExpr> expression;
        std::optional<std::string> new_name;
        ColumnType input_type = ColumnType::Unknown;
        ColumnType output_type = ColumnType::Unknown;
    };

    class AggregationState {
    public:
        virtual ~AggregationState() = default;
        virtual void UpdateBatch(const Column* column , size_t rows , const boost::dynamic_bitset<>& mask) = 0;
        virtual void UpdateRow(const Column* column , size_t index_row) = 0;
        virtual void AppendResult(Column& out) const = 0;
        virtual ColumnType FinalType() const = 0;
    };

    template <typename ValueT, typename InputColumnT, typename OutputColumnT, template <typename> typename OperatorT>
    class TypedAggregationState : public AggregationState {
    public:
        explicit TypedAggregationState()
            : state_(Operator::Init()) {
        }

        void UpdateBatch(const Column* input , size_t rows , const boost::dynamic_bitset<>& mask) override {
            const auto& column = dynamic_cast<const InputColumnT*>(input);
            if (!column) {
                throw std::runtime_error("Aggregation column type mismatch!");
            }
            if (mask.empty() || mask.count() == mask.size()) {
                Operator::UpdateFull(state_, column);
            } else {
                Operator::UpdateFiltered(state_, column, mask);
            }
        }

        void UpdateRow(const Column* input , size_t index_row) override {
            if (input == nullptr) {
                throw std::runtime_error("Aggregation requires input expression!");
            }
            const auto& column = dynamic_cast<const InputColumnT*>(input);
            if (!column) {
                throw std::runtime_error("Aggregation column type mismatch!");
            }
            Operator::UpdateRow(state_, column, index_row);
        }

        void AppendResult(Column& out) const override {
            auto* result = dynamic_cast<OutputColumnT*>(&out);
            if (result == nullptr) {
                throw std::runtime_error("Aggregation output column type mismatch!");
            }
            if constexpr (std::is_same_v<OutputColumnT , StringColumn>) {
                GermanStr value = Operator::Finalize(state_);
                std::string_view view = value.View();
                result->AppendFromString(view.data() , view.size());
            } else {
                result->Push_Back(Operator::Finalize(state_));
            }
        }

        ColumnType FinalType() const override {
            return StaticColumnType<OutputColumnT>();
        }

    private:
        using Operator = OperatorT<ValueT>;
        typename Operator::StateType state_;
    };

    class CountAggregationState : public AggregationState {
    public:
        explicit CountAggregationState(bool has_expression)
            : has_expression_(has_expression), state_(CountOperator::Init()) {
        }

        void UpdateBatch(const Column* column , size_t rows , const boost::dynamic_bitset<>& mask) override {
            if (has_expression_) {
                if (column == nullptr) {
                    throw std::runtime_error("COUNT aggregation expression is null!");
                }
                if (mask.empty() || mask.count() == mask.size()) {
                    CountOperator::UpdateFull(state_, column);
                } else {
                    CountOperator::UpdateFiltered(state_, column, mask);
                }
                return;
            }
            if (!mask.empty() && mask.size() != rows) {
                throw std::runtime_error("Count aggregation mask size mismatch!");
            }
            state_ += mask.empty() ? rows : mask.count();
        }

        void UpdateRow(const Column* column , size_t index_row) override {
            if (has_expression_) {
                if (column == nullptr) {
                    throw std::runtime_error("COUNT aggregation expression is null!");
                }
                CountOperator::UpdateRow(state_, column, index_row);
                return;
            }
            (void)column;
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
        bool has_expression_;
        CountOperator::StateType state_;
    };
} // namespace Aggregation
