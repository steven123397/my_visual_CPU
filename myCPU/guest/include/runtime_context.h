#pragma once

#include <stdbool.h>

typedef struct VmAddressSpace vm_address_space_t;
typedef struct VmProcess vm_process_t;
typedef struct TrapContext trap_context_t;

void runtime_context_reset(void);
void runtime_context_activate_address_space(vm_address_space_t* address_space);
bool runtime_context_address_space_is_active(
    const vm_address_space_t* address_space);
vm_address_space_t* runtime_context_active_address_space(void);
void runtime_context_clear_address_space(
    const vm_address_space_t* address_space);
void runtime_context_activate_process(vm_process_t* process);
bool runtime_context_process_is_active(const vm_process_t* process);
vm_process_t* runtime_context_active_process(void);
void runtime_context_clear_process(const vm_process_t* process);
void runtime_context_activate_trap_context(trap_context_t* trap_context);
bool runtime_context_trap_context_is_active(const trap_context_t* trap_context);
trap_context_t* runtime_context_active_trap_context(void);
void runtime_context_clear_trap_context(const trap_context_t* trap_context);
