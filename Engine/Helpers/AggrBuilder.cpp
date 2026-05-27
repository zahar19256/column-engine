#include "AggrBuilder.h"
#include "Column.h"

AggregationCall MakeAggrCall(AggregationType cur_type , std::optional<std::string> col_name , std::optional<std::string> alias , ColumnType in_type , ColumnType out_type) {
    std::shared_ptr<ScalarExpr> expression;
    if (col_name.has_value()) {
        expression = std::make_shared<ColumnExpr>(std::move(*col_name) , in_type);
    }
    return MakeAggrCall(cur_type , std::move(expression) , std::move(alias) , in_type , out_type);
}

AggregationCall MakeAggrCall(AggregationType cur_type , std::shared_ptr<ScalarExpr> expression , std::optional<std::string> alias , ColumnType in_type , ColumnType out_type) {
    AggregationCall result;
    result.expression = std::move(expression);
    result.new_name = std::move(alias);
    result.input_type = in_type;
    result.output_type = out_type;
    result.type = cur_type;
    return result;
}

std::unique_ptr<AggregationState> MakeAggregationState(const AggregationCall& call) {
    switch (call.type) {
        case AggregationType::Sum:
            if (!call.expression) {
                throw std::runtime_error("SUM aggregation requires expression!");
            }
            switch(call.input_type) {
                case ColumnType::Int8:
                    return std::make_unique<TypedAggregationState<int8_t, Int8Column, Int64Column, SumOperator>>();
                case ColumnType::Int16:
                    return std::make_unique<TypedAggregationState<int16_t, Int16Column, Int64Column, SumOperator>>();
                case ColumnType::Int32:
                    return std::make_unique<TypedAggregationState<int32_t, Int32Column, Int64Column, SumOperator>>();
                case ColumnType::Int64:
                    return std::make_unique<TypedAggregationState<int64_t, Int64Column, Int64Column, SumOperator>>();
                default:
                    throw std::runtime_error("Unkown type for SUM Aggregation!");
            }
            break;
        case AggregationType::Max:
            if (!call.expression) {
                throw std::runtime_error("MAX aggregation requires expression!");
            }
            switch(call.input_type) {
                case ColumnType::Int8:
                    return std::make_unique<TypedAggregationState<int8_t, Int8Column, Int8Column, MaxOperator>>();
                case ColumnType::Int16:
                    return std::make_unique<TypedAggregationState<int16_t, Int16Column, Int16Column, MaxOperator>>();
                case ColumnType::Int32:
                    return std::make_unique<TypedAggregationState<int32_t, Int32Column, Int32Column, MaxOperator>>();
                case ColumnType::Int64:
                    return std::make_unique<TypedAggregationState<int64_t, Int64Column, Int64Column, MaxOperator>>();
                case ColumnType::Date:
                    return std::make_unique<TypedAggregationState<int64_t, DateColumn, DateColumn, MaxOperator>>();
                case ColumnType::Timestamp:
                    return std::make_unique<TypedAggregationState<int64_t, TimeStampColumn, TimeStampColumn, MaxOperator>>();
                case ColumnType::String:
                    return std::make_unique<TypedAggregationState<std::string, StringColumn, StringColumn, MaxOperator>>();
                default:
                    throw std::runtime_error("Unknown type for MAX Aggregation!");
            }
            break;

        case AggregationType::Min:
            if (!call.expression) {
                throw std::runtime_error("MIN aggregation requires expression!");
            }
            switch(call.input_type) {
                case ColumnType::Int8:
                    return std::make_unique<TypedAggregationState<int8_t, Int8Column, Int8Column, MinOperator>>();
                case ColumnType::Int16:
                    return std::make_unique<TypedAggregationState<int16_t, Int16Column, Int16Column, MinOperator>>();
                case ColumnType::Int32:
                    return std::make_unique<TypedAggregationState<int32_t, Int32Column, Int32Column, MinOperator>>();
                case ColumnType::Int64:
                    return std::make_unique<TypedAggregationState<int64_t, Int64Column, Int64Column, MinOperator>>();
                case ColumnType::Date:
                    return std::make_unique<TypedAggregationState<int64_t, DateColumn, DateColumn, MinOperator>>();
                case ColumnType::Timestamp:
                    return std::make_unique<TypedAggregationState<int64_t, TimeStampColumn, TimeStampColumn, MinOperator>>();
                case ColumnType::String:
                    return std::make_unique<TypedAggregationState<std::string, StringColumn, StringColumn, MinOperator>>();
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
                    return std::make_unique<CountAggregationState>(call.expression != nullptr);
                default:
                    throw std::runtime_error("Unknown type for Count Aggregation!");
            }
        case AggregationType::Avg:
            if (!call.expression) {
                throw std::runtime_error("AVG aggregation requires expression!");
            }
            switch(call.input_type) {
                case ColumnType::Int8:
                    return std::make_unique<TypedAggregationState<int8_t, Int8Column, Int8Column, AvgOperator>>();
                case ColumnType::Int16:
                    return std::make_unique<TypedAggregationState<int16_t, Int16Column, Int16Column, AvgOperator>>();
                case ColumnType::Int32:
                    return std::make_unique<TypedAggregationState<int32_t, Int32Column, Int32Column, AvgOperator>>();
                case ColumnType::Int64:
                    return std::make_unique<TypedAggregationState<int64_t, Int64Column, Int64Column, AvgOperator>>();
                default:
                    throw std::runtime_error("Unknown type for AVG Aggregation!");
            }
        case AggregationType::Distinct:
            if (!call.expression) {
                throw std::runtime_error("COUNT DISTINCT aggregation requires expression!");
            }
            switch(call.input_type) {
                case ColumnType::Int8:
                    return std::make_unique<TypedAggregationState<int8_t, Int8Column, Int64Column, CountDistinctOperator>>();
                case ColumnType::Int16:
                    return std::make_unique<TypedAggregationState<int16_t, Int16Column, Int64Column, CountDistinctOperator>>();
                case ColumnType::Int32:
                    return std::make_unique<TypedAggregationState<int32_t, Int32Column, Int64Column, CountDistinctOperator>>();
                case ColumnType::Int64:
                    return std::make_unique<TypedAggregationState<int64_t, Int64Column, Int64Column, CountDistinctOperator>>();
                case ColumnType::Double:
                    return std::make_unique<TypedAggregationState<double, DoubleColumn, Int64Column, CountDistinctOperator>>();
                case ColumnType::String:
                    return std::make_unique<TypedAggregationState<std::string, StringColumn, Int64Column, CountDistinctOperator>>();
                case ColumnType::Date:
                    return std::make_unique<TypedAggregationState<int64_t, DateColumn, Int64Column, CountDistinctOperator>>();
                case ColumnType::Timestamp:
                    return std::make_unique<TypedAggregationState<int64_t, TimeStampColumn, Int64Column, CountDistinctOperator>>();
                default:
                    throw std::runtime_error("Unknown type for COUNT DISTINCT Aggregation!");
            }
        default:
            break;
    }

    throw std::runtime_error("Not implemented aggregation state!");
}
