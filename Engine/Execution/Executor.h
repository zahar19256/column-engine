#pragma once
#include "../../FileBasicTools/src/BelZReader.h"
#include "../../FileBasicTools/DataStructures/Batch.h"
#include "../Functions/Aggregation.h"
#include "../Functions/Expression.h"

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
        if (column_names_.empty()) {
            reader_.ReadBatch(out);
        } else {
            reader_.ReadBatch(out , column_names_);
        }
        out.InitMsk();
        return true;
    }
private:
    BelZReader reader_;
    std::vector<std::string> column_names_;
};

class FilterExecutor : public Executor {
public:
    bool Next(Batch& data) override {
        child->Next(data_);
    }
private:
    Batch data_;// TODO надо сделать BatchView чтобы без копирований.
    std::shared_ptr<Expr> filter_expr_;
};

class AgregationExecutor : public Executor {
public:
    bool Next(Batch& data) override {
        
    }
private:
    bool blocking_ = false;
    Aggregation::AggregationCall call_;
};