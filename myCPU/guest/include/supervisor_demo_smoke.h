#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "trap.h"
#include "user_program_smoke.h"

typedef struct SupervisorDemoSmokePages {
    uint32_t* backing_page;
    uint32_t* remap_page;
    uint32_t* nx_page;
    uint32_t* user_stack_page;
    uint8_t* user_trap_stack_page;
} supervisor_demo_smoke_pages_t;

typedef struct SupervisorDemoSmokeState {
    volatile uint64_t timer_irq_seen;
    volatile uint64_t external_irq_seen;
    volatile uint64_t user_ecall_seen;
    uint32_t* user_data_page;
    uint32_t expected_user_data;
    uint32_t expected_user_timer;
    uint32_t expected_user_external;
} supervisor_demo_smoke_state_t;

void supervisor_demo_smoke_init(supervisor_demo_smoke_state_t* state,
                                uint32_t* user_data_page,
                                uint32_t expected_user_data,
                                uint32_t expected_user_timer,
                                uint32_t expected_user_external);
void supervisor_demo_smoke_init_active_phase(
    user_program_smoke_active_phase_t* phase,
    const volatile uint32_t* rodata_marker,
    volatile uintptr_t* fault_resume_pc_slot);
bool supervisor_demo_smoke_validate_memory_layout(
    void* early_allocation,
    const volatile uint32_t* rodata_marker);
bool supervisor_demo_smoke_validate_pmm_setup(uintptr_t early_cursor);
bool supervisor_demo_smoke_probe_storage_page(void);
bool supervisor_demo_smoke_alloc_pages(supervisor_demo_smoke_pages_t* pages);
bool supervisor_demo_smoke_prime_active_pages(
    supervisor_demo_smoke_pages_t* pages,
    user_program_smoke_active_phase_t* phase);
bool supervisor_demo_smoke_prepare_user_entry(
    supervisor_demo_smoke_state_t* state);
void supervisor_demo_smoke_timer_interrupt_handler(uint64_t cause,
                                                   void* context);
void supervisor_demo_smoke_external_interrupt_handler(uint64_t cause,
                                                      uint32_t source_id,
                                                      void* context);
bool supervisor_demo_smoke_user_ecall_validate(
    const trap_user_runtime_t* user_runtime,
    uint64_t epc,
    uint64_t tval,
    void* context);
bool supervisor_demo_smoke_verify_user_return(
    const supervisor_demo_smoke_state_t* state,
    const trap_user_runtime_t* user_runtime);
bool supervisor_demo_smoke_read_storage_signature(uint8_t* storage_buffer);
bool supervisor_demo_smoke_wait_platform_interrupts(
    supervisor_demo_smoke_state_t* state,
    uint64_t timer_delta);
bool supervisor_demo_smoke_run_platform_tail(
    supervisor_demo_smoke_state_t* state,
    uint64_t timer_delta);
