#include "Aggregation.h"

namespace Aggregation {
    std::unique_ptr<AggregationState> MakeAggregationState(const AggregationCall& call) {
        switch (call.type) {
            case AggregationType::Sum:
                if (!call.column_name.has_value()) {
                    throw std::runtime_error("SUM aggregation requires column name!");
                }
                switch(call.input_type) {
                    case ColumnType::Int8:
                        return std::make_unique<TypedAggregationState<int8_t, SumOperator>>(*call.column_name);
                    case ColumnType::Int16:
                        return std::make_unique<TypedAggregationState<int16_t, SumOperator>>(*call.column_name);
                    case ColumnType::Int32:
                        return std::make_unique<TypedAggregationState<int32_t, SumOperator>>(*call.column_name);
                    case ColumnType::Int64:
                        return std::make_unique<TypedAggregationState<int64_t, SumOperator>>(*call.column_name);
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
                        return std::make_unique<TypedAggregationState<int8_t, MaxOperator>>(*call.column_name);
                    case ColumnType::Int16:
                        return std::make_unique<TypedAggregationState<int16_t, MaxOperator>>(*call.column_name);
                    case ColumnType::Int32:
                        return std::make_unique<TypedAggregationState<int32_t, MaxOperator>>(*call.column_name);
                    case ColumnType::Int64:
                        return std::make_unique<TypedAggregationState<int64_t, MaxOperator>>(*call.column_name);
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
                        return std::make_unique<TypedAggregationState<int8_t, MinOperator>>(*call.column_name);
                    case ColumnType::Int16:
                        return std::make_unique<TypedAggregationState<int16_t, MinOperator>>(*call.column_name);
                    case ColumnType::Int32:
                        return std::make_unique<TypedAggregationState<int32_t, MinOperator>>(*call.column_name);
                    case ColumnType::Int64:
                        return std::make_unique<TypedAggregationState<int64_t, MinOperator>>(*call.column_name);
                    default:
                        throw std::runtime_error("Unknown type for MIN Aggregation!");
                }
                break;
            case AggregationType::Count:
                switch(call.input_type) {
                    case ColumnType::Int8:
                    case ColumnType::Int16:
                    case ColumnType::Int32:
                    case ColumnType::Int64:
                    case ColumnType::String:
                    case ColumnType::Unknown:
                        return std::make_unique<CountAggregationState>(call.column_name);
                    default:
                        throw std::runtime_error("Unknown type for Count Aggregation!");
                }
            case AggregationType::Avg:
                if (!call.column_name.has_value()) {
                    throw std::runtime_error("AVG aggregation requires column name!");
                }
                switch(call.input_type) {
                    case ColumnType::Int8:
                        return std::make_unique<TypedAggregationState<int8_t, AvgOperator>>(*call.column_name);
                    case ColumnType::Int16:
                        return std::make_unique<TypedAggregationState<int16_t, AvgOperator>>(*call.column_name);
                    case ColumnType::Int32:
                        return std::make_unique<TypedAggregationState<int32_t, AvgOperator>>(*call.column_name);
                    case ColumnType::Int64:
                        return std::make_unique<TypedAggregationState<int64_t, AvgOperator>>(*call.column_name);
                    default:
                        throw std::runtime_error("Unknown type for AVG Aggregation!");
                }
            default:
                break;
        }

        throw std::runtime_error("Not implemented aggregation state!");
    }
}
