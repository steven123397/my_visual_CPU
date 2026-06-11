#include "linux_compat_debug.h"

#include "console.h"
#include "linux_compat_vm.h"

static void debug_console_puts(const char* value) {
    size_t i = 0;

    if (value == 0) {
        return;
    }
    while (value[i] != '\0') {
        console_putc(value[i]);
        i += 1U;
    }
}

static void debug_console_i64(int64_t value) {
    char digits[24];
    size_t used = 0;

    if (value < 0) {
        console_putc('-');
        value = -value;
    }
    if (value == 0) {
        console_putc('0');
        return;
    }
    while (value != 0 && used < sizeof(digits)) {
        digits[used] = (char)('0' + (value % 10));
        used += 1U;
        value /= 10;
    }
    while (used > 0) {
        used -= 1U;
        console_putc(digits[used]);
    }
}

static size_t debug_vm_region_count(const linux_compat_runtime_t* runtime) {
    size_t i = 0;
    size_t used = 0;

    if (runtime == 0 || runtime->vm == 0) {
        return 0;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (runtime->vm->regions[i].used) {
            used += 1U;
        }
    }
    return used;
}

static bool debug_str_contains(const char* haystack, const char* needle) {
    size_t i = 0;
    size_t j = 0;

    if (haystack == 0 || needle == 0 || needle[0] == '\0') {
        return false;
    }
    while (haystack[i] != '\0') {
        j = 0;
        while (haystack[i + j] != '\0' && needle[j] != '\0' &&
               haystack[i + j] == needle[j]) {
            j += 1U;
        }
        if (needle[j] == '\0') {
            return true;
        }
        i += 1U;
    }
    return false;
}

static bool debug_path_interesting(const char* path) {
    return debug_str_contains(path, ".git") ||
           debug_str_contains(path, "config") ||
           debug_str_contains(path, "HEAD") ||
           debug_str_contains(path, "template") ||
           debug_str_contains(path, "stage11repo");
}

static bool debug_success_interesting(
    const linux_compat_syscall_request_t* request,
    int64_t value,
    const linux_compat_trace_t* trace) {
    const char* path = trace != 0 ? trace->path : "";

    if (request == 0 || value < 0) {
        return false;
    }
    if (request->number == LINUX_COMPAT_SYS_OPENAT) {
        return true;
    }
    if (request->number == LINUX_COMPAT_SYS_READ) {
        return value == 0 || debug_path_interesting(path);
    }
    if (request->number == LINUX_COMPAT_SYS_LSEEK ||
        request->number == LINUX_COMPAT_SYS_NEWFSTATAT ||
        request->number == LINUX_COMPAT_SYS_FACCESSAT ||
        request->number == LINUX_COMPAT_SYS_READLINKAT ||
        request->number == LINUX_COMPAT_SYS_WRITE ||
        request->number == LINUX_COMPAT_SYS_WRITEV ||
        request->number == LINUX_COMPAT_SYS_PWRITE64 ||
        request->number == LINUX_COMPAT_SYS_FTRUNCATE ||
        request->number == LINUX_COMPAT_SYS_MKDIRAT ||
        request->number == LINUX_COMPAT_SYS_UNLINKAT ||
        request->number == LINUX_COMPAT_SYS_RENAMEAT ||
        request->number == LINUX_COMPAT_SYS_RENAMEAT2) {
        return debug_path_interesting(path);
    }
    return false;
}

void linux_compat_debug_syscall_failure(
    const linux_compat_runtime_t* runtime,
    const linux_compat_syscall_request_t* request,
    int64_t value,
    const linux_compat_trace_t* trace) {
    debug_console_puts("\nDBG syscall fail nr=");
    debug_console_i64(request != 0 ? (int64_t)request->number : -1);
    debug_console_puts(" ret=");
    debug_console_i64(value);
    debug_console_puts(" flags=");
    debug_console_i64(request != 0 ? (int64_t)request->flags : 0);
    if (request != 0 && request->number == LINUX_COMPAT_SYS_MMAP) {
        debug_console_puts(" addr=");
        debug_console_i64((int64_t)request->addr);
        debug_console_puts(" len=");
        debug_console_i64((int64_t)request->length);
        debug_console_puts(" prot=");
        debug_console_i64((int64_t)request->prot);
        debug_console_puts(" fd=");
        debug_console_i64((int64_t)request->fd);
        debug_console_puts(" off=");
        debug_console_i64((int64_t)request->offset);
        debug_console_puts(" vm_regions=");
        debug_console_i64((int64_t)debug_vm_region_count(runtime));
        debug_console_puts("/");
        debug_console_i64((int64_t)VM_PROCESS_MAX_USER_REGIONS);
        if (runtime != 0 && runtime->vm != 0) {
            debug_console_puts(" next_mmap=");
            debug_console_i64((int64_t)runtime->vm->next_mmap);
        }
    }
    debug_console_puts(" msg=");
    debug_console_puts(trace != 0 ? trace->message : "");
    debug_console_puts(" path=");
    debug_console_puts(trace != 0 ? trace->path : "");
    debug_console_puts("\n");
}

void linux_compat_debug_syscall_success(
    const linux_compat_runtime_t* runtime,
    const linux_compat_syscall_request_t* request,
    int64_t value,
    const linux_compat_trace_t* trace) {
    static size_t printed = 0;
    const char* path = trace != 0 ? trace->path : "";

    if (printed >= 160U ||
        !debug_success_interesting(request, value, trace)) {
        return;
    }

    printed += 1U;
    debug_console_puts("\nDBG syscall ok nr=");
    debug_console_i64((int64_t)request->number);
    debug_console_puts(" ret=");
    debug_console_i64(value);
    debug_console_puts(" fd=");
    debug_console_i64((int64_t)request->fd);
    debug_console_puts(" len=");
    debug_console_i64((int64_t)request->length);
    debug_console_puts(" off=");
    debug_console_i64((int64_t)request->offset);
    debug_console_puts(" flags=");
    debug_console_i64((int64_t)request->flags);
    if (request->fd >= 3 && request->fd < (int32_t)LINUX_COMPAT_MAX_FDS &&
        runtime != 0 && runtime->fds[request->fd].open) {
        debug_console_puts(" fd_off=");
        debug_console_i64((int64_t)runtime->fds[request->fd].offset);
    }
    debug_console_puts(" msg=");
    debug_console_puts(trace != 0 ? trace->message : "");
    debug_console_puts(" path=");
    debug_console_puts(path);
    debug_console_puts("\n");
}
