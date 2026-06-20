#include "course_os_stage2.h"

/* Stage2 编排层：验证课程 syscall、进程、FD/FS、shell、COW 和崩溃隔离。
   这里固定一组可重复命令与负向 guardrail，输出稳定 marker。 */

/* 取 C 字符串长度。 */
static size_t str_len(const char* value) {
    size_t i = 0;

    if (value == 0) {
        return 0;
    }
    while (value[i] != '\0') {
        i += 1U;
    }
    return i;
}

/* 判断 haystack 是否包含 needle 子串。 */
static bool str_contains(const char* haystack, const char* needle) {
    size_t i = 0;
    const size_t needle_len = str_len(needle);

    if (haystack == 0 || needle == 0) {
        return false;
    }
    if (needle_len == 0) {
        return true;
    }

    while (haystack[i] != '\0') {
        size_t j = 0;

        while (haystack[i + j] != '\0' &&
               needle[j] != '\0' &&
               haystack[i + j] == needle[j]) {
            j += 1U;
        }
        if (j == needle_len) {
            return true;
        }
        i += 1U;
    }
    return false;
}

/* 向 out 追加一个字符并保持 NUL。 */
static bool append_char(char* out, size_t out_size, size_t* used, char ch) {
    if (out == 0 || used == 0 || *used + 1U >= out_size) {
        return false;
    }
    out[*used] = ch;
    *used += 1U;
    out[*used] = '\0';
    return true;
}

/* 向 out 追加字符串。 */
static bool append_str(char* out,
                       size_t out_size,
                       size_t* used,
                       const char* value) {
    size_t i = 0;

    if (value == 0) {
        return false;
    }
    while (value[i] != '\0') {
        if (!append_char(out, out_size, used, value[i])) {
            return false;
        }
        i += 1U;
    }
    return true;
}

void course_os_stage2_init(course_os_stage2_t* stage) {
    static char user_memory[16] = "x";

    if (stage == 0) {
        return;
    }

    course_shell_init(&stage->shell);
    /* 独立 syscall 对象使用极小用户内存，专门验证坏指针和非法 syscall。 */
    course_syscall_init(&stage->syscalls,
                        stage->shell.shell_pid,
                        (uintptr_t)user_memory,
                        sizeof(user_memory));
    procfs_attach_syscalls(&stage->shell.procfs, &stage->syscalls);
    stage->bad_syscall_guarded = false;
    stage->bad_user_pointer_guarded = false;
    stage->bad_fd_guarded = false;
    stage->proc_write_guarded = false;
    stage->user_crash_guarded = false;
    stage->cow_write_guarded = false;
    stage->pipe_misuse_guarded = false;
}

bool course_os_stage2_run(course_os_stage2_t* stage) {
    char out[1024];
    uint8_t value = 0;
    course_process_t* cow_child = 0;
    int proc_fd = 0;

    if (stage == 0) {
        return false;
    }

    if (!course_shell_run_line(&stage->shell, "echo stage2", out, sizeof(out)) ||
        !str_contains(out, "stage2") ||
        !course_shell_run_line(&stage->shell, "ls", out, sizeof(out)) ||
        !str_contains(out, "tmp") ||
        !course_shell_run_line(&stage->shell, "echo file > /tmp/stage2", out, sizeof(out)) ||
        !course_shell_run_line(&stage->shell, "cat /tmp/stage2", out, sizeof(out)) ||
        !str_contains(out, "file") ||
        !course_shell_run_line(&stage->shell, "ps", out, sizeof(out)) ||
        !str_contains(out, "pid=") ||
        !course_shell_run_line(&stage->shell, "forktest", out, sizeof(out)) ||
        !str_contains(out, "program=forktest") ||
        !course_shell_run_line(&stage->shell, "crash", out, sizeof(out)) ||
        !str_contains(out, "crash=isolated") ||
        !course_shell_run_line(&stage->shell, "cat /proc/crashlog", out, sizeof(out)) ||
        !str_contains(out, "name=crash") ||
        !course_shell_run_line(&stage->shell, "cat /proc/cow", out, sizeof(out)) ||
        !str_contains(out, "cow_faults=") ||
        !course_shell_run_line(&stage->shell, "echo pipe | cat", out, sizeof(out)) ||
        !str_contains(out, "pipe")) {
        return false;
    }

    /* 下面这些 guardrail 固定“错误必须失败”，防止展示路径伪造成功。 */
    stage->bad_syscall_guarded =
        course_syscall_dispatch(&stage->syscalls, 99U, 0U, 0U, 0U, 0U) ==
        COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    stage->bad_user_pointer_guarded =
        course_syscall_dispatch(&stage->syscalls,
                                COURSE_SYSCALL_WRITE,
                                1U,
                                stage->syscalls.user_base + stage->syscalls.user_size,
                                1U,
                                0U) == COURSE_SYSCALL_ERR_BAD_USER_POINTER;
    stage->bad_fd_guarded =
        course_fd_read(&stage->shell.fds, 99, out, sizeof(out)) ==
        COURSE_FD_ERR_BAD_FD;
    proc_fd = course_fd_open(&stage->shell.fds,
                             "/proc/ps",
                             COURSE_FD_OPEN_WRITE);
    stage->proc_write_guarded = proc_fd == COURSE_FD_ERR_PERMISSION_DENIED;
    stage->user_crash_guarded =
        course_shell_run_line(&stage->shell, "echo alive", out, sizeof(out)) &&
        str_contains(out, "alive");

    if (!course_process_map_user_page(&stage->shell.processes,
                                      stage->shell.shell_pid,
                                      0U,
                                      (uint8_t)'A')) {
        return false;
    }
    /* COW 子进程写页后，父进程原字节必须保持不变。 */
    cow_child = course_process_fork(&stage->shell.processes,
                                    stage->shell.shell_pid,
                                    "cowcheck");
    if (cow_child == 0 ||
        !course_process_write_user_byte(&stage->shell.processes,
                                        cow_child->pid,
                                        0U,
                                        0U,
                                        (uint8_t)'B') ||
        !course_process_read_user_byte(&stage->shell.processes,
                                       stage->shell.shell_pid,
                                       0U,
                                       0U,
                                       &value)) {
        return false;
    }
    stage->cow_write_guarded = value == (uint8_t)'A';
    stage->pipe_misuse_guarded =
        !course_shell_run_line(&stage->shell, "echo bad |", out, sizeof(out));

    return stage->bad_syscall_guarded &&
           stage->bad_user_pointer_guarded &&
           stage->bad_fd_guarded &&
           stage->proc_write_guarded &&
           stage->user_crash_guarded &&
           stage->cow_write_guarded &&
           stage->pipe_misuse_guarded;
}

bool course_os_stage2_summary(const course_os_stage2_t* stage,
                              char* out,
                              size_t out_size) {
    size_t used = 0;

    if (stage == 0 || out == 0 || out_size == 0 ||
        !stage->bad_syscall_guarded ||
        !stage->bad_user_pointer_guarded ||
        !stage->bad_fd_guarded ||
        !stage->proc_write_guarded ||
        !stage->user_crash_guarded ||
        !stage->cow_write_guarded ||
        !stage->pipe_misuse_guarded) {
        return false;
    }

    out[0] = '\0';
    return append_str(out, out_size, &used, COURSE_OS_STAGE2_MARKER);
}
