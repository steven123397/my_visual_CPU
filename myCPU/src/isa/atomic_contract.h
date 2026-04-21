#pragma once

#include "effects.h"

class CPU;
class Bus;

extern "C" {
#include "../decode.h"
}

struct AtomicApplyResult {
    bool ok{false};
    TrapRequest trap{};
    RegWrite rd_write{};
    bool platform_state_changed{false};
};

InsnEffects build_atomic_effects(const Insn& insn, uint64_t rs1v, uint64_t rs2v);
AtomicApplyResult apply_atomic_request(CPU& cpu, Bus& bus, const AtomicRequest& request);
