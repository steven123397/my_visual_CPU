#include "course_shell.h"

#include "course_libc.h"
#include "course_shell_linux.h"
#include "course_user_programs.h"

/* Course OS 交互 shell：负责命令解析、内建命令、课程用户程序执行、
   procfs 快捷查看，以及显式 linux/PATH fallback 到 Linux compat 旁路。 */

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

/* 向 out 追加无符号 32 位十进制。 */
static bool append_u32(char* out,
                       size_t out_size,
                       size_t* used,
                       uint32_t value) {
    char digits[10];
    size_t count = 0;

    if (value == 0) {
        return append_char(out, out_size, used, '0');
    }
    while (value != 0 && count < sizeof(digits)) {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        count += 1U;
    }
    while (count > 0) {
        count -= 1U;
        if (!append_char(out, out_size, used, digits[count])) {
            return false;
        }
    }
    return true;
}

/* 向 out 追加带符号 32 位十进制。 */
static bool append_i32(char* out,
                       size_t out_size,
                       size_t* used,
                       int32_t value) {
    uint32_t magnitude = 0;

    if (value < 0) {
        if (!append_char(out, out_size, used, '-')) {
            return false;
        }
        magnitude = (uint32_t)(-(value + 1)) + 1U;
    } else {
        magnitude = (uint32_t)value;
    }
    return append_u32(out, out_size, used, magnitude);
}

/* 向 out 追加 64 位十六进制（0x 前缀）。 */
static bool append_hex_u64(char* out,
                           size_t out_size,
                           size_t* used,
                           uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    bool started = false;
    int shift = 60;

    if (!append_str(out, out_size, used, "0x")) {
        return false;
    }
    while (shift >= 0) {
        const uint8_t nibble = (uint8_t)((value >> (uint32_t)shift) & 0xFU);

        if (nibble != 0U || started || shift == 0) {
            if (!append_char(out, out_size, used, digits[nibble])) {
                return false;
            }
            started = true;
        }
        shift -= 4;
    }
    return true;
}

/* 设置命令成功标记（统一入口便于观测）。 */
static void set_command_success(bool* command_success, bool value) {
    if (command_success != 0) {
        *command_success = value;
    }
}

/* 把命令结构清空（argc/pipeline/重定向）。 */
static void clear_command(course_shell_command_t* command) {
    size_t stage = 0;
    size_t arg = 0;

    if (command == 0) {
        return;
    }
    command->pipeline_len = 0;
    command->has_output_redirect = false;
    command->has_input_redirect = false;
    command->output_path[0] = '\0';
    command->input_path[0] = '\0';
    for (stage = 0; stage < COURSE_SHELL_MAX_PIPELINE_STAGES; ++stage) {
        command->pipeline[stage].argc = 0;
        for (arg = 0; arg < COURSE_SHELL_MAX_ARGS; ++arg) {
            command->pipeline[stage].argv[arg][0] = '\0';
        }
    }
}

/* 把一段 token 拷进 argv 槽并补 NUL。 */
static void copy_token(char* out, size_t out_size, const char* start, size_t len) {
    size_t i = 0;

    if (out == 0 || out_size == 0) {
        return;
    }
    while (i < len && i + 1U < out_size) {
        out[i] = start[i];
        i += 1U;
    }
    out[i] = '\0';
}

/* 拷贝并裁剪前后空格后的一段命令文本到 out。 */
static bool copy_trimmed_segment(char* out,
                                 size_t out_size,
                                 const char* start,
                                 size_t len) {
    if (out == 0 || out_size == 0 || start == 0) {
        return false;
    }
    while (len > 0U && *start == ' ') {
        start += 1;
        len -= 1U;
    }
    while (len > 0U && start[len - 1U] == ' ') {
        len -= 1U;
    }
    if (len == 0U || len >= out_size) {
        out[0] = '\0';
        return false;
    }
    copy_token(out, out_size, start, len);
    return true;
}

/* 在 line 里找 && 分隔位置。 */
static bool find_and_separator(const char* line, size_t* offset) {
    size_t i = 0;

    if (line == 0 || offset == 0) {
        return false;
    }
    while (line[i] != '\0' && line[i + 1U] != '\0') {
        if (line[i] == '&' && line[i + 1U] == '&') {
            *offset = i;
            return true;
        }
        i += 1U;
    }
    return false;
}

/* 向当前简单命令追加一个 argv，超容量返回 false。 */
static bool add_arg(course_shell_simple_command_t* command,
                    const char* start,
                    size_t len) {
    if (command == 0 || command->argc >= COURSE_SHELL_MAX_ARGS || len == 0) {
        return false;
    }
    copy_token(command->argv[command->argc],
               sizeof(command->argv[command->argc]),
               start,
               len);
    command->argc += 1U;
    return true;
}

bool course_shell_parse(const char* line, course_shell_command_t* out_command) {
    const char* p = line;
    size_t stage = 0;
    bool expect_command = true;
    course_shell_simple_command_t* target = 0;

    clear_command(out_command);
    if (line == 0 || out_command == 0) {
        return false;
    }

    target = &out_command->pipeline[0];
    /* 解析器只支持空格分词、管道和单条命令级重定向，避免引号/展开等复杂 shell 语义。 */
    while (*p != '\0') {
        const char* start = 0;
        size_t len = 0;

        while (*p == ' ') {
            p += 1;
        }
        if (*p == '\0') {
            break;
        }
        if (*p == '|') {
            if (expect_command || stage + 1U >= COURSE_SHELL_MAX_PIPELINE_STAGES) {
                return false;
            }
            stage += 1U;
            target = &out_command->pipeline[stage];
            expect_command = true;
            p += 1;
            continue;
        }
        if (*p == '>' || *p == '<') {
            const bool output = *p == '>';
            p += 1;
            while (*p == ' ') {
                p += 1;
            }
            start = p;
            while (p[len] != '\0' && p[len] != ' ' && p[len] != '|' &&
                   p[len] != '>' && p[len] != '<') {
                len += 1U;
            }
            if (len == 0) {
                return false;
            }
            if (output) {
                const char* after_path = p + len;

                while (*after_path == ' ') {
                    after_path += 1;
                }
                if (*after_path == '|') {
                    return false;
                }
                out_command->has_output_redirect = true;
                copy_token(out_command->output_path,
                           sizeof(out_command->output_path),
                           start,
                           len);
            } else {
                out_command->has_input_redirect = true;
                copy_token(out_command->input_path,
                           sizeof(out_command->input_path),
                           start,
                           len);
            }
            p += len;
            continue;
        }

        start = p;
        while (p[len] != '\0' && p[len] != ' ' && p[len] != '|' &&
               p[len] != '>' && p[len] != '<') {
            len += 1U;
        }
        if (!add_arg(target, start, len)) {
            return false;
        }
        expect_command = false;
        p += len;
    }

    out_command->pipeline_len = stage + 1U;
    return out_command->pipeline[0].argc > 0 && !expect_command;
}

/* 把一行命令追加进 transcript（带 "$ " 前缀）。 */
static bool transcript_append(course_shell_t* shell, const char* line) {
    if (shell == 0) {
        return false;
    }
    return append_str(shell->transcript,
                      sizeof(shell->transcript),
                      &shell->transcript_size,
                      "$ ") &&
           append_str(shell->transcript,
                      sizeof(shell->transcript),
                      &shell->transcript_size,
                      line) &&
           append_char(shell->transcript,
                       sizeof(shell->transcript),
                       &shell->transcript_size,
                       '\n');
}

/* 从 scratch 栈申请一个命令结构（支持嵌套调用，用完 release）。 */
static course_shell_command_t* acquire_command_scratch(course_shell_t* shell) {
    if (shell == 0 ||
        shell->command_scratch_depth >= COURSE_SHELL_COMMAND_SCRATCH_DEPTH) {
        return 0;
    }
    return &shell->command_scratch[shell->command_scratch_depth++];
}

/* 归还一层 scratch 栈。 */
static void release_command_scratch(course_shell_t* shell) {
    if (shell != 0 && shell->command_scratch_depth > 0U) {
        shell->command_scratch_depth -= 1U;
    }
}

void course_shell_init(course_shell_t* shell) {
    course_process_t* init = 0;

    if (shell == 0) {
        return;
    }

    course_fs_mkfs(&shell->fs);
    /* 初始化一套自包含的课程 OS 状态，浏览器终端启动后无需外部 rootfs。 */
    (void)course_fs_mkdir(&shell->fs, "/home");
    (void)course_fs_mkdir(&shell->fs, "/home/user");
    (void)course_fs_mkdir(&shell->fs, "/tmp");
    course_scheduler_init(&shell->scheduler);
    course_memory_init(&shell->memory, 4U);
    course_process_table_init(&shell->processes);
    init = course_process_spawn(&shell->processes, 0U, "shell");
    shell->shell_pid = init != 0 ? init->pid : 0;
    if (init != 0 &&
        course_process_exec(&shell->processes,
                            shell->shell_pid,
                            "hello",
                            "") == COURSE_PROCESS_OK) {
        copy_token(init->name, sizeof(init->name), "shell", 5U);
        init->argv[0] = '\0';
    }
    procfs_init(&shell->procfs, &shell->scheduler, &shell->memory, &shell->fs);
    procfs_attach_processes(&shell->procfs, &shell->processes);
    course_fd_table_init(&shell->fds, &shell->fs, &shell->procfs);
    procfs_attach_fd_table(&shell->procfs, shell->shell_pid, &shell->fds);
    course_syscall_init(&shell->syscalls, shell->shell_pid, 0U, 0U);
    (void)course_syscall_attach_fd_table(&shell->syscalls, &shell->fds);
    (void)course_syscall_attach_process_table(&shell->syscalls,
                                              &shell->processes);
    (void)procfs_attach_syscalls(&shell->procfs, &shell->syscalls);
    (void)course_fd_set_cwd(&shell->fds, "/");
    course_semaphore_init(&shell->semaphore, &shell->processes, 0);
    course_mutex_init(&shell->mutex, &shell->processes);
    shell->semaphore_initialized = false;
    shell->mutex_initialized = false;
    linux_compat_runtime_init(&shell->linux_compat_runtime);
    shell->linux_trace.path[0] = '\0';
    shell->linux_trace.errno_value = 0;
    shell->linux_trace.syscall_number = 0;
    shell->linux_trace.pc = 0;
    shell->linux_trace.message[0] = '\0';
    shell->command_scratch_depth = 0U;
    shell->transcript[0] = '\0';
    shell->transcript_size = 0;
}

/* 把 fd 内容一次性读进 out（以 NUL 收尾）。 */
static bool read_all_fd(course_fd_table_t* fds,
                        int fd,
                        char* out,
                        size_t out_size) {
    int count = 0;

    if (out == 0 || out_size == 0) {
        return false;
    }
    out[0] = '\0';
    count = course_fd_read(fds, fd, out, out_size - 1U);
    if (count < 0) {
        return false;
    }
    out[count] = '\0';
    return true;
}

/* 判断脚本行是否应忽略（空行、注释、首行 shebang）。 */
static bool line_ignored(const char* line, bool first_line) {
    const char* p = line;

    if (p == 0) {
        return true;
    }
    while (*p == ' ') {
        p += 1;
    }
    if (*p == '\0') {
        return true;
    }
    if (first_line && p[0] == '#' && p[1] == '!') {
        return true;
    }
    return *p == '#';
}

/* 执行一段脚本文件：按行 run_line，跳过注释/shebang，失败回报行号。 */
static bool run_script(course_shell_t* shell,
                       const char* path,
                       char* out,
                       size_t out_size) {
    char script[1024];
    char line[160];
    char line_out[512];
    size_t used = 0;
    size_t pos = 0;
    uint32_t line_no = 1U;
    int fd = 0;
    bool ok = false;

    if (shell == 0 || path == 0 || out == 0 || out_size == 0) {
        return false;
    }
    out[0] = '\0';
    fd = course_fd_open(&shell->fds, path, COURSE_FD_OPEN_READ);
    if (fd < 0) {
        return false;
    }
    ok = read_all_fd(&shell->fds, fd, script, sizeof(script));
    (void)course_fd_close(&shell->fds, fd);
    if (!ok) {
        return false;
    }

    /* shell script 按行执行，支持 shebang/注释跳过；失败时回报出错行号。 */
    while (script[pos] != '\0') {
        size_t len = 0;
        size_t i = 0;

        while (script[pos + len] != '\0' && script[pos + len] != '\n') {
            len += 1U;
        }
        while (len > 0U && script[pos + len - 1U] == '\r') {
            len -= 1U;
        }
        if (len >= sizeof(line)) {
            len = sizeof(line) - 1U;
        }
        for (i = 0; i < len; ++i) {
            line[i] = script[pos + i];
        }
        line[len] = '\0';

        if (!line_ignored(line, line_no == 1U)) {
            if (!course_shell_run_line(shell, line, line_out, sizeof(line_out))) {
                (void)append_str(out, out_size, &used, line_out);
                (void)append_str(out, out_size, &used, "line=");
                (void)append_u32(out, out_size, &used, line_no);
                (void)append_str(out, out_size, &used, " command=");
                (void)append_str(out, out_size, &used, line);
                (void)append_char(out, out_size, &used, '\n');
                return false;
            }
            if (!append_str(out, out_size, &used, line_out)) {
                return false;
            }
        }

        if (script[pos + len] == '\0') {
            break;
        }
        pos += len + 1U;
        line_no += 1U;
    }

    return true;
}

/* 把内核侧 C 字符串拷进用户态缓冲（模拟用户指针写入）。 */
static bool copy_cstr_to_user(char* out, size_t out_size, const char* value) {
    if (out == 0 || out_size == 0 || value == 0) {
        return false;
    }
    copy_token(out, out_size, value, str_len(value));
    return true;
}

/* 用 libc syscall effect 模拟课程用户程序（hello/echo/cat）的固定输出。 */
static bool run_program_libc_effect(course_shell_t* shell,
                                    const course_shell_simple_command_t* command,
                                    const course_user_program_t* program,
                                    uint32_t pid,
                                    char* stdout_buffer,
                                    size_t stdout_size) {
    char user_memory[512];
    char* path = &user_memory[0];
    char* data = &user_memory[128];
    char* read_buffer = &user_memory[256];
    course_syscall_t syscalls;
    course_libc_t libc;
    size_t i = 0;

    if (shell == 0 || command == 0 || program == 0 ||
        stdout_buffer == 0 || stdout_size == 0) {
        return false;
    }
    for (i = 0; i < sizeof(user_memory); ++i) {
        user_memory[i] = '\0';
    }
    stdout_buffer[0] = '\0';
    course_syscall_init(&syscalls,
                        pid,
                        (uintptr_t)user_memory,
                        sizeof(user_memory));
    if (!course_syscall_attach_fd_table(&syscalls, &shell->fds) ||
        !course_syscall_attach_process_table(&syscalls, &shell->processes)) {
        return false;
    }
    course_libc_init(&libc, &syscalls);

    if (program->kind == COURSE_USER_PROGRAM_HELLO) {
        if (!copy_cstr_to_user(data, 128U, "hello from libc\n") ||
            course_libc_write(&libc, 1, data, str_len(data)) < 0) {
            return false;
        }
    } else if (program->kind == COURSE_USER_PROGRAM_ECHO) {
        if (!copy_cstr_to_user(data,
                               128U,
                               command->argc > 1U ? command->argv[1] : "") ||
            course_libc_write(&libc, 1, data, str_len(data)) < 0) {
            return false;
        }
    } else if (program->kind == COURSE_USER_PROGRAM_CAT) {
        int fd = 0;
        int64_t read_size = 0;

        if (command->argc < 2U ||
            !copy_cstr_to_user(path, 128U, command->argv[1])) {
            return false;
        }
        fd = (int)course_libc_open(&libc, path, COURSE_FD_OPEN_READ);
        if (fd < 0) {
            return false;
        }
        read_size = course_libc_read(&libc, fd, read_buffer, 127U);
        if (read_size < 0 ||
            course_libc_write(&libc,
                              1,
                              read_buffer,
                              (size_t)read_size) != read_size ||
            course_libc_close(&libc, fd) != 0) {
            return false;
        }
    }

    copy_token(stdout_buffer,
               stdout_size,
               syscalls.stdout_buffer,
               str_len(syscalls.stdout_buffer));
    return true;
}

/* 程序执行失败时清理子进程：未退出则先 exit，再 waitpid 收尸。 */
static void cleanup_failed_program_child(course_shell_t* shell,
                                         course_process_t* child) {
    int32_t ignored_status = 0;

    if (shell == 0 || child == 0 || child->state == COURSE_PROCESS_DEAD ||
        child->state == COURSE_PROCESS_UNUSED) {
        return;
    }
    if (child->state != COURSE_PROCESS_ZOMBIE) {
        (void)course_process_exit(&shell->processes, child->pid, 127);
    }
    (void)course_process_waitpid(&shell->processes,
                                 shell->shell_pid,
                                 child->pid,
                                 &ignored_status);
}

/* 执行用户程序命令：fork 子进程、装载 ELF、跑 libc effect、wait 收尸并输出。 */
static bool run_program_command(course_shell_t* shell,
                                const course_shell_simple_command_t* command,
                                char* out,
                                size_t out_size) {
    size_t used = 0;
    course_user_program_t program;
    course_process_t* child = 0;
    int32_t status = 0;
    char program_stdout[COURSE_SYSCALL_IO_BUFFER_SIZE];
    size_t external_elf_size = 0;
    bool external_program = false;
    bool program_found = false;

    if (shell == 0 || command == 0 || command->argc == 0 || out == 0 ||
        out_size == 0) {
        return false;
    }

    out[0] = '\0';
    program_found = course_user_program_lookup(command->argv[0], &program);
    if (!program_found && command->argv[0][0] != '/') {
        return false;
    }
    /* 每次程序运行都 fork 一个课程子进程，便于 wait/crash/COW 证据落在进程表里。 */
    child = course_process_fork(&shell->processes,
                                shell->shell_pid,
                                command->argv[0]);
    if (child == 0) {
        return false;
    }
    if (program_found) {
        if (course_process_exec(&shell->processes,
                                child->pid,
                                program.name,
                                command->argc > 1U ? command->argv[1] : "") !=
            COURSE_PROCESS_OK) {
            cleanup_failed_program_child(shell, child);
            return false;
        }
    } else {
        int fd = 0;
        int read_size = 0;

        /* 绝对路径程序从课程 FS 读取 ELF 镜像，复用 course_elf_loader 的保守子集。 */
        if (!course_fs_size(&shell->fs,
                            command->argv[0],
                            &external_elf_size) ||
            external_elf_size == 0U ||
            external_elf_size > sizeof(shell->external_elf_scratch)) {
            cleanup_failed_program_child(shell, child);
            return false;
        }
        fd = course_fd_open(&shell->fds, command->argv[0], COURSE_FD_OPEN_READ);
        if (fd < 0) {
            cleanup_failed_program_child(shell, child);
            return false;
        }
        read_size = course_fd_read(&shell->fds,
                                   fd,
                                   (char*)(void*)shell->external_elf_scratch,
                                   sizeof(shell->external_elf_scratch));
        (void)course_fd_close(&shell->fds, fd);
        if (read_size <= 0 || (size_t)read_size != external_elf_size) {
            cleanup_failed_program_child(shell, child);
            return false;
        }
        if (course_process_exec_image(&shell->processes,
                                      child->pid,
                                      command->argv[0],
                                      shell->external_elf_scratch,
                                      external_elf_size,
                                      command->argc > 1U ? command->argv[1] : "") !=
            COURSE_PROCESS_OK) {
            cleanup_failed_program_child(shell, child);
            return false;
        }
        external_program = true;
    }
    if (external_program) {
        if (!course_process_exit(&shell->processes, child->pid, 0) ||
            course_process_waitpid(&shell->processes,
                                   shell->shell_pid,
                                   child->pid,
                                   &status) != COURSE_PROCESS_OK) {
            return false;
        }
        return append_str(out, out_size, &used, "program=") &&
               append_str(out, out_size, &used, command->argv[0]) &&
               append_str(out, out_size, &used, " entry=") &&
               append_hex_u64(out, out_size, &used, (uint64_t)child->entry_pc) &&
               append_str(out, out_size, &used, " exit=") &&
               append_u32(out, out_size, &used, (uint32_t)status) &&
               append_char(out, out_size, &used, '\n');
    }
    if (!run_program_libc_effect(shell,
                                 command,
                                 &program,
                                 child->pid,
                                 program_stdout,
                                 sizeof(program_stdout))) {
        return false;
    }
    if (program.kind == COURSE_USER_PROGRAM_FORKTEST) {
        if (!child->user_pages[0].mapped &&
            !course_process_map_user_page(&shell->processes,
                                          child->pid,
                                          0U,
                                          (uint8_t)'F')) {
            return false;
        }
    }
    if (program.kind == COURSE_USER_PROGRAM_CRASH) {
        if (!course_process_record_crash(&shell->processes,
                                         child->pid,
                                         program.entry_pc,
                                         13U,
                                         0xDEADU,
                                         "user-crash")) {
            return false;
        }
    } else if (!course_process_exit(&shell->processes, child->pid, 0)) {
        return false;
    }
    if (course_process_waitpid(&shell->processes,
                               shell->shell_pid,
                               child->pid,
                               &status) != COURSE_PROCESS_OK) {
        return false;
    }
    if (program.kind == COURSE_USER_PROGRAM_CRASH) {
        return append_str(out, out_size, &used, "program=") &&
               append_str(out, out_size, &used, program.name) &&
               append_str(out, out_size, &used, " exit=-128 crash=isolated\n");
    }
    if (program.kind == COURSE_USER_PROGRAM_FORKTEST) {
        course_process_cow_stats_t stats;

        if (!course_process_cow_stats(&shell->processes, &stats)) {
            return false;
        }
        return append_str(out, out_size, &used, "program=") &&
               append_str(out, out_size, &used, program.name) &&
               append_str(out, out_size, &used, " exit=") &&
               append_u32(out, out_size, &used, (uint32_t)status) &&
               (program_stdout[0] == '\0' ||
                (append_str(out, out_size, &used, " stdout=") &&
                 append_str(out, out_size, &used, program_stdout))) &&
               append_str(out, out_size, &used, " cow_shared=") &&
               append_u32(out, out_size, &used, stats.shared_pages) &&
               append_char(out, out_size, &used, '\n');
    }
    return append_str(out, out_size, &used, "program=") &&
           append_str(out, out_size, &used, program.name) &&
           append_str(out, out_size, &used, " exit=") &&
           append_u32(out, out_size, &used, (uint32_t)status) &&
           (program_stdout[0] == '\0' ||
            (append_str(out, out_size, &used, " stdout=") &&
             append_str(out, out_size, &used, program_stdout))) &&
           append_char(out, out_size, &used, '\n');
}

/* 打开并读取一个 /proc 节点到 out。 */
static bool read_proc_file(course_shell_t* shell,
                           const char* path,
                           char* out,
                           size_t out_size) {
    int fd = 0;
    bool ok = false;

    if (shell == 0 || path == 0) {
        return false;
    }
    fd = course_fd_open(&shell->fds, path, COURSE_FD_OPEN_READ);
    if (fd < 0) {
        return false;
    }
    ok = read_all_fd(&shell->fds, fd, out, out_size);
    (void)course_fd_close(&shell->fds, fd);
    return ok;
}

/* 拼接 /proc/<pid>/<suffix> 路径。 */
static bool append_pid_path(char* path,
                            size_t path_size,
                            const char* suffix,
                            uint32_t pid) {
    size_t used = 0;

    if (path == 0 || suffix == 0 || path_size == 0) {
        return false;
    }
    path[0] = '\0';
    return append_str(path, path_size, &used, "/proc/") &&
           append_u32(path, path_size, &used, pid) &&
           append_char(path, path_size, &used, '/') &&
           append_str(path, path_size, &used, suffix);
}

/* 把命令参数解析成 pid（纯数字）。 */
static bool parse_pid_arg(const char* value, uint32_t* out_pid) {
    uint32_t pid = 0;
    size_t i = 0;

    if (value == 0 || value[0] == '\0' || out_pid == 0) {
        return false;
    }
    while (value[i] != '\0') {
        if (value[i] < '0' || value[i] > '9') {
            return false;
        }
        pid = pid * 10U + (uint32_t)(value[i] - '0');
        i += 1U;
    }
    *out_pid = pid;
    return true;
}

/* 读取 /proc/<pid>/<suffix>，pid 缺省取 shell 自身 pid。 */
static bool read_pid_proc_file(course_shell_t* shell,
                               const course_shell_simple_command_t* command,
                               const char* suffix,
                               char* out,
                               size_t out_size) {
    char path[COURSE_FD_MAX_PATH];
    uint32_t pid = 0;

    if (shell == 0 || command == 0) {
        return false;
    }
    pid = shell->shell_pid;
    if (command->argc > 1U && !parse_pid_arg(command->argv[1], &pid)) {
        return false;
    }
    return append_pid_path(path, sizeof(path), suffix, pid) &&
           read_proc_file(shell, path, out, out_size);
}

/* cd 命令：先试课程 FS，失败再走 linux_compat chdir，输出新 cwd。 */
static bool run_cd_command(course_shell_t* shell,
                           const course_shell_simple_command_t* command,
                           char* out,
                           size_t out_size,
                           size_t* used) {
    const char* target = command->argc > 1U ? command->argv[1] : "/";
    const char* linux_cwd = 0;

    if (course_fd_set_cwd(&shell->fds, target) == COURSE_FD_OK) {
        (void)linux_compat_runtime_set_cwd(&shell->linux_compat_runtime,
                                           course_fd_cwd(&shell->fds));
        return append_str(out, out_size, used, course_fd_cwd(&shell->fds)) &&
               append_char(out, out_size, used, '\n');
    }

    if (!linux_compat_runtime_set_cwd(&shell->linux_compat_runtime,
                                      course_fd_cwd(&shell->fds))) {
        return false;
    }
    if (!linux_compat_runtime_chdir(&shell->linux_compat_runtime,
                                    target,
                                    &shell->linux_trace)) {
        return false;
    }

    linux_cwd = linux_compat_runtime_cwd(&shell->linux_compat_runtime);
    if (str_len(linux_cwd) >= sizeof(shell->fds.cwd)) {
        return false;
    }
    copy_token(shell->fds.cwd,
               sizeof(shell->fds.cwd),
               linux_cwd,
               str_len(linux_cwd));
    return append_str(out, out_size, used, shell->fds.cwd) &&
           append_char(out, out_size, used, '\n');
}

/* 把同步结果枚举转成展示字符串。 */
static const char* sync_result_name(course_sync_result_t result) {
    switch (result) {
    case COURSE_SYNC_OK:
        return "ok";
    case COURSE_SYNC_BLOCKED:
        return "blocked";
    case COURSE_SYNC_ERR_BAD_PROCESS:
        return "bad-process";
    case COURSE_SYNC_ERR_NOT_OWNER:
        return "not-owner";
    default:
        return "error";
    }
}

/* 输出信号量当前状态（value/waiters）。 */
static bool append_semaphore_state(char* out,
                                   size_t out_size,
                                   size_t* used,
                                   const course_semaphore_t* semaphore) {
    if (semaphore == 0) {
        return false;
    }
    return append_str(out, out_size, used, "sem value=") &&
           append_i32(out, out_size, used, semaphore->value) &&
           append_str(out, out_size, used, " waiters=") &&
           append_u32(out, out_size, used, semaphore->waiter_count);
}

/* 输出互斥锁当前状态（owner/waiters/misuse）。 */
static bool append_mutex_state(char* out,
                               size_t out_size,
                               size_t* used,
                               const course_mutex_t* mutex) {
    if (mutex == 0) {
        return false;
    }
    return append_str(out, out_size, used, "mutex owner=") &&
           append_u32(out, out_size, used, mutex->owner_pid) &&
           append_str(out, out_size, used, " waiters=") &&
           append_u32(out, out_size, used, mutex->waiter_count) &&
           append_str(out, out_size, used, " misuse=") &&
           append_u32(out, out_size, used, mutex->misuse_guard_count);
}

/* 输出同步操作结果字符串。 */
static bool append_sync_result(char* out,
                               size_t out_size,
                               size_t* used,
                               course_sync_result_t result) {
    return append_str(out, out_size, used, " result=") &&
           append_str(out, out_size, used, sync_result_name(result));
}

/* 输出命令错误信息并置命令成功标记为 false。 */
static bool append_command_error(char* out,
                                 size_t out_size,
                                 size_t* used,
                                 bool* command_success,
                                 const char* message) {
    set_command_success(command_success, false);
    return append_str(out, out_size, used, message);
}

/* sem 命令：signal/wait 信号量并输出状态与结果。 */
static bool run_sem_command(course_shell_t* shell,
                            const course_shell_simple_command_t* command,
                            char* out,
                            size_t out_size,
                            size_t* used,
                            bool* command_success) {
    course_sync_result_t result = COURSE_SYNC_OK;
    uint32_t value = 0;
    uint32_t pid = 0;

    if (command->argc < 2U) {
        return append_command_error(out,
                                    out_size,
                                    used,
                                    command_success,
                                    "sem: usage: sem init <value>|wait [pid]|post|status\n");
    }

    if (str_eq(command->argv[1], "init")) {
        if (command->argc < 3U || !parse_pid_arg(command->argv[2], &value)) {
            return append_command_error(out,
                                        out_size,
                                        used,
                                        command_success,
                                        "sem: usage: sem init <value>\n");
        }
        course_semaphore_init(&shell->semaphore,
                              &shell->processes,
                              (int32_t)value);
        shell->semaphore_initialized = true;
        return append_semaphore_state(out, out_size, used, &shell->semaphore) &&
               append_char(out, out_size, used, '\n');
    }

    if (!shell->semaphore_initialized) {
        return append_command_error(out,
                                    out_size,
                                    used,
                                    command_success,
                                    "sem: not initialized\n");
    }

    if (str_eq(command->argv[1], "status")) {
        return append_semaphore_state(out, out_size, used, &shell->semaphore) &&
               append_char(out, out_size, used, '\n');
    }
    if (str_eq(command->argv[1], "wait")) {
        pid = shell->shell_pid;
        if (command->argc > 2U && !parse_pid_arg(command->argv[2], &pid)) {
            return append_command_error(out,
                                        out_size,
                                        used,
                                        command_success,
                                        "sem: invalid pid\n");
        }
        result = course_semaphore_wait(&shell->semaphore, pid);
        set_command_success(command_success,
                            result != COURSE_SYNC_ERR_BAD_PROCESS);
        return append_semaphore_state(out, out_size, used, &shell->semaphore) &&
               append_sync_result(out, out_size, used, result) &&
               append_char(out, out_size, used, '\n');
    }
    if (str_eq(command->argv[1], "post")) {
        result = course_semaphore_post(&shell->semaphore);
        set_command_success(command_success,
                            result != COURSE_SYNC_ERR_BAD_PROCESS);
        return append_semaphore_state(out, out_size, used, &shell->semaphore) &&
               append_sync_result(out, out_size, used, result) &&
               append_char(out, out_size, used, '\n');
    }

    return append_command_error(out,
                                out_size,
                                used,
                                command_success,
                                "sem: usage: sem init <value>|wait [pid]|post|status\n");
}

/* mutex 命令：lock/unlock 互斥锁并输出状态与结果。 */
static bool run_mutex_command(course_shell_t* shell,
                              const course_shell_simple_command_t* command,
                              char* out,
                              size_t out_size,
                              size_t* used,
                              bool* command_success) {
    course_sync_result_t result = COURSE_SYNC_OK;
    uint32_t pid = shell != 0 ? shell->shell_pid : 0U;

    if (command->argc < 2U) {
        return append_command_error(out,
                                    out_size,
                                    used,
                                    command_success,
                                    "mutex: usage: mutex init|lock [pid]|unlock [pid]|status\n");
    }

    if (str_eq(command->argv[1], "init")) {
        course_mutex_init(&shell->mutex, &shell->processes);
        shell->mutex_initialized = true;
        return append_mutex_state(out, out_size, used, &shell->mutex) &&
               append_char(out, out_size, used, '\n');
    }

    if (!shell->mutex_initialized) {
        return append_command_error(out,
                                    out_size,
                                    used,
                                    command_success,
                                    "mutex: not initialized\n");
    }

    if (str_eq(command->argv[1], "status")) {
        return append_mutex_state(out, out_size, used, &shell->mutex) &&
               append_char(out, out_size, used, '\n');
    }
    if (str_eq(command->argv[1], "lock") ||
        str_eq(command->argv[1], "unlock")) {
        if (command->argc > 2U && !parse_pid_arg(command->argv[2], &pid)) {
            return append_command_error(out,
                                        out_size,
                                        used,
                                        command_success,
                                        "mutex: invalid pid\n");
        }
        result = str_eq(command->argv[1], "lock")
                     ? course_mutex_lock(&shell->mutex, pid)
                     : course_mutex_unlock(&shell->mutex, pid);
        set_command_success(command_success,
                            result == COURSE_SYNC_OK ||
                                result == COURSE_SYNC_BLOCKED);
        return append_mutex_state(out, out_size, used, &shell->mutex) &&
               append_sync_result(out, out_size, used, result) &&
               append_char(out, out_size, used, '\n');
    }

    return append_command_error(out,
                                out_size,
                                used,
                                command_success,
                                "mutex: usage: mutex init|lock [pid]|unlock [pid]|status\n");
}

/* concurrency_demo 命令：用信号量编排多进程同步并输出最终状态。 */
static bool run_concurrency_demo(course_shell_t* shell,
                                 char* out,
                                 size_t out_size,
                                 size_t* used,
                                 bool* command_success) {
    course_process_t* worker_a = 0;
    course_process_t* worker_b = 0;
    course_process_t* sem_owner = 0;
    course_semaphore_t semaphore;
    course_mutex_t mutex;
    int32_t status = 0;
    bool sem_blocked_ok = false;
    bool sem_posted_ok = false;
    bool mutex_lock_ok = false;
    bool mutex_blocked_ok = false;
    bool mutex_misuse_ok = false;
    bool mutex_release_ok = false;

    if (shell == 0) {
        return false;
    }

    worker_a = course_process_fork(&shell->processes,
                                   shell->shell_pid,
                                   "worker-a");
    worker_b = course_process_fork(&shell->processes,
                                   shell->shell_pid,
                                   "worker-b");
    sem_owner = course_process_fork(&shell->processes,
                                    shell->shell_pid,
                                    "sem-owner");
    if (worker_a == 0 || worker_b == 0 || sem_owner == 0) {
        if (worker_a != 0) {
            (void)course_process_exit(&shell->processes, worker_a->pid, 1);
            (void)course_process_waitpid(&shell->processes,
                                         shell->shell_pid,
                                         worker_a->pid,
                                         &status);
        }
        if (worker_b != 0) {
            (void)course_process_exit(&shell->processes, worker_b->pid, 1);
            (void)course_process_waitpid(&shell->processes,
                                         shell->shell_pid,
                                         worker_b->pid,
                                         &status);
        }
        if (sem_owner != 0) {
            (void)course_process_exit(&shell->processes, sem_owner->pid, 1);
            (void)course_process_waitpid(&shell->processes,
                                         shell->shell_pid,
                                         sem_owner->pid,
                                         &status);
        }
        return append_command_error(out,
                                    out_size,
                                    used,
                                    command_success,
                                    "concurrency_demo: process setup failed\n");
    }

    course_semaphore_init(&semaphore, &shell->processes, 0);
    course_mutex_init(&mutex, &shell->processes);
    sem_blocked_ok = course_semaphore_wait(&semaphore, worker_b->pid) ==
                         COURSE_SYNC_BLOCKED &&
                     worker_b->state == COURSE_PROCESS_BLOCKED &&
                     semaphore.waiter_count == 1U;
    sem_posted_ok = course_semaphore_post(&semaphore) == COURSE_SYNC_OK &&
                    worker_b->state == COURSE_PROCESS_READY &&
                    semaphore.waiter_count == 0U;
    mutex_lock_ok = course_mutex_lock(&mutex, worker_a->pid) == COURSE_SYNC_OK &&
                    mutex.owner_pid == worker_a->pid;
    mutex_blocked_ok = course_mutex_lock(&mutex, worker_b->pid) ==
                           COURSE_SYNC_BLOCKED &&
                       worker_b->state == COURSE_PROCESS_BLOCKED &&
                       mutex.waiter_count == 1U;
    mutex_misuse_ok = course_mutex_unlock(&mutex, worker_b->pid) ==
                          COURSE_SYNC_ERR_NOT_OWNER &&
                      mutex.misuse_guard_count == 1U &&
                      mutex.owner_pid == worker_a->pid;
    mutex_release_ok = course_mutex_unlock(&mutex, worker_a->pid) ==
                           COURSE_SYNC_OK &&
                       worker_b->state == COURSE_PROCESS_READY &&
                       mutex.owner_pid == worker_b->pid;

    set_command_success(command_success,
                        sem_blocked_ok && sem_posted_ok && mutex_lock_ok &&
                            mutex_blocked_ok && mutex_misuse_ok &&
                            mutex_release_ok);
    if (!course_process_exit(&shell->processes, worker_a->pid, 0) ||
        !course_process_exit(&shell->processes, worker_b->pid, 0) ||
        !course_process_exit(&shell->processes, sem_owner->pid, 0) ||
        course_process_waitpid(&shell->processes,
                               shell->shell_pid,
                               worker_a->pid,
                               &status) != COURSE_PROCESS_OK ||
        course_process_waitpid(&shell->processes,
                               shell->shell_pid,
                               worker_b->pid,
                               &status) != COURSE_PROCESS_OK ||
        course_process_waitpid(&shell->processes,
                               shell->shell_pid,
                               sem_owner->pid,
                               &status) != COURSE_PROCESS_OK) {
        set_command_success(command_success, false);
        return append_str(out,
                          out_size,
                          used,
                          "concurrency_demo: teardown failed\n");
    }

    return append_str(out, out_size, used, "concurrency_demo worker-a=") &&
           append_u32(out, out_size, used, worker_a->pid) &&
           append_str(out, out_size, used, " worker-b=") &&
           append_u32(out, out_size, used, worker_b->pid) &&
           append_str(out, out_size, used, " sem-owner=") &&
           append_u32(out, out_size, used, sem_owner->pid) &&
           append_str(out, out_size, used, " sem-blocked=") &&
           append_str(out, out_size, used, sem_blocked_ok ? "ok" : "error") &&
           append_str(out, out_size, used, " sem-posted=") &&
           append_str(out, out_size, used, sem_posted_ok ? "ok" : "error") &&
           append_str(out, out_size, used, " mutex-lock=") &&
           append_str(out, out_size, used, mutex_lock_ok ? "ok" : "error") &&
           append_str(out, out_size, used, " mutex-blocked=") &&
           append_str(out, out_size, used, mutex_blocked_ok ? "ok" : "error") &&
           append_str(out, out_size, used, " mutex-misuse=") &&
           append_str(out, out_size, used, mutex_misuse_ok ? "ok" : "error") &&
           append_str(out, out_size, used, " mutex-release=") &&
           append_str(out, out_size, used, mutex_release_ok ? "ok" : "error") &&
           append_char(out, out_size, used, '\n');
}

/* 执行单个简单命令：内建命令、procfs 快捷查看、用户程序、linux 旁路分发。 */
static bool run_simple(course_shell_t* shell,
                       const course_shell_simple_command_t* command,
                       const char* stdin_text,
                       char* out,
                       size_t out_size,
                       bool* command_success) {
    size_t used = 0;
    course_user_program_t program;

    if (shell == 0 || command == 0 || command->argc == 0 || out == 0 ||
        out_size == 0) {
        return false;
    }
    out[0] = '\0';
    set_command_success(command_success, true);

    if (str_eq(command->argv[0], "help")) {
        return append_str(out,
                          out_size,
                          &used,
                          "help ls cat echo ps kill cd pwd exit exec sh "
                          "meminfo schedstat fsstat syscalls cow crashlog "
                          "cpuinfo uptime status fd maps sem mutex mkfs "
                          "concurrency_demo\n");
    }
    if (str_eq(command->argv[0], "pwd")) {
        return append_str(out, out_size, &used, course_fd_cwd(&shell->fds)) &&
               append_char(out, out_size, &used, '\n');
    }
    if (str_eq(command->argv[0], "cd")) {
        return run_cd_command(shell, command, out, out_size, &used);
    }
    if (str_eq(command->argv[0], "echo")) {
        size_t i = 1U;

        for (i = 1U; i < command->argc; ++i) {
            if (i > 1U && !append_char(out, out_size, &used, ' ')) {
                return false;
            }
            if (!append_str(out, out_size, &used, command->argv[i])) {
                return false;
            }
        }
        return append_char(out, out_size, &used, '\n');
    }
    if (str_eq(command->argv[0], "cat")) {
        if (command->argc > 1U) {
            int fd = course_fd_open(&shell->fds, command->argv[1], COURSE_FD_OPEN_READ);
            bool ok = false;

            if (fd < 0) {
                return false;
            }
            ok = read_all_fd(&shell->fds, fd, out, out_size);
            (void)course_fd_close(&shell->fds, fd);
            return ok;
        }
        if (stdin_text == 0) {
            stdin_text = "";
        }
        return append_str(out, out_size, &used, stdin_text);
    }
    if (str_eq(command->argv[0], "sh")) {
        if (command->argc < 2U) {
            return false;
        }
        return run_script(shell, command->argv[1], out, out_size);
    }
    if (str_eq(command->argv[0], "linux")) {
        return course_shell_run_linux_command(shell,
                                              command,
                                              stdin_text,
                                              out,
                                              out_size,
                                              command_success);
    }
    if (str_eq(command->argv[0], "ps")) {
        return read_proc_file(shell, "/proc/ps", out, out_size);
    }
    if (str_eq(command->argv[0], "meminfo")) {
        return read_proc_file(shell, "/proc/meminfo", out, out_size);
    }
    if (str_eq(command->argv[0], "schedstat")) {
        return read_proc_file(shell, "/proc/schedstat", out, out_size);
    }
    if (str_eq(command->argv[0], "fsstat")) {
        return read_proc_file(shell, "/proc/fsstat", out, out_size);
    }
    if (str_eq(command->argv[0], "mkfs")) {
        course_fs_mkfs(&shell->fs);
        course_fd_table_init(&shell->fds, &shell->fs, &shell->procfs);
        procfs_attach_fd_table(&shell->procfs, shell->shell_pid, &shell->fds);
        (void)course_syscall_attach_fd_table(&shell->syscalls, &shell->fds);
        (void)linux_compat_runtime_set_cwd(&shell->linux_compat_runtime, "/");
        return append_str(out,
                          out_size,
                          &used,
                          "mkfs: filesystem initialized\n");
    }
    if (str_eq(command->argv[0], "syscalls")) {
        return read_proc_file(shell, "/proc/syscalls", out, out_size);
    }
    if (str_eq(command->argv[0], "cow")) {
        return read_proc_file(shell, "/proc/cow", out, out_size);
    }
    if (str_eq(command->argv[0], "crashlog")) {
        return read_proc_file(shell, "/proc/crashlog", out, out_size);
    }
    if (str_eq(command->argv[0], "cpuinfo")) {
        return read_proc_file(shell, "/proc/cpuinfo", out, out_size);
    }
    if (str_eq(command->argv[0], "uptime")) {
        return read_proc_file(shell, "/proc/uptime", out, out_size);
    }
    if (str_eq(command->argv[0], "status")) {
        return read_pid_proc_file(shell, command, "status", out, out_size);
    }
    if (str_eq(command->argv[0], "fd")) {
        return read_pid_proc_file(shell, command, "fd", out, out_size);
    }
    if (str_eq(command->argv[0], "maps")) {
        return read_pid_proc_file(shell, command, "maps", out, out_size);
    }
    if (str_eq(command->argv[0], "sem")) {
        return run_sem_command(shell,
                               command,
                               out,
                               out_size,
                               &used,
                               command_success);
    }
    if (str_eq(command->argv[0], "mutex")) {
        return run_mutex_command(shell,
                                 command,
                                 out,
                                 out_size,
                                 &used,
                                 command_success);
    }
    if (str_eq(command->argv[0], "concurrency_demo")) {
        return run_concurrency_demo(shell,
                                    out,
                                    out_size,
                                    &used,
                                    command_success);
    }
    if (str_eq(command->argv[0], "ls")) {
        const char* target = command->argc > 1U ? command->argv[1]
                                                  : course_fd_cwd(&shell->fds);
        char resolved[COURSE_FD_MAX_PATH];

        if (target[0] != '/' &&
            course_fd_resolve_path(&shell->fds, target, resolved,
                                   sizeof(resolved)) == COURSE_FD_OK) {
            target = resolved;
        }
        if (course_fs_listdir(&shell->fs, target, out, out_size)) {
            return true;
        }
        set_command_success(command_success, false);
        return append_str(out, out_size, &used, "ls: cannot access ") &&
               append_str(out, out_size, &used, target) &&
               append_char(out, out_size, &used, '\n');
    }
    if (str_eq(command->argv[0], "kill")) {
        uint32_t target_pid = 0U;
        size_t i = 0;
        unsigned long parsed = 0UL;
        const char* digits = command->argc > 1U ? command->argv[1] : "";

        if (digits[0] == '\0') {
            set_command_success(command_success, false);
            return append_str(out, out_size, &used, "kill: missing pid\n");
        }
        for (i = 0; digits[i] != '\0'; ++i) {
            if (digits[i] < '0' || digits[i] > '9') {
                set_command_success(command_success, false);
                return append_str(out, out_size, &used,
                                 "kill: invalid pid\n");
            }
            parsed = parsed * 10U + (unsigned long)(digits[i] - '0');
        }
        target_pid = (uint32_t)parsed;
        if (target_pid == 1U || target_pid == shell->shell_pid) {
            set_command_success(command_success, false);
            return append_str(out, out_size, &used, "kill: permission denied\n");
        }
        if (!course_process_kill(&shell->processes, shell->shell_pid, target_pid)) {
            set_command_success(command_success, false);
            return append_str(out, out_size, &used, "kill: no such process\n");
        }
        set_command_success(command_success, true);
        return append_str(out, out_size, &used, "killed ") &&
               append_str(out, out_size, &used, digits) &&
               append_char(out, out_size, &used, '\n');
    }
    if (str_eq(command->argv[0], "exit")) {
        return append_str(out, out_size, &used, "exit\n");
    }
    if (str_eq(command->argv[0], "exec")) {
        course_shell_simple_command_t exec_command;
        size_t i = 0;

        if (command->argc < 2U) {
            return false;
        }
        if (command->argv[1][0] == '/') {
            size_t external_size = 0;

            if (!course_fs_size(&shell->fs, command->argv[1], &external_size)) {
                set_command_success(command_success, false);
                return append_str(out, out_size, &used, "exec: no such file\n");
            }
        }
        exec_command.argc = command->argc - 1U;
        for (i = 0; i < exec_command.argc; ++i) {
            copy_token(exec_command.argv[i],
                       sizeof(exec_command.argv[i]),
                       command->argv[i + 1U],
                       str_len(command->argv[i + 1U]));
        }
        if (!run_program_command(shell, &exec_command, out, out_size)) {
            set_command_success(command_success, false);
            if (command->argv[1][0] == '/') {
                out[0] = '\0';
                return append_str(out, out_size, &used, "exec: bad elf\n");
            }
            return false;
        }
        return true;
    }

    if (course_user_program_lookup(command->argv[0], &program)) {
        return run_program_command(shell, command, out, out_size);
    }
    return course_shell_run_linux_fallback_command(shell,
                                                   command,
                                                   stdin_text,
                                                   out,
                                                   out_size,
                                                   command_success);
}

/* 执行单条命令行（已切好的 pipeline + 重定向），按 stage 串联管道与重定向。 */
static bool run_single_command_line_internal(course_shell_t* shell,
                                             const char* line,
                                             char* out,
                                             size_t out_size,
                                             bool record_transcript,
                                             bool* command_success) {
    course_shell_command_t* command = 0;
    char* left_out = 0;
    const char* stdin_text = 0;
    bool ok = false;
    bool result = false;

    if (shell == 0 || line == 0 || out == 0 || out_size == 0) {
        return false;
    }
    set_command_success(command_success, false);
    left_out = shell->line_output_scratch;
    command = acquire_command_scratch(shell);
    if (command == 0) {
        return false;
    }

    if (!course_shell_parse(line, command) ||
        (record_transcript && !transcript_append(shell, line))) {
        goto out;
    }

    if (command->has_input_redirect) {
        int fd = course_fd_open(&shell->fds,
                                command->input_path,
                                COURSE_FD_OPEN_READ);

        if (fd < 0) {
            goto out;
        }
        ok = read_all_fd(&shell->fds,
                         fd,
                         left_out,
                         COURSE_SHELL_LINE_OUTPUT_SIZE);
        (void)course_fd_close(&shell->fds, fd);
        if (!ok) {
            goto out;
        }
        stdin_text = left_out;
    }

    if (command->pipeline_len == 0U) {
        goto out;
    }
    if (command->pipeline_len == 1U) {
        ok = run_simple(shell,
                        &command->pipeline[0],
                        stdin_text,
                        out,
                        out_size,
                        command_success);
        if (!ok) {
            goto out;
        }
    } else {
        char* stage_buffers[2] = {shell->line_output_scratch,
                                  shell->command_output_scratch};
        char* stage_input = (char*)stdin_text;
        size_t stage = 0;

        for (stage = 0; stage < command->pipeline_len; ++stage) {
            char* stage_output = stage_buffers[stage % 2U];
            const size_t stage_output_size = COURSE_SHELL_COMMAND_OUTPUT_SIZE;

            if (stage_output == stage_input) {
                stage_output = stage_buffers[(stage + 1U) % 2U];
            }

            ok = run_simple(shell,
                            &command->pipeline[stage],
                            stage_input,
                            stage_output,
                            stage_output_size,
                            command_success);
            if (!ok) {
                goto out;
            }
            stage_input = stage_output;
        }
        if (stage_input != out) {
            size_t copied = 0;

            out[0] = '\0';
            if (!append_str(out, out_size, &copied, stage_input)) {
                goto out;
            }
        }
    }
    if (command->has_output_redirect) {
        int fd = course_fd_open(&shell->fds,
                                command->output_path,
                                COURSE_FD_OPEN_CREATE | COURSE_FD_OPEN_WRITE);
        size_t len = str_len(out);

        if (fd < 0 ||
            course_fd_write(&shell->fds, fd, out, len) != (int)len ||
            course_fd_close(&shell->fds, fd) != COURSE_FD_OK) {
            goto out;
        }
    }
    result = true;

out:
    release_command_scratch(shell);
    return result;
}

/* 执行一行命令：按 && 切分多段，逐段执行并短路失败；可选记 transcript。 */
static bool course_shell_run_line_internal(course_shell_t* shell,
                                           const char* line,
                                           char* out,
                                           size_t out_size,
                                           bool record_transcript) {
    char segment[128];
    char* segment_out = 0;
    const char* cursor = line;
    size_t used = 0;
    size_t first_and_offset = 0;
    bool segment_success = false;

    if (shell == 0 || line == 0 || out == 0 || out_size == 0) {
        return false;
    }

    if (!find_and_separator(line, &first_and_offset)) {
        return run_single_command_line_internal(shell,
                                                line,
                                                out,
                                                out_size,
                                                record_transcript,
                                                0);
    }

    if (record_transcript && !transcript_append(shell, line)) {
        return false;
    }
    segment_out = shell->command_output_scratch;
    out[0] = '\0';

    while (true) {
        size_t and_offset = 0;
        const bool has_and = find_and_separator(cursor, &and_offset);
        const size_t segment_len = has_and ? and_offset : str_len(cursor);

        if (!copy_trimmed_segment(segment,
                                  sizeof(segment),
                                  cursor,
                                  segment_len) ||
            !run_single_command_line_internal(shell,
                                              segment,
                                              segment_out,
                                              COURSE_SHELL_COMMAND_OUTPUT_SIZE,
                                              false,
                                              &segment_success) ||
            !append_str(out, out_size, &used, segment_out)) {
            return false;
        }

        if (!has_and || !segment_success) {
            return true;
        }
        cursor += and_offset + 2U;
    }
}

bool course_shell_run_line(course_shell_t* shell,
                           const char* line,
                           char* out,
                           size_t out_size) {
    return course_shell_run_line_internal(shell, line, out, out_size, true);
}

bool course_shell_transcript(const course_shell_t* shell,
                             char* out,
                             size_t out_size) {
    size_t used = 0;

    if (shell == 0 || out == 0 || out_size == 0) {
        return false;
    }
    out[0] = '\0';
    return append_str(out, out_size, &used, shell->transcript);
}
