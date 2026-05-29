#include "course_syscall.h"

static void clear_buffer(char* buffer, size_t size) {
    size_t i = 0;

    if (buffer == 0) {
        return;
    }
    for (i = 0; i < size; ++i) {
        buffer[i] = '\0';
    }
}

static bool str_eq(const char* a, const char* b) {
    size_t i = 0;

    if (a == 0 || b == 0) {
        return false;
    }
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return false;
        }
        i += 1U;
    }
    return a[i] == b[i];
}

static bool record_call(course_syscall_t* syscalls, uint32_t number) {
    if (syscalls == 0) {
        return false;
    }

    syscalls->stats.total_calls += 1U;
    if (number < COURSE_SYSCALL_COUNT) {
        syscalls->stats.calls[number] += 1U;
    }
    return true;
}

static int64_t record_result(course_syscall_t* syscalls, int64_t result) {
    if (syscalls == 0) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }

    syscalls->stats.last_error = result < 0 ? (int32_t)result : COURSE_SYSCALL_OK;
    if (result < 0) {
        syscalls->stats.failures += 1U;
    }
    return result;
}

static bool append_to_buffer(char* out,
                             size_t* used,
                             const char* data,
                             size_t size) {
    size_t i = 0;

    if (out == 0 || used == 0 || data == 0 ||
        *used >= COURSE_SYSCALL_IO_BUFFER_SIZE ||
        size >= COURSE_SYSCALL_IO_BUFFER_SIZE - *used) {
        return false;
    }

    for (i = 0; i < size; ++i) {
        out[*used + i] = data[i];
    }
    *used += size;
    out[*used] = '\0';
    return true;
}

static int64_t dispatch_write(course_syscall_t* syscalls,
                              uint64_t fd,
                              uintptr_t user_ptr,
                              size_t size) {
    const char* data = (const char*)user_ptr;

    if (!course_syscall_user_range_valid(syscalls, user_ptr, size)) {
        return COURSE_SYSCALL_ERR_BAD_USER_POINTER;
    }

    if (fd == 1U) {
        return append_to_buffer(syscalls->stdout_buffer,
                                &syscalls->stdout_size,
                                data,
                                size)
                   ? (int64_t)size
                   : COURSE_SYSCALL_ERR_NO_MEMORY;
    }
    if (fd == 2U) {
        return append_to_buffer(syscalls->stderr_buffer,
                                &syscalls->stderr_size,
                                data,
                                size)
                   ? (int64_t)size
                   : COURSE_SYSCALL_ERR_NO_MEMORY;
    }
    return COURSE_SYSCALL_ERR_BAD_FD;
}

void course_syscall_init(course_syscall_t* syscalls,
                         uint32_t pid,
                         uintptr_t user_base,
                         size_t user_size) {
    size_t i = 0;

    if (syscalls == 0) {
        return;
    }

    syscalls->pid = pid;
    syscalls->user_base = user_base;
    syscalls->user_size = user_size;
    syscalls->exited = false;
    syscalls->exit_code = 0;
    syscalls->stdout_size = 0;
    syscalls->stderr_size = 0;
    clear_buffer(syscalls->stdout_buffer, sizeof(syscalls->stdout_buffer));
    clear_buffer(syscalls->stderr_buffer, sizeof(syscalls->stderr_buffer));
    syscalls->stats.total_calls = 0;
    syscalls->stats.failures = 0;
    syscalls->stats.last_error = COURSE_SYSCALL_OK;
    for (i = 0; i < COURSE_SYSCALL_COUNT; ++i) {
        syscalls->stats.calls[i] = 0;
    }
}

int64_t course_syscall_dispatch(course_syscall_t* syscalls,
                                uint32_t number,
                                uint64_t arg0,
                                uint64_t arg1,
                                uint64_t arg2,
                                uint64_t arg3) {
    int64_t result = COURSE_SYSCALL_ERR_INVALID_SYSCALL;

    (void)arg3;
    if (!record_call(syscalls, number)) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }

    switch (number) {
    case COURSE_SYSCALL_WRITE:
        result = dispatch_write(syscalls, arg0, (uintptr_t)arg1, (size_t)arg2);
        break;
    case COURSE_SYSCALL_EXIT:
        syscalls->exited = true;
        syscalls->exit_code = (int32_t)arg0;
        result = COURSE_SYSCALL_OK;
        break;
    case COURSE_SYSCALL_GETPID:
        result = (int64_t)syscalls->pid;
        break;
    default:
        result = COURSE_SYSCALL_ERR_INVALID_SYSCALL;
        break;
    }

    return record_result(syscalls, result);
}

bool course_syscall_user_range_valid(const course_syscall_t* syscalls,
                                     uintptr_t user_ptr,
                                     size_t size) {
    if (syscalls == 0 || user_ptr < syscalls->user_base) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    if (syscalls->user_size == 0 ||
        user_ptr - syscalls->user_base > syscalls->user_size) {
        return false;
    }
    return size <= syscalls->user_size - (user_ptr - syscalls->user_base);
}

bool course_syscall_stats(const course_syscall_t* syscalls,
                          course_syscall_stats_t* out_stats) {
    size_t i = 0;

    if (syscalls == 0 || out_stats == 0) {
        return false;
    }

    out_stats->total_calls = syscalls->stats.total_calls;
    out_stats->failures = syscalls->stats.failures;
    out_stats->last_error = syscalls->stats.last_error;
    for (i = 0; i < COURSE_SYSCALL_COUNT; ++i) {
        out_stats->calls[i] = syscalls->stats.calls[i];
    }
    return true;
}

bool course_syscall_stdout_equals(const course_syscall_t* syscalls,
                                  const char* expected) {
    return syscalls != 0 && str_eq(syscalls->stdout_buffer, expected);
}

bool course_syscall_stderr_equals(const course_syscall_t* syscalls,
                                  const char* expected) {
    return syscalls != 0 && str_eq(syscalls->stderr_buffer, expected);
}

bool course_syscall_exited(const course_syscall_t* syscalls) {
    return syscalls != 0 && syscalls->exited;
}

int32_t course_syscall_exit_code(const course_syscall_t* syscalls) {
    return syscalls != 0 ? syscalls->exit_code : 0;
}

const char* course_syscall_name(uint32_t number) {
    switch (number) {
    case COURSE_SYSCALL_READ:
        return "read";
    case COURSE_SYSCALL_WRITE:
        return "write";
    case COURSE_SYSCALL_OPEN:
        return "open";
    case COURSE_SYSCALL_CLOSE:
        return "close";
    case COURSE_SYSCALL_SEEK:
        return "seek";
    case COURSE_SYSCALL_EXIT:
        return "exit";
    case COURSE_SYSCALL_FORK:
        return "fork";
    case COURSE_SYSCALL_EXEC:
        return "exec";
    case COURSE_SYSCALL_WAIT:
        return "wait";
    case COURSE_SYSCALL_WAITPID:
        return "waitpid";
    case COURSE_SYSCALL_GETPID:
        return "getpid";
    case COURSE_SYSCALL_PS:
        return "ps";
    case COURSE_SYSCALL_KILL:
        return "kill";
    default:
        return "invalid";
    }
}
