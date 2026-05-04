#pragma once
#include "../Planer/Base.h"

class Executor {
    virtual ~Executor() = default;
    virtual bool Next(Batch& data) const = 0;
};