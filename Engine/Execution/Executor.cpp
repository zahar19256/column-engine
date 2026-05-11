#include "Executor.h"

std::string GlobalAggregationExecutor::DefaultOutputName(const Aggregation::AggregationCall& call , size_t index) const {
    if (call.new_name.has_value()) {
        return *call.new_name;
    }
    if (call.column_name.has_value()) {
        return *call.column_name;
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

        auto column = MakeColumn(result_type);
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
