#pragma once

#include "../debug/debug_snapshot.h"

class ExecutionBackend {
public:
    virtual ~ExecutionBackend() = default;

    virtual void step() = 0;
    virtual const char* name() const = 0;
    virtual BackendDebugSnapshot debug_snapshot() const = 0;
};
