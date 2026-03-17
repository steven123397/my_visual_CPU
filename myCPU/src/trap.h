#pragma once

#include <cstdint>

#include "cpu.h"

void trap_enter(CPU& cpu, uint64_t cause, uint64_t tval);
void trap_return(CPU& cpu);
