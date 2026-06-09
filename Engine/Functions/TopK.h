#pragma once
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include "../../FileBasicTools/DataStructures/Batch.h"
#include "../../FileBasicTools/DataStructures/Utility.h"
#include "../Operators/ScalarOperators.h"
#include "Column.h"
#include "Filter.h"
#include "Functions/Expression.h"
#include "Scheme.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

struct TopKBatch {
    Batch data;
    Batch keys;
};

enum class SortDirection {
    Asc,
    Desc
};

void MaterializeTopK(
    const Batch& data ,
    const std::vector<std::shared_ptr<Column>>& keys ,
    const std::vector<size_t>& index ,
    TopKBatch& result ,
    size_t limit);

void Merge(
    TopKBatch& left ,
    TopKBatch& right ,
    size_t limit ,
    const std::vector<SortDirection>& directions);

template <typename ColumnT>
inline void TypedTopK(std::shared_ptr<ColumnT> column , std::vector<size_t>& index , std::vector<size_t>& classes , size_t limit , SortDirection direction = SortDirection::Asc) {
    if (!column) {
        throw std::runtime_error("Null column in TypedTopK!");
    }
    if (index.size() != classes.size()) {
        throw std::runtime_error("TopK index/classes size mismatch!");
    }
    if (limit == 0 || index.empty()) {
        index.clear();
        classes.clear();
        return;
    }
    const size_t need = std::min(limit , index.size());
    auto value_less = [&](size_t left_pos , size_t right_pos) {
        const auto left = column->At(index[left_pos]);
        const auto right = column->At(index[right_pos]);
        if (left == right) {
            return false;
        }
        if (direction == SortDirection::Desc) {
            return right < left;
        }
        return left < right;
    };
    auto value_equal = [&](size_t left_pos , size_t right_pos) {
        return column->At(index[left_pos]) == column->At(index[right_pos]);
    };
    auto better = [&](size_t left_pos , size_t right_pos) {
        if (classes[left_pos] != classes[right_pos]) {
            return classes[left_pos] < classes[right_pos];
        }
        if (value_equal(left_pos , right_pos)) {
            return false;
        }
        return value_less(left_pos , right_pos);
    };

    auto better_with_tie = [&](size_t left_pos , size_t right_pos) {
        if (better(left_pos , right_pos)) {
            return true;
        }
        if (better(right_pos , left_pos)) {
            return false;
        }
        return index[left_pos] < index[right_pos];
    };
    auto equivalent = [&](size_t left_pos , size_t right_pos) {
        return classes[left_pos] == classes[right_pos] && value_equal(left_pos , right_pos);
    };
    auto not_worse_than = [&](size_t left_pos , size_t right_pos) {
        return better(left_pos , right_pos) || equivalent(left_pos , right_pos);
    };

    std::vector<size_t> selected;
    selected.reserve(index.size());
    if (need == index.size()) {
        selected.resize(index.size());
        std::iota(selected.begin() , selected.end() , 0);
    } else {
        auto heap_compare = [&](size_t left_pos , size_t right_pos) {
            return better_with_tie(left_pos , right_pos);
        };
        std::priority_queue<size_t , std::vector<size_t> , decltype(heap_compare)> heap(heap_compare);
        for (size_t pos = 0; pos < index.size(); ++pos) {
            if (heap.size() < need) {
                heap.push(pos);
                continue;
            }
            if (better_with_tie(pos , heap.top())) {
                heap.pop();
                heap.push(pos);
            }
        }
        const size_t boundary_pos = heap.top();
        for (size_t pos = 0; pos < index.size(); ++pos) {
            if (not_worse_than(pos , boundary_pos)) {
                selected.push_back(pos);
            }
        }
    }

    std::sort(selected.begin() , selected.end() , better_with_tie);
    std::vector<size_t> next_index;
    std::vector<size_t> next_classes;
    next_index.reserve(selected.size());
    next_classes.reserve(selected.size());
    size_t current_class = 0;
    for (size_t out_pos = 0; out_pos < selected.size(); ++out_pos) {
        if (out_pos != 0 && !equivalent(selected[out_pos - 1] , selected[out_pos])) {
            ++current_class;
        }
        next_index.push_back(index[selected[out_pos]]);
        next_classes.push_back(current_class);
    }
    index = std::move(next_index);
    classes = std::move(next_classes);
}

void DispatchTopK(std::shared_ptr<Column> col , std::vector<size_t>& index , std::vector<size_t>& classes , size_t limit , SortDirection direction = SortDirection::Asc);

class TopKSort {
public:
    explicit TopKSort(size_t limit , std::vector<SortDirection> directions = {})
        : limit_(limit) , directions_(std::move(directions)) {
    }

    void UpdateBatch(const std::vector<std::shared_ptr<ScalarExpr>>& order , const Batch& data) {
        const auto& mask = data.GetMsk();
        std::vector<size_t> index;
        std::vector<size_t> classes;
        std::vector<std::shared_ptr<Column>> keys;
        index.reserve(data.GetRows());
        classes.reserve(data.GetRows());
        keys.reserve(order.size());
        Utility::StringArena expr_arena;
        EvalContext env{&expr_arena};
        bool use_mask = !mask.empty() && mask.count() != mask.size();
        for (size_t i = 0; i < data.GetRows(); ++i) {
            if (!use_mask || mask[i]) {
                index.push_back(i);
                classes.push_back(0);
            }
        }
        for (size_t expr_index = 0; expr_index < order.size(); ++expr_index) {
            std::shared_ptr<Column> col = order[expr_index]->EvalBatch(data , env);
            keys.push_back(col);
            const SortDirection direction =
                expr_index < directions_.size() ? directions_[expr_index] : SortDirection::Asc;
            DispatchTopK(col , index , classes , limit_ , direction);
        }
        if (index.size() > limit_) {
            index.resize(limit_);
            classes.resize(limit_);
        }
        TopKBatch batch_result;
        MaterializeTopK(data , keys , index , batch_result , limit_);
        Merge(result_ , batch_result , limit_ , directions_);
        index_ = std::move(index);
        classes_ = std::move(classes);

    }
    const std::vector<size_t>& Index() const {
        return index_;
    }
    const std::vector<size_t>& Classes() const {
        return classes_;
    }
    const Batch& Result() const {
        return result_.data;
    }
    Batch TakeResult() {
        return std::move(result_.data);
    }
    const Batch& Keys() const {
        return result_.keys;
    }
private:
    TopKBatch result_;
    size_t limit_;
    std::vector<SortDirection> directions_;
    std::vector<size_t> index_;
    std::vector<size_t> classes_;
};
