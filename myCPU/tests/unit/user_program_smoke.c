#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/memory.h"
#include "../../guest/include/platform.h"
#include "../../guest/include/user_program_smoke.h"

static uintptr_t g_text_start = MEM_BASE;
static uintptr_t g_text_end = MEM_BASE + 2U * MEMORY_PAGE_SIZE;
static int g_plan_standard_calls = 0;
static uintptr_t g_last_exec_symbol = 0;
static uintptr_t g_last_ecall_symbol = 0;
static bool g_plan_standard_result = true;

static void reset_stub_state(void);
static int fail(const char* message);
static int test_smoke_init_and_plan_wrapper(void);
static int test_validate_standard_plan(void);

bool user_program_plan_standard(user_program_t* program,
                                uintptr_t exec_symbol,
                                uintptr_t ecall_symbol) {
    g_plan_standard_calls += 1;
    g_last_exec_symbol = exec_symbol;
    g_last_ecall_symbol = ecall_symbol;
    if (!g_plan_standard_result || program == NULL) {
        return false;
    }

    program->bootstrap.planned = true;
    return true;
}

uintptr_t user_program_value(const user_program_t* program,
                             user_program_value_id_t value_id) {
    if (program == NULL || !program->bootstrap.planned) {
        return 0;
    }

    switch (value_id) {
    case USER_PROGRAM_VALUE_EXEC_PAGE_PADDR:
        return program->bootstrap.exec_page_paddr;
    case USER_PROGRAM_VALUE_EXEC_VADDR:
        return program->bootstrap.exec_vaddr;
    case USER_PROGRAM_VALUE_STACK_VADDR:
        return program->bootstrap.stack_vaddr;
    case USER_PROGRAM_VALUE_ALIAS_VADDR:
        return program->bootstrap.alias_vaddr;
    case USER_PROGRAM_VALUE_ANON_VADDR:
        return program->bootstrap.anon_vaddr;
    case USER_PROGRAM_VALUE_ANON_TAIL_VADDR:
        return program->bootstrap.anon_tail_vaddr;
    case USER_PROGRAM_VALUE_ENTRY_PC:
        return program->bootstrap.entry_pc;
    case USER_PROGRAM_VALUE_EXPECTED_ECALL_PC:
        return program->bootstrap.expected_ecall_pc;
    case USER_PROGRAM_VALUE_USER_SP:
        return program->bootstrap.user_sp;
    }

    return 0;
}

uintptr_t memory_kernel_start(void) {
    return MEM_BASE;
}

uintptr_t memory_text_start(void) {
    return g_text_start;
}

uintptr_t memory_text_end(void) {
    return g_text_end;
}

static void reset_stub_state(void) {
    g_text_start = MEM_BASE;
    g_text_end = MEM_BASE + 2U * MEMORY_PAGE_SIZE;
    g_plan_standard_calls = 0;
    g_last_exec_symbol = 0;
    g_last_ecall_symbol = 0;
    g_plan_standard_result = true;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int test_smoke_init_and_plan_wrapper(void) {
    user_program_smoke_t smoke;
    user_program_t program;

    reset_stub_state();
    memset(&smoke, 0xA5, sizeof(smoke));
    memset(&program, 0, sizeof(program));
    user_program_smoke_init(&smoke);

    if (smoke.program != NULL || smoke.remap_region.address_space != NULL ||
        smoke.remap_region.registered || smoke.remap_region.object != NULL ||
        smoke.invalid_region.address_space != NULL ||
        smoke.invalid_region.registered || smoke.invalid_region.object != NULL ||
        smoke.remap_object.initialized ||
        smoke.remap_object.backing_kind != VM_OBJECT_BACKING_NONE) {
        return fail("expected smoke init to clear scratch regions and object state");
    }

    if (!user_program_smoke_plan_standard(&program, 0x1234U, 0x5678U) ||
        g_plan_standard_calls != 1 || g_last_exec_symbol != 0x1234U ||
        g_last_ecall_symbol != 0x5678U) {
        return fail("expected smoke plan wrapper to forward program symbols");
    }

    return 0;
}

static int test_validate_standard_plan(void) {
    user_program_t program = {0};

    reset_stub_state();
    program.bootstrap.planned = true;
    program.bootstrap.exec_page_paddr = g_text_start;
    program.bootstrap.exec_vaddr = 4U * MEMORY_PAGE_SIZE;
    program.bootstrap.stack_vaddr = 5U * MEMORY_PAGE_SIZE;
    program.bootstrap.alias_vaddr = 6U * MEMORY_PAGE_SIZE;
    program.bootstrap.anon_vaddr = 7U * MEMORY_PAGE_SIZE;
    program.bootstrap.anon_tail_vaddr = 8U * MEMORY_PAGE_SIZE;
    program.bootstrap.entry_pc = program.bootstrap.exec_vaddr + 0x40U;
    program.bootstrap.expected_ecall_pc = program.bootstrap.exec_vaddr + 0x80U;
    program.bootstrap.user_sp = program.bootstrap.stack_vaddr + MEMORY_PAGE_SIZE;

    if (!user_program_smoke_validate_standard_plan(&program, 0, MEM_BASE)) {
        return fail("expected smoke plan validator to accept canonical layout");
    }

    program.bootstrap.user_sp = program.bootstrap.stack_vaddr;
    if (user_program_smoke_validate_standard_plan(&program, 0, MEM_BASE)) {
        return fail("expected smoke plan validator to reject bad user stack top");
    }

    program.bootstrap.user_sp = program.bootstrap.stack_vaddr + MEMORY_PAGE_SIZE;
    program.bootstrap.exec_page_paddr = g_text_end;
    if (user_program_smoke_validate_standard_plan(&program, 0, MEM_BASE)) {
        return fail("expected smoke plan validator to reject exec page outside text");
    }

    return 0;
}

int main(void) {
    if (test_smoke_init_and_plan_wrapper() != 0 ||
        test_validate_standard_plan() != 0) {
        return 1;
    }

    return 0;
}
