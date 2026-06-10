#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/linux_compat.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

void console_putc(char ch) {
    (void)ch;
}

static int dispatch(linux_compat_runtime_t* runtime,
                    linux_compat_syscall_request_t* request,
                    linux_compat_syscall_response_t* response,
                    linux_compat_trace_t* trace) {
    return linux_compat_syscall_dispatch(runtime, request, response, trace);
}

static int test_metadata_syscalls_used_by_help_run(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    linux_compat_utsname_t utsname;
    linux_compat_rlimit_t limit;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MPROTECT;
    request.addr = 0x40000000U;
    request.length = 4096U;
    request.prot = LINUX_COMPAT_PROT_READ;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        !strstr(trace.message, "mprotect")) {
        return fail("expected mprotect help-run no-op to be traced");
    }

    memset(&utsname, 0, sizeof(utsname));
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_UNAME;
    request.read_buffer = &utsname;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        strcmp(utsname.machine, "riscv64") != 0) {
        return fail("expected uname to report riscv64");
    }

    memset(&limit, 0, sizeof(limit));
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_PRLIMIT64;
    request.command = 3U;
    request.read_buffer = &limit;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        limit.current == 0U ||
        limit.maximum < limit.current) {
        return fail("expected prlimit64 to report a stable stack limit");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_SET_TID_ADDRESS;
    request.addr = 0x7000U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value <= 0) {
        return fail("expected set_tid_address to return a deterministic tid");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_SET_ROBUST_LIST;
    request.addr = 0x8000U;
    request.length = 24U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected set_robust_list to be accepted for help-run");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_RT_SIGACTION;
    request.fd = 2;
    request.length = 8U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected rt_sigaction to be accepted for help-run");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_RT_SIGPROCMASK;
    request.fd = 0;
    request.length = 8U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected rt_sigprocmask to be accepted for help-run");
    }

    return 0;
}

static int test_path_vector_and_stat_syscalls_used_by_help_run(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    char link_target[64];
    uint8_t buffer[8];
    linux_compat_iovec_t iov[2];
    linux_compat_statx_t statx;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_FACCESSAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/bin/busybox";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected faccessat to accept readable rootfs paths");
    }

    memset(link_target, 0, sizeof(link_target));
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_READLINKAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/proc/self/exe";
    request.read_buffer = link_target;
    request.length = sizeof(link_target);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value <= 0 ||
        !strstr(link_target, "linux-compat")) {
        return fail("expected readlinkat /proc/self/exe to return a diagnostic target");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/bin/busybox";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 3) {
        return fail("expected openat to allocate fd 3 for pread64");
    }

    memset(buffer, 0, sizeof(buffer));
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_PREAD64;
    request.fd = 3;
    request.read_buffer = buffer;
    request.length = sizeof(buffer);
    request.offset = 0;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != (int64_t)sizeof(buffer) ||
        buffer[0] != 0x7fU ||
        runtime.fds[3].offset != 0U) {
        return fail("expected pread64 to read without advancing fd offset");
    }

    iov[0].base = "usage:";
    iov[0].length = 6U;
    iov[1].base = " git\n";
    iov[1].length = 5U;
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_WRITEV;
    request.fd = 1;
    request.write_buffer = iov;
    request.length = 2U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 11 ||
        !strstr(runtime.stdout_buffer, "usage: git")) {
        return fail("expected writev to append stdout chunks");
    }

    memset(&statx, 0, sizeof(statx));
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_STATX;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/usr/bin/git";
    request.statx = &statx;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        statx.size == 0U ||
        (statx.mode & LINUX_COMPAT_S_IFREG) == 0U) {
        return fail("expected statx to expose rootfs file metadata");
    }

    return 0;
}

int main(void) {
    if (test_metadata_syscalls_used_by_help_run() != 0 ||
        test_path_vector_and_stat_syscalls_used_by_help_run() != 0) {
        return 1;
    }
    return 0;
}
