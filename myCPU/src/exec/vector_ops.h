#pragma once

#include <cstdint>

#include "../arch/vector_state.h"
#include "../isa/effects.h"

class Bus;
class CPU;

extern "C" {
#include "../decode.h"
}

bool is_vector_opcode(uint8_t opcode);
InsnEffects build_vector_effects(const Insn& insn, uint64_t rs1v);

struct VectorApplyResult {
    bool ok{true};
    TrapRequest trap{};
    bool platform_state_changed{false};
};

struct VectorComputeResult {
    bool ok{true};
    TrapRequest trap{};
    VectorState::VectorReg result{};
};

bool is_serializing_vector_insn(const Insn& insn);
bool is_non_memory_vector_alu_insn(const Insn& insn);
bool is_non_memory_vector_alu(const VectorRequest& request);
VectorComputeResult compute_vector_alu_result(const VectorState& vector,
                                              const VectorRequest& request);
VectorApplyResult apply_vector_request(CPU& cpu, Bus& bus, const VectorRequest& request);
