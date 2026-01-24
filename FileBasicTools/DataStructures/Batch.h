#pragma once
#include "Scheme.h"
#include "Column.h"
#include <vector>
#include <memory>

class Batch {

private:
    Scheme scheme_;
    std::vector <std::shared_ptr<Column>> columns_;
};