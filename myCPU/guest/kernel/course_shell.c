#include "course_shell.h"

#include "course_libc.h"
#include "course_user_programs.h"
#include "kernel_bringup.h"
#include "trap.h"
#include "vm.h"

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

static void zero_bytes(void* ptr, size_t size) {
    size_t i = 0;
    uint8_t* bytes = (uint8_t*)ptr;

    if (ptr == 0) {
        return;
    }
    for (i = 0; i < size; ++i) {
        bytes[i] = 0;
    }
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
    shell->linux_trace.path[0] = '\0';
    shell->linux_trace.errno_value = 0;
    shell->linux_trace.syscall_number = 0;
    shell->linux_trace.pc = 0;
    shell->linux_trace.message[0] = '\0';
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

static bool copy_cstr_to_user(char* out, size_t out_size, const char* value) {
    if (out == 0 || out_size == 0 || value == 0) {
        return false;
    }
    copy_token(out, out_size, value, str_len(value));
    return true;
}

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

static bool run_program_command(course_shell_t* shell,
                                const course_shell_simple_command_t* command,
                                char* out,
                                size_t out_size) {
    size_t used = 0;
    course_user_program_t program;
    course_process_t* child = 0;
    int32_t status = 0;
    char program_stdout[COURSE_SYSCALL_IO_BUFFER_SIZE];

    if (shell == 0 || command == 0 || command->argc == 0 || out == 0 ||
        out_size == 0 ||
        !course_user_program_lookup(command->argv[0], &program)) {
        return false;
    }

    out[0] = '\0';
    child = course_process_fork(&shell->processes, shell->shell_pid, program.name);
    if (child == 0 ||
        course_process_exec(&shell->processes,
                            child->pid,
                            program.name,
                            command->argc > 1U ? command->argv[1] : "") !=
            COURSE_PROCESS_OK) {
        return false;
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

static bool run_linux_command(course_shell_t* shell,
                              const course_shell_simple_command_t* command,
                              char* out,
                              size_t out_size) {
    linux_compat_exec_request_t request;
    linux_compat_result_t result = LINUX_COMPAT_OK;
    const char* argv[LINUX_COMPAT_MAX_ARGS];
    course_process_t* child = 0;
    vm_address_space_t* address_space = 0;
    int32_t status = 0;
    size_t i = 0;

    if (shell == 0 || command == 0 || out == 0 || out_size == 0) {
        return false;
    }
    if (command->argc < 2U) {
        size_t used = 0;

        out[0] = '\0';
        return append_str(out,
                          out_size,
                          &used,
                          "linux-compat: usage: linux <path-or-command> "
                          "[args...]\n");
    }
    if (command->argc - 1U > LINUX_COMPAT_MAX_ARGS) {
        size_t used = 0;

        out[0] = '\0';
        return append_str(out,
                          out_size,
                          &used,
                          "linux-compat: too many args\n");
    }

    zero_bytes(&request, sizeof(request));
    zero_bytes(&shell->linux_compat_process, sizeof(shell->linux_compat_process));
    trap_user_runtime_init(&shell->linux_compat_user_runtime);
    zero_bytes(shell->linux_compat_trap_stack,
               sizeof(shell->linux_compat_trap_stack));

    for (i = 1U; i < command->argc; ++i) {
        argv[i - 1U] = command->argv[i];
    }

    child = course_process_fork(&shell->processes,
                                shell->shell_pid,
                                command->argv[1]);
    if (child == 0 ||
        !course_process_set_abi(&shell->processes,
                                child->pid,
                                COURSE_PROCESS_ABI_LINUX_COMPAT)) {
        out[0] = '\0';
        i = 0;
        return append_str(out,
                          out_size,
                          &i,
                          "linux-compat: process setup failed\n");
    }

    if (!kernel_bringup_create_linux_compat_address_space(
            &address_space,
            KERNEL_BRINGUP_MMIO_UART | KERNEL_BRINGUP_MMIO_CLINT |
                KERNEL_BRINGUP_MMIO_PLIC | KERNEL_BRINGUP_MMIO_STORAGE |
                KERNEL_BRINGUP_MMIO_AI_ACCEL)) {
        if (address_space != 0) {
            (void)vm_address_space_destroy(address_space);
        }
        out[0] = '\0';
        i = 0;
        return append_str(out,
                          out_size,
                          &i,
                          "linux-compat: vm setup failed: kernel_bringup\n");
    }
    if (!vm_process_create(&shell->linux_compat_process, address_space)) {
        if (address_space != 0) {
            (void)vm_address_space_destroy(address_space);
        }
        out[0] = '\0';
        i = 0;
        return append_str(out,
                          out_size,
                          &i,
                          "linux-compat: vm setup failed: process_create\n");
    }

    request.path = command->argv[1];
    request.argc = command->argc - 1U;
    request.argv = argv;
    request.trap_context = trap_active_context();
    request.user_runtime = &shell->linux_compat_user_runtime;
    request.address_space = address_space;
    request.process = &shell->linux_compat_process;
    request.trap_stack_base = shell->linux_compat_trap_stack;
    request.trap_stack_size = sizeof(shell->linux_compat_trap_stack);
    result = linux_compat_run(&request, out, out_size, &shell->linux_trace);
    if (result != LINUX_COMPAT_OK && out[0] == '\0') {
        i = 0;
        (void)append_str(out,
                         out_size,
                         &i,
                         "linux-compat: run failed\n");
    }
    (void)vm_address_space_destroy(address_space);
    if (!course_process_exit(&shell->processes, child->pid, 0) ||
        course_process_waitpid(&shell->processes,
                               shell->shell_pid,
                               child->pid,
                               &status) != COURSE_PROCESS_OK) {
        if (out_size != 0) {
            out[0] = '\0';
        }
        i = 0;
        return append_str(out,
                          out_size,
                          &i,
                          "linux-compat: process teardown failed\n");
    }
    (void)status;
    return true;
}

static bool run_simple(course_shell_t* shell,
                       const course_shell_simple_command_t* command,
                       const char* stdin_text,
                       char* out,
                       size_t out_size) {
    size_t used = 0;

    if (shell == 0 || command == 0 || command->argc == 0 || out == 0 ||
        out_size == 0) {
        return false;
    }
    out[0] = '\0';

    if (str_eq(command->argv[0], "help")) {
        return append_str(out,
                          out_size,
                          &used,
                          "help ls cat echo ps kill cd pwd exit exec sh "
                          "meminfo schedstat fsstat syscalls cow crashlog "
                          "cpuinfo uptime status fd maps\n");
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
    if (str_eq(command->argv[0], "sh")) {
        if (command->argc < 2U) {
            return false;
        }
        return run_script(shell, command->argv[1], out, out_size);
    }
    if (str_eq(command->argv[0], "linux")) {
        return run_linux_command(shell, command, out, out_size);
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
    if (str_eq(command->argv[0], "ls")) {
        return append_str(out, out_size, &used, ".\n");
    }
    if (str_eq(command->argv[0], "kill")) {
        return append_str(out, out_size, &used, "kill=ok\n");
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
        exec_command.argc = command->argc - 1U;
        for (i = 0; i < exec_command.argc; ++i) {
            copy_token(exec_command.argv[i],
                       sizeof(exec_command.argv[i]),
                       command->argv[i + 1U],
                       str_len(command->argv[i + 1U]));
        }
        return run_program_command(shell, &exec_command, out, out_size);
    }

    return run_program_command(shell, command, out, out_size);
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
