#pragma once

#include <stdint.h>

#include "linux_compat.h"

/* Linux compat 调试输出：在 host smoke 或 UART trace 中打印 syscall 成败上下文。 */
/* 打印一次 syscall 失败上下文（runtime/request/返回值/trace）。 */
void linux_compat_debug_syscall_failure(
    const linux_compat_runtime_t* runtime,
    const linux_compat_syscall_request_t* request,
    int64_t value,
    const linux_compat_trace_t* trace);

/* 打印一次 syscall 成功上下文。 */
void linux_compat_debug_syscall_success(
    const linux_compat_runtime_t* runtime,
    const linux_compat_syscall_request_t* request,
    int64_t value,
    const linux_compat_trace_t* trace);
