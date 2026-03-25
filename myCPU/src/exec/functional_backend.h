#pragma once

#include "backend.h"

class CPU;
class Bus;

class FunctionalBackend : public ExecutionBackend {
public:
    FunctionalBackend(CPU& cpu, Bus& bus);

    void step() override;
    const char* name() const override;

private:
    CPU& cpu_;
    Bus& bus_;
};
