#pragma once

class CPU;

extern "C" {
#include "../decode.h"
}

bool execute_system_instruction(CPU& cpu, const Insn& insn);
