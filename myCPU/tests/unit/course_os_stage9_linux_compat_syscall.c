/* Linux compat Stage9 syscall 单测：验证 Linux ABI 子集、trace 和错误码转换。 */
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
                    linux_compat_syscall_response_t* response) {
    linux_compat_trace_t trace;

    return linux_compat_syscall_dispatch(runtime, request, response, &trace);
}

static int test_fcntl_flags_dup_and_fail_closed(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_FCNTL;
    request.fd = 1;
    request.command = LINUX_COMPAT_F_GETFD;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected F_GETFD on stdout to return default fd flags");
    }

    request.command = LINUX_COMPAT_F_SETFD;
    request.arg = LINUX_COMPAT_FD_CLOEXEC;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected F_SETFD FD_CLOEXEC to succeed");
    }
    request.command = LINUX_COMPAT_F_GETFD;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != LINUX_COMPAT_FD_CLOEXEC) {
        return fail("expected F_GETFD to return close-on-exec");
    }

    request.command = LINUX_COMPAT_F_SETFL;
    request.arg = LINUX_COMPAT_O_NONBLOCK;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected F_SETFL O_NONBLOCK to succeed");
    }
    request.command = LINUX_COMPAT_F_GETFL;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        (response.value & LINUX_COMPAT_O_NONBLOCK) == 0) {
        return fail("expected F_GETFL to include O_NONBLOCK");
    }

    request.command = LINUX_COMPAT_F_DUPFD;
    request.arg = 3;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != 3 ||
        !runtime.fds[3].open ||
        runtime.fds[3].flags != runtime.fds[1].flags) {
        return fail("expected F_DUPFD to duplicate into first available slot");
    }

    request.command = LINUX_COMPAT_F_SETFD;
    request.arg = 4;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != -22) {
        return fail("expected unsupported fd flags to return EINVAL");
    }

    return 0;
}

static int test_ioctl_tty_contracts(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_winsize_t winsize = {0};
    linux_compat_termios_t termios = {0};

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_IOCTL;
    request.fd = 1;
    request.command = LINUX_COMPAT_TIOCGWINSZ;
    request.arg = (uint64_t)(uintptr_t)&winsize;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        winsize.ws_row != 24 ||
        winsize.ws_col != 80) {
        return fail("expected TIOCGWINSZ on stdout to report 80x24");
    }

    request.command = LINUX_COMPAT_TCGETS;
    request.arg = (uint64_t)(uintptr_t)&termios;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        (termios.c_lflag & 0xaU) != 0xaU) {
        return fail("expected TCGETS on stdout to report echo/canonical mode");
    }

    request.command = LINUX_COMPAT_FIONBIO;
    request.arg = 1;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        (runtime.fds[1].flags & LINUX_COMPAT_O_NONBLOCK) == 0) {
        return fail("expected FIONBIO to set O_NONBLOCK on tty fd");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/bin/busybox";
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != 3) {
        return fail("expected openat before non-tty ioctl to return fd 3");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_IOCTL;
    request.fd = 3;
    request.command = LINUX_COMPAT_TIOCGWINSZ;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != -25) {
        return fail("expected non-tty ioctl to return ENOTTY");
    }

    return 0;
}

static int test_getrandom_and_clock_gettime(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    uint8_t random_bytes[32] = {0};
    linux_compat_timespec_t ts = {0};
    size_t i = 0;
    bool nonzero = false;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_GETRANDOM;
    request.read_buffer = random_bytes;
    request.length = sizeof(random_bytes);
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != (int64_t)sizeof(random_bytes)) {
        return fail("expected getrandom to fill requested bytes");
    }
    for (i = 0; i < sizeof(random_bytes); ++i) {
        nonzero = nonzero || random_bytes[i] != 0U;
    }
    if (!nonzero) {
        return fail("expected deterministic getrandom bytes to be nonzero");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_CLOCK_GETTIME;
    request.fd = LINUX_COMPAT_CLOCK_MONOTONIC;
    request.read_buffer = &ts;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        ts.tv_nsec < 0 ||
        ts.tv_nsec >= 1000000000LL) {
        return fail("expected clock_gettime monotonic to write timespec");
    }

    request.fd = 99;
    if (dispatch(&runtime, &request, &response) != LINUX_COMPAT_OK ||
        response.value != -22) {
        return fail("expected unsupported clock id to return EINVAL");
    }

    return 0;
}

int main(void) {
    if (test_fcntl_flags_dup_and_fail_closed() != 0 ||
        test_ioctl_tty_contracts() != 0 ||
        test_getrandom_and_clock_gettime() != 0) {
        return 1;
    }
    return 0;
}
