#pragma once

#include "backend.h"
#include "execution_profile.h"
#include "../debug/memory_capture.h"

class CPU;
class Bus;

class FunctionalBackend : public ExecutionBackend {
public:
    FunctionalBackend(CPU& cpu, Bus& bus);

    void step() override;
    const char* name() const override;
    BackendDebugSnapshot debug_snapshot() const override;
    void synchronize_debug_gpr_write() override {}
    void set_memory_receiver(MemoryRecordReceiver* receiver) { memory_receiver_ = receiver; }

private:
    CPU& cpu_;
    Bus& bus_;
    ExecutionProfile profile_{};
    MemoryRecordReceiver* memory_receiver_{nullptr};
};
