#pragma once
#include "../../FileBasicTools/src/BelZReader.h"
#include "../Functions/Aggregation.h"
#include "../Functions/Expression.h"
#include "../Helpers/Helpers.h"
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

using AggrState = std::vector<std::unique_ptr<Aggregation::AggregationState>>;
using GroupKey = std::vector<ScalarValue>;

struct HashGroupKey {
    size_t operator()(const GroupKey& key) const {
        size_t result = 0;
        for (const auto& value : key) {
            const size_t current = std::visit(
                [](const auto& item) {
                    return std::hash<std::decay_t<decltype(item)>>{}(item);
                },
                value);
            result ^= current + 0x9e3779b9 + (result << 6) + (result >> 2);
        }
        return result;
    }
};

class Executor {
public:
    virtual ~Executor() = default;
    virtual bool Next(Batch& data) = 0;
    std::shared_ptr<Executor> child;
};

class ScanExecutor : public Executor {
public:
    ScanExecutor(const std::string& table_name , const std::vector<std::string>& column_names): reader_(table_name), column_names_(column_names) {

    }
    bool Next(Batch& out) override {
        if (reader_.Empty()) {
            return false;
        }
        reader_.ReadBatch(out , column_names_);
        out.InitMsk();
        return true;
    }
private:
    BelZReader reader_;
    std::vector<std::string> column_names_;
};

class FilterExecutor : public Executor {
public:
    explicit FilterExecutor(std::shared_ptr<PredicateExpr> filter_expr)
        : filter_expr_(std::move(filter_expr)) {
    }

    bool Next(Batch& data) override {
        if (!child) {
            throw std::runtime_error("No child in Filter!");
        }
        if (!child->Next(data)) {
            return false;
        }
        if (!filter_expr_) {
            return true;
        }
        data.ApplyMsk(filter_expr_->Eval(data));
        return true;
    }
private:
    std::shared_ptr<PredicateExpr> filter_expr_;
};

class AggregationExecutorBase : public Executor {
protected:
    explicit AggregationExecutorBase(std::vector<Aggregation::AggregationCall> calls)
        : calls_(std::move(calls)) {
    }

    std::vector<std::unique_ptr<Aggregation::AggregationState>> MakeStates() const {
        std::vector<std::unique_ptr<Aggregation::AggregationState>> result;
        result.reserve(calls_.size());

        for (const auto& call : calls_) {
            result.push_back(Aggregation::MakeAggregationState(call));
        }

        return result;
    }

protected:
    std::vector<Aggregation::AggregationCall> calls_;
};

class GlobalAggregationExecutor : public AggregationExecutorBase {
public:
    explicit GlobalAggregationExecutor(std::vector<Aggregation::AggregationCall> calls)
        : AggregationExecutorBase(std::move(calls)) {
        states_ = MakeStates();
    }
    bool Next(Batch& data) override {
        if (finished_) {
            return false;
        }
        if (!child) {
            throw std::runtime_error("No child in GlobalAggregationExecutor!");
        }
        Batch input;
        while (child->Next(input)) {
            for (auto& state : states_) {
                state->UpdateBatch(input);
            }
        }
        OutputBatch(data);
        finished_ = true;
        return true;
    }
private:
    std::string DefaultOutputName(const Aggregation::AggregationCall& call , size_t index) const;
    void OutputBatch(Batch& out) const;

    bool finished_ = false;
    std::vector<std::unique_ptr<Aggregation::AggregationState>> states_;
};

class AggregationExecutor : public GlobalAggregationExecutor {
public:
    explicit AggregationExecutor(Aggregation::AggregationCall call)
        : GlobalAggregationExecutor(std::vector<Aggregation::AggregationCall>{std::move(call)}) {
    }
};

class GroupByAggregationExecutor : public AggregationExecutorBase {
private:
    
    std::vector<std::shared_ptr<ScalarExpr>> group_by_;
    std::unordered_map <GroupKey , AggrState , HashGroupKey> data_;
};
