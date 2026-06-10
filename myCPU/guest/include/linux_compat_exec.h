#pragma once

#include <stddef.h>
#include <stdint.h>

#include "linux_compat.h"
#include "linux_compat_loader.h"
#include "linux_compat_vm.h"
#include "trap.h"

linux_compat_result_t linux_compat_exec_load(
    linux_compat_vm_t* vm,
    const uint8_t* image,
    size_t image_size,
    const linux_compat_load_plan_t* plan,
    uintptr_t* out_entry_pc,
    linux_compat_trace_t* out_trace);

linux_compat_result_t linux_compat_exec_build_stack(
    linux_compat_vm_t* vm,
    const linux_compat_load_plan_t* plan,
    size_t argc,
    const char* const* argv,
    uintptr_t* out_user_sp,
    linux_compat_trace_t* out_trace);

linux_compat_result_t linux_compat_exec_enter(
    linux_compat_vm_t* vm,
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime,
    void* trap_stack_base,
    size_t trap_stack_size,
    uintptr_t entry_pc,
    uintptr_t user_sp,
    linux_compat_runtime_t* runtime,
    linux_compat_trace_t* out_trace);
