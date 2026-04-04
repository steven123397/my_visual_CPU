#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "trap.h"

bool supervisor_demo_smoke_run(trap_context_t* trap_context,
                               uintptr_t exec_symbol,
                               uintptr_t ecall_symbol);

#ifdef UNIT_TEST_HOST
typedef struct SupervisorDemoSmokePages {
    uint32_t* backing_page;
    uint32_t* remap_page;
    uint32_t* nx_page;
    uint32_t* user_stack_page;
    uint8_t* user_trap_stack_page;
} supervisor_demo_smoke_pages_t;

bool supervisor_demo_smoke_alloc_pages_for_test(
    supervisor_demo_smoke_pages_t* pages);
#endif
