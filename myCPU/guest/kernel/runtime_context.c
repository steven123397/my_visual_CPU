#include "runtime_context.h"

#include "vm.h"

static vm_address_space_t* active_address_space = NULL;
static vm_process_t* active_process = NULL;
static trap_context_t* active_trap_context = NULL;

void runtime_context_reset(void) {
    active_address_space = NULL;
    active_process = NULL;
    active_trap_context = NULL;
}

void runtime_context_activate_address_space(vm_address_space_t* address_space) {
    active_address_space = address_space;
    if (active_process != NULL && active_process->address_space != address_space) {
        active_process = NULL;
    }
}

bool runtime_context_address_space_is_active(
    const vm_address_space_t* address_space) {
    return address_space != NULL && address_space == active_address_space;
}

vm_address_space_t* runtime_context_active_address_space(void) {
    return active_address_space;
}

void runtime_context_clear_address_space(
    const vm_address_space_t* address_space) {
    if (address_space == NULL || active_address_space != address_space) {
        return;
    }

    active_address_space = NULL;
    if (active_process != NULL && active_process->address_space == address_space) {
        active_process = NULL;
    }
}

void runtime_context_activate_process(vm_process_t* process) {
    active_process = process;
    if (process == NULL) {
        active_address_space = NULL;
        return;
    }

    active_address_space = process->address_space;
}

bool runtime_context_process_is_active(const vm_process_t* process) {
    return process != NULL && process == active_process;
}

vm_process_t* runtime_context_active_process(void) {
    return active_process;
}

void runtime_context_clear_process(const vm_process_t* process) {
    if (process == NULL || active_process != process) {
        return;
    }

    active_process = NULL;
}

void runtime_context_activate_trap_context(trap_context_t* trap_context) {
    active_trap_context = trap_context;
}

bool runtime_context_trap_context_is_active(const trap_context_t* trap_context) {
    return trap_context != NULL && trap_context == active_trap_context;
}

trap_context_t* runtime_context_active_trap_context(void) {
    return active_trap_context;
}

void runtime_context_clear_trap_context(const trap_context_t* trap_context) {
    if (trap_context == NULL || active_trap_context != trap_context) {
        return;
    }

    active_trap_context = NULL;
}
