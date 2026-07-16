#include "TopK.h"

void DispatchTopK(std::shared_ptr<Column> col, std::vector<size_t>& index, std::vector<size_t>& classes, size_t limit,
                  SortDirection direction) {
    switch (col->GetType()) {
    case ColumnType::Int8:
        TypedTopK<Int8Column>(As<Int8Column>(col), index, classes, limit, direction);
        break;
    case ColumnType::Int16:
        TypedTopK<Int16Column>(As<Int16Column>(col), index, classes, limit, direction);
        break;
    case ColumnType::Int32:
        TypedTopK<Int32Column>(As<Int32Column>(col), index, classes, limit, direction);
        break;
    case ColumnType::Int64:
        TypedTopK<Int64Column>(As<Int64Column>(col), index, classes, limit, direction);
        break;
    case ColumnType::Int128:
        TypedTopK<Int128Column>(As<Int128Column>(col), index, classes, limit, direction);
        break;
    case ColumnType::Double:
        TypedTopK<DoubleColumn>(As<DoubleColumn>(col), index, classes, limit, direction);
        break;
    case ColumnType::Date:
        TypedTopK<DateColumn>(As<DateColumn>(col), index, classes, limit, direction);
        break;
    case ColumnType::Timestamp:
        TypedTopK<TimeStampColumn>(As<TimeStampColumn>(col), index, classes, limit, direction);
        break;
    case ColumnType::String:
        TypedTopK<StringColumn>(As<StringColumn>(col), index, classes, limit, direction);
        break;
    case ColumnType::FlatString:
    case ColumnType::Unknown:
        throw std::runtime_error("Unknown column type in TopK dispatcher!");
    }
}

namespace {

template <typename ColumnT>
void AppendTypedValue(const std::shared_ptr<Column>& source, const std::shared_ptr<Column>& target, size_t row) {
    auto typed_source = As<ColumnT>(source);
    auto typed_target = As<ColumnT>(target);
    if (!typed_source || !typed_target) {
        throw std::runtime_error("TopK column type mismatch!");
    }
    if constexpr (std::is_same_v<ColumnT, StringColumn>) {
        const std::string_view value = typed_source->At_view(row);
        typed_target->AppendFromString(value.data(), value.size());
    } else {
        typed_target->Push_Back(typed_source->At(row));
    }
}

void AppendColumnValue(const std::shared_ptr<Column>& source, const std::shared_ptr<Column>& target, size_t row) {
    if (!source || !target) {
        throw std::runtime_error("Null column in TopK materialization!");
    }
    if (row >= source->Size()) {
        throw std::runtime_error("TopK row index is out of range!");
    }
    switch (source->GetType()) {
    case ColumnType::Int8:
        AppendTypedValue<Int8Column>(source, target, row);
        return;
    case ColumnType::Int16:
        AppendTypedValue<Int16Column>(source, target, row);
        return;
    case ColumnType::Int32:
        AppendTypedValue<Int32Column>(source, target, row);
        return;
    case ColumnType::Int64:
        AppendTypedValue<Int64Column>(source, target, row);
        return;
    case ColumnType::Int128:
        AppendTypedValue<Int128Column>(source, target, row);
        return;
    case ColumnType::String:
        AppendTypedValue<StringColumn>(source, target, row);
        return;
    case ColumnType::Double:
        AppendTypedValue<DoubleColumn>(source, target, row);
        return;
    case ColumnType::Date:
        AppendTypedValue<DateColumn>(source, target, row);
        return;
    case ColumnType::Timestamp:
        AppendTypedValue<TimeStampColumn>(source, target, row);
        return;
    case ColumnType::FlatString:
    case ColumnType::Unknown:
        break;
    }
    throw std::runtime_error("Unknown column type in TopK materialization!");
}

int CompareScalar(const Utility::ScalarValue& left, const Utility::ScalarValue& right) {
    if (left.index() != right.index()) {
        return left.index() < right.index() ? -1 : 1;
    }
    return std::visit(
        [](const auto& l, const auto& r) -> int {
            using LeftT = std::decay_t<decltype(l)>;
            using RightT = std::decay_t<decltype(r)>;
            if constexpr (!std::is_same_v<LeftT, RightT>) {
                return 0;
            } else {
                if (l < r) {
                    return -1;
                }
                if (r < l) {
                    return 1;
                }
                return 0;
            }
        },
        left, right);
}

bool RowBetter(const Batch& left, size_t left_row, const Batch& right, size_t right_row,
               const std::vector<SortDirection>& directions) {
    const size_t size = std::min(left.Size(), right.Size());
    for (size_t i = 0; i < size; ++i) {
        int cmp =
            CompareScalar(left.GetColumn(i)->GetScalarValue(left_row), right.GetColumn(i)->GetScalarValue(right_row));
        if (cmp != 0) {
            const SortDirection direction = i < directions.size() ? directions[i] : SortDirection::Asc;
            if (direction == SortDirection::Desc) {
                cmp = -cmp;
            }
            return cmp < 0;
        }
    }
    if (left.Size() != right.Size()) {
        return left.Size() < right.Size();
    }
    return false;
}

std::vector<std::shared_ptr<Column>> MakeColumns(const Batch& batch, size_t rows, Utility::StringArena* arena) {
    std::vector<std::shared_ptr<Column>> result;
    result.reserve(batch.Size());
    for (size_t column_index = 0; column_index < batch.Size(); ++column_index) {
        auto column = MakeColumn(batch.GetType(column_index), arena);
        column->Reserve(rows);
        result.push_back(std::move(column));
    }
    return result;
}

std::vector<std::shared_ptr<Column>> MakeKeyColumns(const std::vector<std::shared_ptr<Column>>& keys, size_t rows,
                                                    Utility::StringArena* arena) {
    std::vector<std::shared_ptr<Column>> result;
    result.reserve(keys.size());
    for (const auto& key : keys) {
        if (!key) {
            throw std::runtime_error("Null TopK key column!");
        }
        auto column = MakeColumn(key->GetType(), arena);
        column->Reserve(rows);
        result.push_back(std::move(column));
    }
    return result;
}

Scheme MakeKeyScheme(const std::vector<std::shared_ptr<Column>>& keys) {
    Scheme scheme;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (!keys[i]) {
            throw std::runtime_error("Null TopK key column!");
        }
        scheme.Push_Back(SchemeNode{"key_" + std::to_string(i), keys[i]->GetType()});
    }
    return scheme;
}

void BuildBatch(Batch& batch, const Scheme& scheme, std::vector<std::shared_ptr<Column>>& columns, size_t rows) {
    batch.SetScheme(scheme);
    for (auto& column : columns) {
        batch.AddColumn(std::move(column));
    }
    if (columns.empty()) {
        batch.SetRows(rows);
    }
    batch.InitMsk();
}

void AppendRow(const Batch& source, size_t row, const std::vector<std::shared_ptr<Column>>& target_columns) {
    for (size_t column_index = 0; column_index < source.Size(); ++column_index) {
        AppendColumnValue(source.GetColumn(column_index), target_columns[column_index], row);
    }
}

void AppendTopKRow(const TopKBatch& source, size_t row, const std::vector<std::shared_ptr<Column>>& data_columns,
                   const std::vector<std::shared_ptr<Column>>& key_columns) {
    AppendRow(source.data, row, data_columns);
    AppendRow(source.keys, row, key_columns);
}

} // namespace

void MaterializeTopK(const Batch& data, const std::vector<std::shared_ptr<Column>>& keys,
                     const std::vector<size_t>& index, TopKBatch& result, size_t limit) {
    result.data.Clear();
    result.keys.Clear();
    const size_t result_size = std::min(limit, index.size());
    std::vector<std::shared_ptr<Column>> data_columns = MakeColumns(data, result_size, result.data.GetStringArena());
    std::vector<std::shared_ptr<Column>> key_columns = MakeKeyColumns(keys, result_size, result.keys.GetStringArena());
    for (size_t pos = 0; pos < result_size; ++pos) {
        const size_t row_index = index[pos];
        if (row_index >= data.GetRows()) {
            throw std::runtime_error("TopK materialize row index is out of range!");
        }
        for (size_t column_index = 0; column_index < data.Size(); ++column_index) {
            AppendColumnValue(data.GetColumn(column_index), data_columns[column_index], row_index);
        }
        for (size_t key_index = 0; key_index < keys.size(); ++key_index) {
            AppendColumnValue(keys[key_index], key_columns[key_index], row_index);
        }
    }
    BuildBatch(result.data, data.GetScheme(), data_columns, result_size);
    BuildBatch(result.keys, MakeKeyScheme(keys), key_columns, result_size);
}

void Merge(TopKBatch& left, TopKBatch& right, size_t limit, const std::vector<SortDirection>& directions) {
    if (limit == 0) {
        left.data.Clear();
        left.keys.Clear();
        right.data.Clear();
        right.keys.Clear();
        return;
    }
    if (left.data.GetRows() == 0) {
        left = std::move(right);
        return;
    }
    if (right.data.GetRows() == 0) {
        return;
    }
    TopKBatch result;
    const size_t result_limit = std::min(limit, left.data.GetRows() + right.data.GetRows());
    std::vector<std::shared_ptr<Column>> data_columns =
        MakeColumns(left.data, result_limit, result.data.GetStringArena());
    std::vector<std::shared_ptr<Column>> key_columns =
        MakeColumns(left.keys, result_limit, result.keys.GetStringArena());
    size_t left_index = 0;
    size_t right_index = 0;
    size_t result_size = 0;
    while (result_size < limit && (left_index < left.data.GetRows() || right_index < right.data.GetRows())) {
        if (right_index == right.data.GetRows()) {
            AppendTopKRow(left, left_index++, data_columns, key_columns);
            ++result_size;
            continue;
        }
        if (left_index == left.data.GetRows()) {
            AppendTopKRow(right, right_index++, data_columns, key_columns);
            ++result_size;
            continue;
        }
        if (RowBetter(right.keys, right_index, left.keys, left_index, directions)) {
            AppendTopKRow(right, right_index++, data_columns, key_columns);
        } else {
            AppendTopKRow(left, left_index++, data_columns, key_columns);
        }
        ++result_size;
    }
    BuildBatch(result.data, left.data.GetScheme(), data_columns, result_size);
    BuildBatch(result.keys, left.keys.GetScheme(), key_columns, result_size);
    left = std::move(result);
    right.data.Clear();
    right.keys.Clear();
}
