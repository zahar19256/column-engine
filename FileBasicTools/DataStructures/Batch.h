#pragma once
#include "Scheme.h"
#include "Column.h"
#include "Row.h"
#include <vector>
#include <memory>

class Batch {
public:
    Batch(const std::vector<Row<std::string>>& chunk , const Scheme& scheme) : scheme_(scheme) {
        ChunkToBatch(chunk);
    }
private:
    void ChunkToBatch(const std::vector<Row<std::string>>& chunk);
    Scheme scheme_;
    std::vector <std::shared_ptr<Column>> columns_;
};