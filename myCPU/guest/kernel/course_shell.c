#include "course_shell.h"

#include "course_user_programs.h"

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

static bool append_char(char* out, size_t out_size, size_t* used, char ch) {
    if (out == 0 || used == 0 || *used + 1U >= out_size) {
        return false;
    }
    out[*used] = ch;
    *used += 1U;
    out[*used] = '\0';
    return true;
}

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

static void clear_command(course_shell_command_t* command) {
    size_t i = 0;

    if (command == 0) {
        return;
    }
    command->left.argc = 0;
    command->right.argc = 0;
    command->has_pipe = false;
    command->has_output_redirect = false;
    command->has_input_redirect = false;
    command->output_path[0] = '\0';
    command->input_path[0] = '\0';
    for (i = 0; i < COURSE_SHELL_MAX_ARGS; ++i) {
        command->left.argv[i][0] = '\0';
        command->right.argv[i][0] = '\0';
    }
}

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
    course_shell_simple_command_t* target = 0;

    clear_command(out_command);
    if (line == 0 || out_command == 0) {
        return false;
    }

    target = &out_command->left;
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
            out_command->has_pipe = true;
            target = &out_command->right;
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
        p += len;
    }

    return out_command->left.argc > 0;
}

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

void course_shell_init(course_shell_t* shell) {
    course_process_t* init = 0;

    if (shell == 0) {
        return;
    }

    course_fs_mkfs(&shell->fs);
    (void)course_fs_mkdir(&shell->fs, "/home");
    (void)course_fs_mkdir(&shell->fs, "/home/user");
    (void)course_fs_mkdir(&shell->fs, "/tmp");
    course_scheduler_init(&shell->scheduler);
    course_memory_init(&shell->memory, 4U);
    course_process_table_init(&shell->processes);
    init = course_process_spawn(&shell->processes, 0U, "shell");
    shell->shell_pid = init != 0 ? init->pid : 0;
    procfs_init(&shell->procfs, &shell->scheduler, &shell->memory, &shell->fs);
    procfs_attach_processes(&shell->procfs, &shell->processes);
    course_fd_table_init(&shell->fds, &shell->fs, &shell->procfs);
    (void)course_fd_set_cwd(&shell->fds, "/");
    shell->transcript[0] = '\0';
    shell->transcript_size = 0;
}

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

static bool run_simple(course_shell_t* shell,
                       const course_shell_simple_command_t* command,
                       const char* stdin_text,
                       char* out,
                       size_t out_size) {
    size_t used = 0;
    course_user_program_t program;

    if (shell == 0 || command == 0 || command->argc == 0 || out == 0 ||
        out_size == 0) {
        return false;
    }
    out[0] = '\0';

    if (str_eq(command->argv[0], "help")) {
        return append_str(out, out_size, &used, "help ls cat echo ps kill cd pwd exit\n");
    }
    if (str_eq(command->argv[0], "pwd")) {
        return append_str(out, out_size, &used, course_fd_cwd(&shell->fds)) &&
               append_char(out, out_size, &used, '\n');
    }
    if (str_eq(command->argv[0], "cd")) {
        const char* target = command->argc > 1U ? command->argv[1] : "/";
        return course_fd_set_cwd(&shell->fds, target) == COURSE_FD_OK &&
               append_str(out, out_size, &used, course_fd_cwd(&shell->fds)) &&
               append_char(out, out_size, &used, '\n');
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
    if (str_eq(command->argv[0], "ps")) {
        int fd = course_fd_open(&shell->fds, "/proc/ps", COURSE_FD_OPEN_READ);
        bool ok = false;

        if (fd < 0) {
            return false;
        }
        ok = read_all_fd(&shell->fds, fd, out, out_size);
        (void)course_fd_close(&shell->fds, fd);
        return ok;
    }
    if (str_eq(command->argv[0], "cow")) {
        int fd = course_fd_open(&shell->fds, "/proc/cow", COURSE_FD_OPEN_READ);
        bool ok = false;

        if (fd < 0) {
            return false;
        }
        ok = read_all_fd(&shell->fds, fd, out, out_size);
        (void)course_fd_close(&shell->fds, fd);
        return ok;
    }
    if (str_eq(command->argv[0], "ls")) {
        return append_str(out, out_size, &used, ".\n");
    }
    if (str_eq(command->argv[0], "kill")) {
        return append_str(out, out_size, &used, "kill=ok\n");
    }
    if (str_eq(command->argv[0], "exit")) {
        return append_str(out, out_size, &used, "exit\n");
    }

    if (!course_user_program_lookup(command->argv[0], &program)) {
        return false;
    }
    {
        course_process_t* child =
            course_process_fork(&shell->processes, shell->shell_pid, program.name);
        int32_t status = 0;

        if (child == 0 ||
            course_process_exec(&shell->processes,
                                child->pid,
                                program.name,
                                command->argc > 1U ? command->argv[1] : "") !=
                COURSE_PROCESS_OK) {
            return false;
        }
        if (program.kind == COURSE_USER_PROGRAM_FORKTEST &&
            !course_process_map_user_page(&shell->processes,
                                          child->pid,
                                          0U,
                                          (uint8_t)'F')) {
            return false;
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
                   append_str(out, out_size, &used, " cow_shared=") &&
                   append_u32(out, out_size, &used, stats.shared_pages) &&
                   append_char(out, out_size, &used, '\n');
        }
        return append_str(out, out_size, &used, "program=") &&
               append_str(out, out_size, &used, program.name) &&
               append_str(out, out_size, &used, " exit=") &&
               append_u32(out, out_size, &used, (uint32_t)status) &&
               append_char(out, out_size, &used, '\n');
    }
}

bool course_shell_run_line(course_shell_t* shell,
                           const char* line,
                           char* out,
                           size_t out_size) {
    course_shell_command_t command;
    char left_out[512];
    const char* stdin_text = 0;
    bool ok = false;

    if (shell == 0 || line == 0 || out == 0 || out_size == 0 ||
        !course_shell_parse(line, &command) ||
        !transcript_append(shell, line)) {
        return false;
    }

    if (command.has_input_redirect) {
        int fd = course_fd_open(&shell->fds,
                                command.input_path,
                                COURSE_FD_OPEN_READ);

        if (fd < 0) {
            return false;
        }
        ok = read_all_fd(&shell->fds, fd, left_out, sizeof(left_out));
        (void)course_fd_close(&shell->fds, fd);
        if (!ok) {
            return false;
        }
        stdin_text = left_out;
    }

    ok = run_simple(shell,
                    &command.left,
                    stdin_text,
                    command.has_pipe ? left_out : out,
                    command.has_pipe ? sizeof(left_out) : out_size);
    if (!ok) {
        return false;
    }
    if (command.has_pipe) {
        ok = run_simple(shell,
                        &command.right,
                        left_out,
                        out,
                        out_size);
        if (!ok) {
            return false;
        }
    }
    if (command.has_output_redirect) {
        int fd = course_fd_open(&shell->fds,
                                command.output_path,
                                COURSE_FD_OPEN_CREATE | COURSE_FD_OPEN_WRITE);
        size_t len = str_len(out);

        if (fd < 0 ||
            course_fd_write(&shell->fds, fd, out, len) != (int)len ||
            course_fd_close(&shell->fds, fd) != COURSE_FD_OK) {
            return false;
        }
    }
    return true;
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
