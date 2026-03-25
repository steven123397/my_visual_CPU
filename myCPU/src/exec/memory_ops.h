#pragma once

struct InsnEffects;
class CPU;
class Bus;

extern "C" {
#include "../decode.h"
}

InsnEffects build_memory_effects(const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm);
uint64_t extend_loaded_value(uint64_t value, int size, bool sign_extend);
bool apply_memory_effects(CPU& cpu, Bus& bus, const InsnEffects& effects);
bool execute_memory_instruction(CPU& cpu, Bus& bus, const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm);
