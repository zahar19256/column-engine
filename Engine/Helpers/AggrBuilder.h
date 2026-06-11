#pragma once
#include "../Functions/Aggregation.h"
#include "../Functions/Expression.h"
#include <memory>

using namespace Aggregation;

AggregationCall MakeAggrCall(AggregationType cur_type, std::optional<std::string> col_name,
                             std::optional<std::string> alias, ColumnType in_type, ColumnType out_type);
AggregationCall MakeAggrCall(AggregationType cur_type, std::shared_ptr<ScalarExpr> expression,
                             std::optional<std::string> alias, ColumnType in_type, ColumnType out_type);

std::unique_ptr<AggregationState> MakeAggregationState(const AggregationCall& call);
