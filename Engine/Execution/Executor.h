#pragma once
#include "../../FileBasicTools/src/BelZReader.h"
#include "../../FileBasicTools/DataStructures/Utility.h"
#include "../Functions/Aggregation.h"
#include "../Functions/Expression.h"
#include "../Functions/TopK.h"
#include "../Helpers/AggrBuilder.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <absl/container/flat_hash_map.h>
#include <utility>
#include <vector>

using AggrState = std::vector<std::unique_ptr<Aggregation::AggregationState>>;

class Executor {
public:
    virtual ~Executor() = default;
    virtual bool Next(Batch& data) = 0;
    virtual double ReadMillis() const {
        return child ? child->ReadMillis() : 0.0;
    }
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
        const auto start = std::chrono::steady_clock::now();
        reader_.ReadBatch(out , column_names_);
        const auto finish = std::chrono::steady_clock::now();
        read_ms_ += std::chrono::duration<double , std::milli>(finish - start).count();
        out.InitMsk();
        return true;
    }
    double ReadMillis() const override {
        return read_ms_;
    }
private:
    BelZReader reader_;
    std::vector<std::string> column_names_;
    double read_ms_ = 0.0;
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
            result.push_back(MakeAggregationState(call));
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
            Utility::StringArena expr_arena;
            EvalContext env(&expr_arena);
            for (size_t i = 0; i < states_.size(); ++i) {
                const std::shared_ptr<Column> column = calls_[i].expression ? calls_[i].expression->EvalBatch(input , env) : nullptr;
                states_[i]->UpdateBatch(column.get() , input.GetRows() , input.GetMsk());
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

class GroupByAggregationExecutor : public AggregationExecutorBase {
public:
    explicit GroupByAggregationExecutor(std::vector<std::shared_ptr<ScalarExpr>> group_by,
          std::vector<Aggregation::AggregationCall> calls)
        : AggregationExecutorBase(std::move(calls)), group_by_(std::move(group_by)) {
    }
    bool Next(Batch& data) override;
private:
    std::string DefaultGroupByOutputName(const std::shared_ptr<ScalarExpr>& expr , size_t index) const;
    std::string DefaultOutputName(const Aggregation::AggregationCall& call , size_t index) const;
    ColumnType GroupByOutputType(size_t index) const;
    ColumnType AggregationOutputType(size_t index) const;
    void OutputBatch(Batch& out) const;

    std::vector<std::shared_ptr<ScalarExpr>> group_by_;
    absl::flat_hash_map <Utility::GroupKey , AggrState , Utility::GroupHash> storage_;
    bool finished_ = false;
    Utility::StringArena arena_;
};

class ProjectionExecutor : public Executor {
public:
    explicit ProjectionExecutor(
        std::vector<std::string> need_columns,
        std::vector<std::pair<std::string , std::string>> alias = {})
        : need_columns_(std::move(need_columns)), alias_(std::move(alias)) {
        need_indexes_.resize(need_columns_.size());
    }

    bool Next(Batch& data) override {
        if (!child) {
            throw std::runtime_error("No child in Projection executor!");
        }
        if (!child->Next(data)) {
            return false;
        }
        Batch result;
        Scheme scheme;
        std::vector<std::pair<std::string , std::string>> source_aliases;
        for (size_t i = 0; i < need_columns_.size(); ++i) {
            if (need_indexes_[i] == std::nullopt) {
                need_indexes_[i] = data.GetScheme().GetIndex(need_columns_[i]);
            }
            result.AddColumn(data.GetColumn(need_indexes_[i].value()));
            SchemeNode info = data.GetScheme().GetInfo(need_indexes_[i].value());
            for (const auto& [source , alias] : alias_) {
                if (source == need_columns_[i]) {
                    info.name = alias;
                    source_aliases.emplace_back(alias , source);
                    break;
                }
            }
            scheme.Push_Back(std::move(info));
        }
        result.SetScheme(scheme);
        result.SetMsk(data.GetMsk());
        for (const auto& [current_name , alias] : source_aliases) {
            result.AddAlias(current_name , alias);
        }
        std::swap(result , data);
        return true;
    }
private:
    std::vector <std::string> need_columns_;
    std::vector <std::optional<size_t>> need_indexes_;
    std::vector<std::pair<std::string , std::string>> alias_;
};


class OrderByExecutor : public Executor {
public:
    explicit OrderByExecutor(
        std::vector<std::shared_ptr<ScalarExpr>> order_expr ,
        size_t limit ,
        std::vector<SortDirection> directions = {})
        : order_expr_(std::move(order_expr)) ,
          directions_(std::move(directions)) ,
          limit_(limit) {
    }
    bool Next(Batch& data) override;
private:
    std::vector<std::shared_ptr<ScalarExpr>> order_expr_;
    std::vector<SortDirection> directions_;
    bool finished_ = false;
    size_t limit_;
};

class LimitExecutor : public Executor {
public:
    explicit LimitExecutor(size_t limit , size_t offset = 0) : limit_(limit) , offset_(offset) {
    }
    bool Next(Batch& data) override;
private:
    std::shared_ptr<Column> SliceColumn(std::shared_ptr<Column> column , size_t del , Utility::StringArena* arena , bool front = true);
    size_t limit_;
    size_t offset_ = 0;
    size_t skipped_ = 0;
    size_t gived_ = 0;
    Batch result_;
    
};
