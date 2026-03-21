#pragma once

class CPU;

extern "C" {
#include "../decode.h"
}

bool execute_integer_instruction(CPU& cpu, const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm, uint64_t pc);
