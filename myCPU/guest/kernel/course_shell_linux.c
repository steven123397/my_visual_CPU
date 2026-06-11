#include "course_shell_linux.h"

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

static void set_command_success(bool* command_success, bool value) {
    if (command_success != 0) {
        *command_success = value;
    }
}

bool course_shell_run_linux_command(course_shell_t* shell,
                                    const course_shell_simple_command_t* command,
                                    const char* stdin_text,
                                    char* out,
                                    size_t out_size,
                                    bool* command_success) {
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
    set_command_success(command_success, false);
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
    request.cwd = course_fd_cwd(&shell->fds);
    request.argc = command->argc - 1U;
    request.argv = argv;
    request.stdin_text = stdin_text;
    request.stdin_size = stdin_text != 0 ? str_len(stdin_text) : 0U;
    request.session_runtime = &shell->linux_compat_runtime;
    request.trap_context = trap_active_context();
    request.user_runtime = &shell->linux_compat_user_runtime;
    request.address_space = address_space;
    request.process = &shell->linux_compat_process;
    request.trap_stack_base = shell->linux_compat_trap_stack;
    request.trap_stack_size = sizeof(shell->linux_compat_trap_stack);
    result = linux_compat_run(&request, out, out_size, &shell->linux_trace);
    set_command_success(command_success,
                        result == LINUX_COMPAT_OK &&
                            shell->linux_compat_runtime.exit_code == 0);
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
        set_command_success(command_success, false);
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

bool course_shell_run_linux_fallback_command(
    course_shell_t* shell,
    const course_shell_simple_command_t* command,
    const char* stdin_text,
    char* out,
    size_t out_size,
    bool* command_success) {
    course_shell_simple_command_t linux_command;
    char resolved_path[LINUX_COMPAT_MAX_PATH];
    size_t i = 0;
    bool has_slash = false;

    if (shell == 0 || command == 0 || command->argc == 0U ||
        command->argc + 1U > COURSE_SHELL_MAX_ARGS) {
        return false;
    }
    for (i = 0; command->argv[0][i] != '\0'; ++i) {
        if (command->argv[0][i] == '/') {
            has_slash = true;
            break;
        }
    }
    if (has_slash) {
        copy_token(resolved_path,
                   sizeof(resolved_path),
                   command->argv[0],
                   str_len(command->argv[0]));
    } else if (linux_compat_resolve_path(command->argv[0],
                                         resolved_path,
                                         sizeof(resolved_path),
                                         &shell->linux_trace) !=
               LINUX_COMPAT_OK) {
        return false;
    }

    zero_bytes(&linux_command, sizeof(linux_command));
    linux_command.argc = command->argc + 1U;
    copy_token(linux_command.argv[0],
               sizeof(linux_command.argv[0]),
               "linux",
               5U);
    copy_token(linux_command.argv[1],
               sizeof(linux_command.argv[1]),
               resolved_path,
               str_len(resolved_path));
    for (i = 1U; i < command->argc; ++i) {
        copy_token(linux_command.argv[i + 1U],
                   sizeof(linux_command.argv[i + 1U]),
                   command->argv[i],
                   str_len(command->argv[i]));
    }
    return course_shell_run_linux_command(shell,
                                          &linux_command,
                                          stdin_text,
                                          out,
                                          out_size,
                                          command_success);
}
