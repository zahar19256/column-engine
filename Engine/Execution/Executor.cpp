#include "Executor.h"
#include "Functions/Aggregation.h"
#include "GermanString.h"
#include "Utility.h"

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <variant>
namespace { // TODO вынести в хелперы/Types

ColumnType ScalarValueType(const Utility::ScalarValue& value) {
    return std::visit([](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            return ColumnType::Int64;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return ColumnType::String;
        } else if constexpr (std::is_same_v<T, double>) {
            return ColumnType::Double;
        } else if constexpr (std::is_same_v<T, __int128_t>) {
            return ColumnType::Int128;
        } else {
            return ColumnType::Unknown;
        }
    }, value);
}

template <typename ColumnT , typename ValueT>
void PushTypedValue(Column& column , ValueT value) {
    auto* typed = dynamic_cast<ColumnT*>(&column);
    if (typed == nullptr) {
        throw std::runtime_error("Group by output column type mismatch!");
    }
    typed->Push_Back(value);
}

int64_t RequireInt64Value(const Utility::ScalarValue& value) {
    if (const auto* current = std::get_if<int64_t>(&value)) {
        return *current;
    }
    throw std::runtime_error("Group by key value is not Int64-compatible!");
}

void AppendScalarValue(Column& column , const Utility::ScalarValue& value) {
    switch(column.GetType()) {
        case ColumnType::Int8:
            PushTypedValue<Int8Column>(column , static_cast<int8_t>(RequireInt64Value(value)));
            return;
        case ColumnType::Int16:
            PushTypedValue<Int16Column>(column , static_cast<int16_t>(RequireInt64Value(value)));
            return;
        case ColumnType::Int32:
            PushTypedValue<Int32Column>(column , static_cast<int32_t>(RequireInt64Value(value)));
            return;
        case ColumnType::Int64:
            PushTypedValue<Int64Column>(column , RequireInt64Value(value));
            return;
        case ColumnType::Date:
            PushTypedValue<DateColumn>(column , RequireInt64Value(value));
            return;
        case ColumnType::Timestamp:
            PushTypedValue<TimeStampColumn>(column , RequireInt64Value(value));
            return;
        case ColumnType::Int128:
            if (const auto* current = std::get_if<__int128_t>(&value)) {
                PushTypedValue<Int128Column>(column , *current);
                return;
            }
            PushTypedValue<Int128Column>(column , static_cast<__int128_t>(RequireInt64Value(value)));
            return;
        case ColumnType::Double:
            if (const auto* current = std::get_if<double>(&value)) {
                PushTypedValue<DoubleColumn>(column , *current);
                return;
            }
            throw std::runtime_error("Group by key value is not Double!");
        case ColumnType::String:
            if (const auto* current = std::get_if<GermanStr>(&value)) {
                PushTypedValue<StringColumn>(column , *current);
                return;
            }
            throw std::runtime_error("Group by key value is not String!");
        default:
            throw std::runtime_error("Unknown column type in append scalar value!");
    }
    throw std::runtime_error("Unknown group by output column type!");
}

} // namespace

std::string GlobalAggregationExecutor::DefaultOutputName(const Aggregation::AggregationCall& call , size_t index) const {
    if (call.new_name.has_value()) {
        return *call.new_name;
    }
    if (const auto* column = dynamic_cast<const ColumnExpr*>(call.expression.get())) {
        return column->GetName();
    }
    return "aggregation_" + std::to_string(index);
}

void GlobalAggregationExecutor::OutputBatch(Batch& out) const {
    out.Clear();
    Scheme scheme;
    std::vector<std::shared_ptr<Column>> columns;
    columns.reserve(states_.size());
    for (size_t i = 0; i < states_.size(); ++i) {
        const ColumnType result_type =
            calls_[i].output_type == ColumnType::Unknown ? states_[i]->FinalType() : calls_[i].output_type;

        auto column = MakeColumn(result_type , out.GetStringArena());
        states_[i]->AppendResult(*column);
        scheme.Push_Back(SchemeNode{DefaultOutputName(calls_[i] , i), result_type});
        columns.push_back(std::move(column));
    }
    out.SetScheme(scheme);
    for (auto& column : columns) {
        out.AddColumn(std::move(column));
    }
    out.InitMsk();
}

bool GroupByAggregationExecutor::Next(Batch& data) {
    if (finished_) {
        return false;
    }
    if (!child) {
        throw std::runtime_error("No child in GroupByExecutor!");
    }
    Batch input;
    while (child->Next(input)) {
        std::vector<std::shared_ptr<Column>> group_columns;
        group_columns.reserve(group_by_.size());
        for (const auto& expr : group_by_) {
            group_columns.push_back(expr->EvalBatch(input , env_));
        }
        std::vector<std::shared_ptr<Column>> aggregate_columns;
        aggregate_columns.reserve(calls_.size());
        for (const AggregationCall& call : calls_) {
            aggregate_columns.push_back(call.expression ? call.expression->EvalBatch(input , env_) : nullptr);
        }

        const auto& mask = input.GetMsk();
        bool use_mask = !mask.empty() && mask.count() != mask.size();
        for (size_t row_index = 0; row_index < input.GetRows(); ++row_index) {
            if (use_mask && !mask[row_index]) {
                continue;
            }
            Utility::GroupKey key;
            key.reserve(group_by_.size());
            for (const auto& column : group_columns) {
                key.push_back(column->GetScalarValue(row_index));
            }
            auto [it , added] = storage_.try_emplace(std::move(key));
            if (added) {
                it->second = MakeStates();
            }
            for (size_t i = 0; i < it->second.size(); ++i) {
                it->second[i]->UpdateRow(aggregate_columns[i].get() , row_index);
            }
        }
    }
    OutputBatch(data);
    finished_ = true;
    return true;
}

// LLM Attention
// Ниже часть для сериализации вывода групп-бай была создана при помощи ИИ!
std::string GroupByAggregationExecutor::DefaultGroupByOutputName(const std::shared_ptr<ScalarExpr>& expr , size_t index) const {
    if (const auto* column = dynamic_cast<const ColumnExpr*>(expr.get())) {
        return column->GetName();
    }
    return "group_" + std::to_string(index);
}

std::string GroupByAggregationExecutor::DefaultOutputName(const Aggregation::AggregationCall& call , size_t index) const {
    if (call.new_name.has_value()) {
        return *call.new_name;
    }
    if (const auto* column = dynamic_cast<const ColumnExpr*>(call.expression.get())) {
        return column->GetName();
    }
    return "aggregation_" + std::to_string(index);
}

ColumnType GroupByAggregationExecutor::GroupByOutputType(size_t index) const {
    const ColumnType type = group_by_[index]->GetType();
    if (type != ColumnType::Unknown) {
        return type;
    }
    if (!storage_.empty()) {
        return ScalarValueType(storage_.begin()->first[index]);
    }
    throw std::runtime_error("Cannot infer empty group by output type!");
}

ColumnType GroupByAggregationExecutor::AggregationOutputType(size_t index) const {
    if (calls_[index].output_type != ColumnType::Unknown) {
        return calls_[index].output_type;
    }
    if (!storage_.empty()) {
        return storage_.begin()->second[index]->FinalType();
    }
    return MakeAggregationState(calls_[index])->FinalType();
}

void GroupByAggregationExecutor::OutputBatch(Batch& out) const {
    out.Clear();
    Scheme scheme;
    std::vector<std::shared_ptr<Column>> columns;
    columns.reserve(group_by_.size() + calls_.size());

    for (size_t i = 0; i < group_by_.size(); ++i) {
        const ColumnType result_type = GroupByOutputType(i);
        auto column = MakeColumn(result_type , out.GetStringArena());
        column->Reserve(storage_.size());
        scheme.Push_Back(SchemeNode{DefaultGroupByOutputName(group_by_[i] , i), result_type});
        columns.push_back(std::move(column));
    }

    for (size_t i = 0; i < calls_.size(); ++i) {
        const ColumnType result_type = AggregationOutputType(i);
        auto column = MakeColumn(result_type , out.GetStringArena());
        column->Reserve(storage_.size());
        scheme.Push_Back(SchemeNode{DefaultOutputName(calls_[i] , i), result_type});
        columns.push_back(std::move(column));
    }

    for (const auto& [key , states] : storage_) {
        if (key.size() != group_by_.size() || states.size() != calls_.size()) {
            throw std::runtime_error("Invalid group by state size!");
        }
        for (size_t i = 0; i < key.size(); ++i) {
            AppendScalarValue(*columns[i] , key[i]);
        }
        for (size_t i = 0; i < states.size(); ++i) {
            states[i]->AppendResult(*columns[group_by_.size() + i]);
        }
    }

    out.SetScheme(scheme);
    for (auto& column : columns) {
        out.AddColumn(std::move(column));
    }
    out.InitMsk();
}

// LLM work done!


bool LimitExecutor::Next(Batch& data) {
    if (!child) {
        throw std::runtime_error("No child in limit executor!");
    }
    if (gived_ == limit_) {
        return false;
    }
    while (child->Next(data)) {
        if (data.GetMsk().count() != data.GetRows()) {
            throw std::runtime_error("LimitExecutor input batch has not full mask!");
        }
        size_t rows = data.GetRows();
        if (skipped_ + rows <= offset_) {
            skipped_ += rows;
            continue;
        }
        size_t drop_front = skipped_ < offset_ ? offset_ - skipped_ : 0;
        skipped_ += drop_front;
        size_t available = rows - drop_front;
        size_t need = limit_ - gived_;
        size_t take = std::min(available , need);
        if (take == 0) {
            return false;
        }
        size_t drop_back = available - take;
        Batch result;
        result.SetScheme(data.GetScheme());
        for (size_t column_index = 0; column_index < data.Size(); ++column_index) {
            std::shared_ptr<Column> column = data.GetColumn(column_index);
            if (drop_front != 0) {
                column = SliceColumn(std::move(column) , drop_front , result.GetStringArena());
            }
            if (drop_back != 0) {
                column = SliceColumn(std::move(column) , drop_back , result.GetStringArena() , false);
            }
            result.AddColumn(std::move(column));
        }
        result.SetRows(take);
        result.InitMsk();
        gived_ += take;
        std::swap(result , data);
        return true;
    }
    return false;
}

std::shared_ptr<Column> LimitExecutor::SliceColumn(std::shared_ptr<Column> column , size_t del , Utility::StringArena* arena , bool front) {
    if (!column) {
        throw std::runtime_error("Can't slice null column!");
    }
    const size_t size = column->Size();
    if (del > size) {
        throw std::runtime_error("Can't slice more rows than column contains!");
    }
    if (del == 0) {
        return column;
    }
    size_t begin = front ? del : 0;
    size_t end = front ? size : size - del;
    std::shared_ptr<Column> result = MakeColumn(column->GetType() , arena);
    for (size_t row = begin; row < end; ++row) {
        AppendScalarValue(*result , column->GetScalarValue(row));
    }
    return result;
}

bool OrderByExecutor::Next(Batch& data) {
    if (!child) {
        throw std::runtime_error("No child in OrderByExecutor!");
    }
    if (finished_) {
        return false;
    }
    if (limit_ == 0) {
        data.Clear();
        finished_ = true;
        return false;
    }
    TopKSort sorter(limit_ , directions_);
    Batch input;
    while (child->Next(input)) {
        sorter.UpdateBatch(order_expr_ , input);
    }
    data = sorter.Result();
    finished_ = true;
    return data.GetRows() != 0;
}
