#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_shell.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static int test_parser_pipeline_and_redirection(void) {
    course_shell_command_t command;

    if (!course_shell_parse("echo hello | cat > out.txt", &command) ||
        command.pipeline_len != 2U ||
        strcmp(command.pipeline[0].argv[0], "echo") != 0 ||
        strcmp(command.pipeline[0].argv[1], "hello") != 0 ||
        strcmp(command.pipeline[1].argv[0], "cat") != 0 ||
        strcmp(command.output_path, "out.txt") != 0 ||
        command.has_input_redirect ||
        !command.has_output_redirect) {
        return fail("expected shell parser to capture argv pipe and output redirect");
    }

    if (!course_shell_parse("cat < in.txt", &command) ||
        command.pipeline_len != 1U ||
        strcmp(command.pipeline[0].argv[0], "cat") != 0 ||
        strcmp(command.input_path, "in.txt") != 0 ||
        !command.has_input_redirect) {
        return fail("expected shell parser to capture input redirect");
    }

    if (!course_shell_parse("echo a | cat | cat", &command) ||
        command.pipeline_len != 3U ||
        strcmp(command.pipeline[0].argv[0], "echo") != 0 ||
        strcmp(command.pipeline[1].argv[0], "cat") != 0 ||
        strcmp(command.pipeline[2].argv[0], "cat") != 0) {
        return fail("expected shell parser to capture multi-stage pipeline");
    }

    if (course_shell_parse("| cat", &command) ||
        course_shell_parse("echo a |", &command) ||
        course_shell_parse("echo a || cat", &command) ||
        course_shell_parse("echo a > /tmp/mid | cat", &command)) {
        return fail("expected shell parser to reject invalid pipeline syntax");
    }

    return 0;
}

static int test_builtins_external_programs_and_transcript(void) {
    static course_shell_t shell;
    char out[1024];

    course_shell_init(&shell);
    if (!course_shell_run_line(&shell, "pwd", out, sizeof(out)) ||
        !contains(out, "/")) {
        return fail("expected pwd builtin");
    }
    if (!course_shell_run_line(&shell, "cd /home/user", out, sizeof(out)) ||
        !course_shell_run_line(&shell, "pwd", out, sizeof(out)) ||
        !contains(out, "/home/user")) {
        return fail("expected cd builtin to update cwd");
    }
    if (!course_shell_run_line(&shell, "echo hi", out, sizeof(out)) ||
        !contains(out, "hi")) {
        return fail("expected echo builtin");
    }
    if (!course_shell_run_line(&shell, "hello", out, sizeof(out)) ||
        !contains(out, "program=hello") ||
        !contains(out, "exit=0")) {
        return fail("expected external hello program");
    }
    if (!course_shell_run_line(&shell, "ps", out, sizeof(out)) ||
        !contains(out, "pid=")) {
        return fail("expected ps builtin to read process table");
    }

    if (!course_shell_transcript(&shell, out, sizeof(out)) ||
        !contains(out, "$ pwd") ||
        !contains(out, "$ hello")) {
        return fail("expected stable shell transcript");
    }

    return 0;
}

static int test_redirection_and_pipe_execution(void) {
    static course_shell_t shell;
    char out[1024];

    course_shell_init(&shell);
    if (!course_shell_run_line(&shell, "echo redirected > /tmp/out", out, sizeof(out)) ||
        !course_shell_run_line(&shell, "cat < /tmp/out", out, sizeof(out)) ||
        !contains(out, "redirected")) {
        return fail("expected output and input redirection through fd layer");
    }

    if (!course_shell_run_line(&shell, "echo pipe | cat", out, sizeof(out)) ||
        !contains(out, "pipe")) {
        return fail("expected single pipe to connect stdout to stdin");
    }

    if (!course_shell_run_line(&shell, "echo alpha | cat | cat", out, sizeof(out)) ||
        !contains(out, "alpha")) {
        return fail("expected multi-stage pipeline to propagate stdout through all stages");
    }

    if (!course_shell_run_line(&shell,
                               "echo nested pipe | cat | cat > /tmp/pipeline_out",
                               out,
                               sizeof(out)) ||
        !course_shell_run_line(&shell,
                               "cat < /tmp/pipeline_out",
                               out,
                               sizeof(out)) ||
        !contains(out, "nested pipe")) {
        return fail("expected multi-stage pipeline to cooperate with redirection");
    }

    if (!course_shell_run_line(&shell,
                               "echo chain-pipe | cat | cat && echo after",
                               out,
                               sizeof(out)) ||
        !contains(out, "chain-pipe") ||
        !contains(out, "after")) {
        return fail("expected multi-stage pipeline to preserve output inside && chains");
    }

    return 0;
}

static int test_and_chain_uses_command_status_not_output_text(void) {
    static course_shell_t shell;
    char out[1024];

    course_shell_init(&shell);
    if (!course_shell_run_line(&shell,
                               "echo linux-compat: && echo after",
                               out,
                               sizeof(out)) ||
        !contains(out, "linux-compat:\n") ||
        !contains(out, "after\n")) {
        return fail("expected && chain to use command status instead of output text");
    }

    if (!course_shell_run_line(&shell,
                               "kill abc && echo after",
                               out,
                               sizeof(out)) ||
        !contains(out, "kill: invalid pid") ||
        contains(out, "after\n")) {
        return fail("expected invalid kill to stop && chain");
    }

    return 0;
}

static int expect_shell_command_contains(course_shell_t* shell,
                                         const char* command,
                                         const char* expected) {
    char out[1024];

    if (!course_shell_run_line(shell, command, out, sizeof(out)) ||
        !contains(out, expected)) {
        fprintf(stderr,
                "expected `%s` to include `%s`, got:\n%s\n",
                command,
                expected,
                out);
        return 1;
    }

    return 0;
}

static int test_proc_shortcut_commands(void) {
    static course_shell_t shell;

    course_shell_init(&shell);
    if (expect_shell_command_contains(&shell, "meminfo", "total=") != 0 ||
        expect_shell_command_contains(&shell, "schedstat", "policy=") != 0 ||
        expect_shell_command_contains(&shell, "fsstat", "file_creates=") != 0 ||
        expect_shell_command_contains(&shell, "syscalls", "total_calls=") != 0 ||
        expect_shell_command_contains(&shell, "cow", "leak_free=") != 0 ||
        expect_shell_command_contains(&shell, "crashlog", "none") != 0 ||
        expect_shell_command_contains(&shell, "cpuinfo", "isa=rv64im") != 0 ||
        expect_shell_command_contains(&shell, "uptime", "ticks=") != 0 ||
        expect_shell_command_contains(&shell, "status", "pid=1") != 0 ||
        expect_shell_command_contains(&shell, "fd", "fd=0 kind=stdio") != 0 ||
        expect_shell_command_contains(&shell, "maps", "stack") != 0 ||
        expect_shell_command_contains(&shell, "status 1", "pid=1") != 0 ||
        expect_shell_command_contains(&shell, "fd 1", "fd=0 kind=stdio") != 0 ||
        expect_shell_command_contains(&shell, "maps 1", "stack") != 0 ||
        expect_shell_command_contains(&shell, "help", "meminfo schedstat fsstat") != 0) {
        return 1;
    }

    return 0;
}

static int test_kill_command_in_shell(void) {
    static course_shell_t shell;
    course_process_t* child = NULL;
    int32_t status = 0;
    char out[1024];

    course_shell_init(&shell);
    child = course_process_fork(&shell.processes, shell.shell_pid, "victim");
    if (child == NULL) {
        return fail("expected shell test to fork a kill target");
    }

    if (!course_shell_run_line(&shell, "kill", out, sizeof(out)) ||
        !contains(out, "missing pid")) {
        return fail("expected kill without pid to report missing pid");
    }

    if (!course_shell_run_line(&shell, "kill abc", out, sizeof(out)) ||
        !contains(out, "invalid pid")) {
        return fail("expected kill with non-numeric pid to report invalid pid");
    }

    if (!course_shell_run_line(&shell, "kill 1", out, sizeof(out)) ||
        !contains(out, "permission denied")) {
        return fail("expected kill init to fail with permission denied");
    }

    {
        char command[32];

        snprintf(command, sizeof(command), "kill %u", child->pid);
        if (!course_shell_run_line(&shell, command, out, sizeof(out)) ||
        !contains(out, "killed") ||
        child->state != COURSE_PROCESS_ZOMBIE ||
        course_process_waitpid(&shell.processes,
                               shell.shell_pid,
                               child->pid,
                               &status) != COURSE_PROCESS_OK ||
        status != 9) {
            return fail("expected shell kill to terminate and reap target process");
        }
    }

    return 0;
}

static int test_ls_lists_directory_contents(void) {
    static course_shell_t shell;
    char out[1024];

    course_shell_init(&shell);

    if (!course_shell_run_line(&shell, "ls", out, sizeof(out)) ||
        !contains(out, "home") ||
        !contains(out, "tmp")) {
        return fail("expected ls with no args to list cwd entries including home and tmp");
    }

    if (!course_shell_run_line(&shell, "ls /home", out, sizeof(out)) ||
        !contains(out, "user")) {
        return fail("expected ls /home to list user directory");
    }

    if (!course_fs_create(&shell.fs, "/home/user/test.txt", false) ||
        !course_shell_run_line(&shell, "ls /home/user", out, sizeof(out)) ||
        !contains(out, "test.txt")) {
        return fail("expected ls /home/user to show newly created file");
    }

    if (!course_shell_run_line(&shell, "ls /no/such/path", out, sizeof(out)) ||
        !contains(out, "ls: cannot access")) {
        return fail("expected ls on nonexistent path to report a diagnostic");
    }

    if (!course_shell_run_line(&shell,
                               "ls /no/such/path && echo after",
                               out,
                               sizeof(out)) ||
        !contains(out, "ls: cannot access") ||
        contains(out, "after\n")) {
        return fail("expected failed ls to stop && chain");
    }

    return 0;
}

int main(void) {
    if (test_parser_pipeline_and_redirection() != 0 ||
        test_builtins_external_programs_and_transcript() != 0 ||
        test_redirection_and_pipe_execution() != 0 ||
        test_and_chain_uses_command_status_not_output_text() != 0 ||
        test_proc_shortcut_commands() != 0 ||
        test_kill_command_in_shell() != 0 ||
        test_ls_lists_directory_contents() != 0) {
        return 1;
    }

    return 0;
}
