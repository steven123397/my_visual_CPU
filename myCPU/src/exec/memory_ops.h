#pragma once

class CPU;
class Bus;

extern "C" {
#include "../decode.h"
}

bool execute_memory_instruction(CPU& cpu, Bus& bus, const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm);
