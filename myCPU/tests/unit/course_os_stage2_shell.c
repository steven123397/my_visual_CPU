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
        strcmp(command.left.argv[0], "echo") != 0 ||
        strcmp(command.left.argv[1], "hello") != 0 ||
        strcmp(command.right.argv[0], "cat") != 0 ||
        strcmp(command.output_path, "out.txt") != 0 ||
        !command.has_pipe ||
        !command.has_output_redirect) {
        return fail("expected shell parser to capture argv pipe and output redirect");
    }

    if (!course_shell_parse("cat < in.txt", &command) ||
        strcmp(command.left.argv[0], "cat") != 0 ||
        strcmp(command.input_path, "in.txt") != 0 ||
        !command.has_input_redirect) {
        return fail("expected shell parser to capture input redirect");
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

int main(void) {
    if (test_parser_pipeline_and_redirection() != 0 ||
        test_builtins_external_programs_and_transcript() != 0 ||
        test_redirection_and_pipe_execution() != 0 ||
        test_and_chain_uses_command_status_not_output_text() != 0 ||
        test_proc_shortcut_commands() != 0) {
        return 1;
    }

    return 0;
}
