#include "course_syscall.h"

#include "course_fd.h"
#include "course_process.h"

/* 课程 syscall 分发层：只接受课程 ABI 的小号表，统一做用户指针检查、
   FD/进程表转发和错误码统计。Linux syscall 不进入这里。 */

/* 把缓冲整批清 0。 */
static void clear_buffer(char* buffer, size_t size) {
    size_t i = 0;

    if (buffer == 0) {
        return;
    }
    for (i = 0; i < size; ++i) {
        buffer[i] = '\0';
    }
}

/* 判断两个 C 字符串是否完全相等。 */
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

/* 记一次 syscall 调用（总数 + 分项）。 */
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

/* 记 syscall 结果：负值记失败与 last_error。 */
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

/* 向 IO 缓冲追加 size 字节并补 NUL，溢出返回 false。 */
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

/* write syscall：fd=1 写 stdout、fd=2 写 stderr，其它拒绝。 */
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

/* 校验用户字符串在沙箱内以 NUL 结尾，可输出长度。 */
static bool user_string_valid(const course_syscall_t* syscalls,
                              uintptr_t user_ptr,
                              size_t* out_len) {
    const char* value = (const char*)user_ptr;
    size_t i = 0;

    if (out_len != 0) {
        *out_len = 0;
    }
    if (syscalls == 0 || value == 0 ||
        !course_syscall_user_range_valid(syscalls, user_ptr, 1U)) {
        return false;
    }
    /* 字符串必须在用户沙箱内遇到 NUL；不能只检查首地址。 */
    while (course_syscall_user_range_valid(syscalls, user_ptr, i + 1U)) {
        if (value[i] == '\0') {
            if (out_len != 0) {
                *out_len = i;
            }
            return true;
        }
        i += 1U;
    }
    return false;
}

/* read syscall：转发到 FD 表。 */
static int64_t dispatch_read(course_syscall_t* syscalls,
                             uint64_t fd,
                             uintptr_t user_ptr,
                             size_t size) {
    if (syscalls == 0 || syscalls->fd_table == 0) {
        return COURSE_SYSCALL_ERR_BAD_FD;
    }
    if (!course_syscall_user_range_valid(syscalls, user_ptr, size)) {
        return COURSE_SYSCALL_ERR_BAD_USER_POINTER;
    }
    return (int64_t)course_fd_read(syscalls->fd_table,
                                   (int)fd,
                                   (char*)user_ptr,
                                   size);
}

/* open syscall：校验路径后转发到 FD 表。 */
static int64_t dispatch_open(course_syscall_t* syscalls,
                             uintptr_t path_ptr,
                             uint32_t flags) {
    if (syscalls == 0 || syscalls->fd_table == 0) {
        return COURSE_SYSCALL_ERR_BAD_FD;
    }
    if (!user_string_valid(syscalls, path_ptr, 0)) {
        return COURSE_SYSCALL_ERR_BAD_USER_POINTER;
    }
    return (int64_t)course_fd_open(syscalls->fd_table,
                                   (const char*)path_ptr,
                                   flags);
}

/* close syscall：转发到 FD 表。 */
static int64_t dispatch_close(course_syscall_t* syscalls, uint64_t fd) {
    if (syscalls == 0 || syscalls->fd_table == 0) {
        return COURSE_SYSCALL_ERR_BAD_FD;
    }
    return (int64_t)course_fd_close(syscalls->fd_table, (int)fd);
}

/* seek syscall：转发到 FD 表。 */
static int64_t dispatch_seek(course_syscall_t* syscalls,
                             uint64_t fd,
                             size_t offset) {
    if (syscalls == 0 || syscalls->fd_table == 0) {
        return COURSE_SYSCALL_ERR_BAD_FD;
    }
    return (int64_t)course_fd_seek(syscalls->fd_table, (int)fd, offset);
}

/* fork syscall：在进程表里 fork 当前进程，返回子 pid。 */
static int64_t dispatch_fork(course_syscall_t* syscalls,
                             uintptr_t child_name_ptr) {
    course_process_t* child = 0;
    const char* child_name = (const char*)child_name_ptr;

    if (syscalls == 0 || syscalls->process_table == 0) {
        return COURSE_SYSCALL_ERR_NO_MEMORY;
    }
    if (child_name_ptr != 0U &&
        !user_string_valid(syscalls, child_name_ptr, 0)) {
        return COURSE_SYSCALL_ERR_BAD_USER_POINTER;
    }
    child = course_process_fork(syscalls->process_table,
                                syscalls->pid,
                                child_name_ptr != 0U ? child_name : "child");
    return child != 0 ? (int64_t)child->pid : COURSE_SYSCALL_ERR_NO_MEMORY;
}

/* exec syscall：用程序名替换当前进程映像，转成 syscall 错误码。 */
static int64_t dispatch_exec(course_syscall_t* syscalls,
                             uintptr_t program_ptr,
                             uintptr_t argv_ptr) {
    const char* argv = "";
    int32_t result = 0;

    if (syscalls == 0 || syscalls->process_table == 0) {
        return COURSE_SYSCALL_ERR_NO_MEMORY;
    }
    if (!user_string_valid(syscalls, program_ptr, 0)) {
        return COURSE_SYSCALL_ERR_BAD_USER_POINTER;
    }
    if (argv_ptr != 0U) {
        if (!user_string_valid(syscalls, argv_ptr, 0)) {
            return COURSE_SYSCALL_ERR_BAD_USER_POINTER;
        }
        argv = (const char*)argv_ptr;
    }
    result = course_process_exec(syscalls->process_table,
                                 syscalls->pid,
                                 (const char*)program_ptr,
                                 argv);
    if (result == COURSE_PROCESS_OK) {
        return COURSE_SYSCALL_OK;
    }
    if (result == COURSE_PROCESS_ERR_NO_SUCH_PROGRAM ||
        result == COURSE_PROCESS_ERR_BAD_ELF) {
        return COURSE_SYSCALL_ERR_NO_SUCH_FILE;
    }
    return COURSE_SYSCALL_ERR_NO_MEMORY;
}

/* waitpid syscall：等待指定子进程并把状态写回用户态。 */
static int64_t dispatch_waitpid(course_syscall_t* syscalls,
                                uint32_t child_pid,
                                uintptr_t status_ptr) {
    int32_t status = 0;
    int32_t result = 0;

    if (syscalls == 0 || syscalls->process_table == 0) {
        return COURSE_SYSCALL_ERR_NO_CHILD;
    }
    if (status_ptr != 0U &&
        !course_syscall_user_range_valid(syscalls,
                                         status_ptr,
                                         sizeof(status))) {
        return COURSE_SYSCALL_ERR_BAD_USER_POINTER;
    }
    result = course_process_waitpid(syscalls->process_table,
                                    syscalls->pid,
                                    child_pid,
                                    &status);
    if (result != COURSE_PROCESS_OK) {
        return COURSE_SYSCALL_ERR_NO_CHILD;
    }
    if (status_ptr != 0U) {
        *(int32_t*)status_ptr = status;
    }
    return COURSE_SYSCALL_OK;
}

/* wait syscall：等待任意子进程并把状态写回用户态。 */
static int64_t dispatch_wait(course_syscall_t* syscalls, uintptr_t status_ptr) {
    int32_t status = 0;
    int32_t result = 0;

    if (syscalls == 0 || syscalls->process_table == 0) {
        return COURSE_SYSCALL_ERR_NO_CHILD;
    }
    if (status_ptr != 0U &&
        !course_syscall_user_range_valid(syscalls,
                                         status_ptr,
                                         sizeof(status))) {
        return COURSE_SYSCALL_ERR_BAD_USER_POINTER;
    }
    result = course_process_wait(syscalls->process_table,
                                 syscalls->pid,
                                 &status);
    if (result != COURSE_PROCESS_OK) {
        return COURSE_SYSCALL_ERR_NO_CHILD;
    }
    if (status_ptr != 0U) {
        *(int32_t*)status_ptr = status;
    }
    return COURSE_SYSCALL_OK;
}

/* exit syscall：标记退出、记退出码并通知进程表。 */
static int64_t dispatch_exit(course_syscall_t* syscalls, int32_t exit_code) {
    if (syscalls == 0) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }
    syscalls->exited = true;
    syscalls->exit_code = exit_code;
    if (syscalls->process_table != 0 &&
        !course_process_exit(syscalls->process_table,
                             syscalls->pid,
                             exit_code)) {
        return COURSE_SYSCALL_ERR_NO_CHILD;
    }
    return COURSE_SYSCALL_OK;
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
    syscalls->fd_table = 0;
    syscalls->process_table = 0;
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

    /* 每个 case 只做类型转换和转发，成功/失败统计统一由 record_result 记录。 */
    switch (number) {
    case COURSE_SYSCALL_READ:
        result = dispatch_read(syscalls, arg0, (uintptr_t)arg1, (size_t)arg2);
        break;
    case COURSE_SYSCALL_WRITE:
        result = dispatch_write(syscalls, arg0, (uintptr_t)arg1, (size_t)arg2);
        break;
    case COURSE_SYSCALL_OPEN:
        result = dispatch_open(syscalls, (uintptr_t)arg0, (uint32_t)arg1);
        break;
    case COURSE_SYSCALL_CLOSE:
        result = dispatch_close(syscalls, arg0);
        break;
    case COURSE_SYSCALL_SEEK:
        result = dispatch_seek(syscalls, arg0, (size_t)arg1);
        break;
    case COURSE_SYSCALL_EXIT:
        result = dispatch_exit(syscalls, (int32_t)arg0);
        break;
    case COURSE_SYSCALL_FORK:
        result = dispatch_fork(syscalls, (uintptr_t)arg0);
        break;
    case COURSE_SYSCALL_EXEC:
        result = dispatch_exec(syscalls, (uintptr_t)arg0, (uintptr_t)arg1);
        break;
    case COURSE_SYSCALL_WAIT:
        result = dispatch_wait(syscalls, (uintptr_t)arg0);
        break;
    case COURSE_SYSCALL_WAITPID:
        result = dispatch_waitpid(syscalls,
                                  (uint32_t)arg0,
                                  (uintptr_t)arg1);
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
    /* 使用减法形式避免 user_ptr + size 溢出后绕回合法区间。 */
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

bool course_syscall_attach_fd_table(course_syscall_t* syscalls,
                                    struct CourseFdTable* fd_table) {
    if (syscalls == 0 || fd_table == 0) {
        return false;
    }
    syscalls->fd_table = fd_table;
    return true;
}

bool course_syscall_attach_process_table(
    course_syscall_t* syscalls,
    struct CourseProcessTable* process_table) {
    if (syscalls == 0 || process_table == 0) {
        return false;
    }
    syscalls->process_table = process_table;
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
