#pragma once
#include "cpu.h"
#include "memory.h"

void trap_enter(CPU *cpu, uint64_t cause, uint64_t tval);
void trap_return(CPU *cpu);  // MRET
