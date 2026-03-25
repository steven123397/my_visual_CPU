#pragma once

#include <cstdint>

struct InsnEffects;
class CPU;

extern "C" {
#include "../decode.h"
}

InsnEffects build_integer_effects(const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm, uint64_t pc);
bool execute_integer_instruction(CPU& cpu, const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm, uint64_t pc);
