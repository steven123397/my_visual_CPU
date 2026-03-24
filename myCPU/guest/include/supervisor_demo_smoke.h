#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "trap.h"

bool supervisor_demo_smoke_run(trap_context_t* trap_context,
                               uintptr_t exec_symbol,
                               uintptr_t ecall_symbol);
