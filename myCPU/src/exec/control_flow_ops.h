#pragma once

#include <cstdint>

struct InsnEffects;
class CPU;

extern "C" {
#include "../decode.h"
}

InsnEffects build_control_flow_effects(const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm, uint64_t pc);
bool execute_control_flow_instruction(
    CPU& cpu,
    const Insn& insn,
    uint64_t rs1v,
    uint64_t rs2v,
    int64_t imm,
    uint64_t pc,
    uint64_t& next_pc);
