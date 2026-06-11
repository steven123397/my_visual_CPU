#pragma once

#include <stdint.h>

#include "linux_compat.h"

void linux_compat_debug_syscall_failure(
    const linux_compat_runtime_t* runtime,
    const linux_compat_syscall_request_t* request,
    int64_t value,
    const linux_compat_trace_t* trace);

void linux_compat_debug_syscall_success(
    const linux_compat_runtime_t* runtime,
    const linux_compat_syscall_request_t* request,
    int64_t value,
    const linux_compat_trace_t* trace);
