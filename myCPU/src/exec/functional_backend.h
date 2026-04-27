#pragma once

#include "backend.h"
#include "execution_profile.h"

class CPU;
class Bus;

class FunctionalBackend : public ExecutionBackend {
public:
    FunctionalBackend(CPU& cpu, Bus& bus);

    void step() override;
    const char* name() const override;
    BackendDebugSnapshot debug_snapshot() const override;

private:
    CPU& cpu_;
    Bus& bus_;
    ExecutionProfile profile_{};
};
