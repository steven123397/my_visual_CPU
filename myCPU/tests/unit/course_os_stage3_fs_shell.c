/* Stage3 FS/shell 单测：覆盖 mkfs、seek、unlink/rmdir 和脚本化 shell 输出。 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_fd.h"
#include "../../guest/include/course_fs.h"
#include "../../guest/include/course_memory.h"
#include "../../guest/include/course_scheduler.h"
#include "../../guest/include/course_shell.h"
#include "../../guest/include/procfs.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static void fill_pattern(char* out, size_t size) {
    size_t i = 0;

    for (i = 0; i < size; ++i) {
        out[i] = (char)('a' + (i % 26U));
    }
}

static int test_fs_mkfs_seek_unlink_rmdir_and_capacity(void) {
    static course_fs_t fs;
    static char big[COURSE_FS_MAX_DATA];
    char name[64];
    char out[8];
    size_t i = 0;

    course_fs_mkfs(&fs);
    course_fs_mkdir(&fs, "/a");
    course_fs_mkdir(&fs, "/a/b");
    course_fs_mkdir(&fs, "/a/b/c");
    for (i = 0; i < 128U; ++i) {
        snprintf(name, sizeof(name), "/a/b/c/f%03zu", i);
        if (!course_fs_create(&fs, name, false)) {
            return fail("expected 128 file capacity");
        }
    }

    fill_pattern(big, sizeof(big));
    if (!course_fs_create(&fs, "/a/b/c/big", false) ||
        !course_fs_write(&fs, "/a/b/c/big", 0U, big, sizeof(big)) ||
        !course_fs_read(&fs, "/a/b/c/big", 4096U, out, 4U) ||
        out[0] != big[4096U]) {
        return fail("expected 64KB file and seek-like offset read");
    }

    if (!course_fs_create(&fs, "/a/b/c/remove", false) ||
        !course_fs_unlink(&fs, "/a/b/c/remove") ||
        course_fs_lookup(&fs, "/a/b/c/remove") ||
        course_fs_unlink(&fs, "/a/b/c") ||
        !course_fs_mkdir(&fs, "/a/b/c/empty") ||
        !course_fs_rmdir(&fs, "/a/b/c/empty") ||
        course_fs_rmdir(&fs, "/a/b/c")) {
        return fail("expected unlink/rmdir guardrails");
    }

    course_fs_mkfs(&fs);
    if (course_fs_lookup(&fs, "/a/b/c/f001")) {
        return fail("expected mkfs to reset previous tree");
    }

    return 0;
}

static int test_fd_seek_only_accepts_regular_files(void) {
    static course_fs_t fs;
    course_scheduler_t scheduler;
    course_memory_t memory;
    procfs_t procfs;
    course_fd_table_t fds;
    int file_fd = -1;
    int proc_fd = -1;

    course_fs_mkfs(&fs);
    course_fs_create(&fs, "/regular", false);
    course_fs_write(&fs, "/regular", 0U, "abcdef", 6U);
    course_scheduler_init(&scheduler);
    course_memory_init(&memory, 2U);
    procfs_init(&procfs, &scheduler, &memory, &fs);
    course_fd_table_init(&fds, &fs, &procfs);

    file_fd = course_fd_open(&fds, "/regular", COURSE_FD_OPEN_READ);
    proc_fd = course_fd_open(&fds, "/proc/fsstat", COURSE_FD_OPEN_READ);
    if (file_fd < 3 ||
        proc_fd < 3 ||
        course_fd_seek(&fds, file_fd, 2U) != COURSE_FD_OK ||
        course_fd_seek(&fds, proc_fd, 0U) !=
            COURSE_FD_ERR_PERMISSION_DENIED ||
        course_fd_seek(&fds, 99, 0U) != COURSE_FD_ERR_BAD_FD) {
        return fail("expected seek to work only for regular file fds");
    }

    return 0;
}

static int test_shell_script_mode_success_and_failure_line(void) {
    static course_shell_t shell;
    char out[2048];
    char transcript[2048];
    const char* script =
        "#!/bin/sh\n"
        "# ignored comment\n"
        "\n"
        "echo stage3\n"
        "echo profile > /tmp/profile\n"
        "cat /tmp/profile\n";
    const char* bad_script =
        "echo before\n"
        "missing-command\n"
        "echo after\n";

    course_shell_init(&shell);
    course_fs_create(&shell.fs, "/demo.sh", false);
    course_fs_write(&shell.fs, "/demo.sh", 0U, script, strlen(script));
    if (!course_shell_run_line(&shell, "sh /demo.sh", out, sizeof(out)) ||
        !contains(out, "stage3") ||
        !contains(out, "profile") ||
        !contains(out, "stage3\nprofile\n") ||
        !course_shell_transcript(&shell, transcript, sizeof(transcript)) ||
        !contains(transcript, "$ sh /demo.sh") ||
        !contains(transcript, "$ echo stage3") ||
        !contains(transcript, "$ cat /tmp/profile")) {
        return fail("expected shell script mode to run non-comment lines");
    }

    course_fs_create(&shell.fs, "/bad.sh", false);
    course_fs_write(&shell.fs, "/bad.sh", 0U, bad_script, strlen(bad_script));
    if (course_shell_run_line(&shell, "sh /bad.sh", out, sizeof(out)) ||
        !contains(out, "line=2") ||
        !contains(out, "missing-command") ||
        !contains(out, "before")) {
        return fail("expected script failure to report line and preserve output");
    }

    return 0;
}

static int test_exec_cat_reads_file_through_user_program_path(void) {
    static course_shell_t shell;
    char out[1024];

    course_shell_init(&shell);
    course_fs_mkdir(&shell.fs, "/demo");
    course_fs_create(&shell.fs, "/demo/input.txt", false);
    course_fs_write(&shell.fs, "/demo/input.txt", 0U, "stage3-cat", 10U);

    if (!course_shell_run_line(&shell,
                               "exec cat /demo/input.txt",
                               out,
                               sizeof(out)) ||
        !contains(out, "program=cat") ||
        !contains(out, "stage3-cat") ||
        !contains(out, "exit=0")) {
        return fail("expected exec cat to read a file through the user program path");
    }

    return 0;
}

static int test_linux_compat_launcher_is_explicit_and_fails_closed_without_runtime(void) {
    static course_shell_t shell;
    char out[1024];

    course_shell_init(&shell);

    if (!course_shell_run_line(&shell,
                               "linux /bin/busybox --help",
                               out,
                               sizeof(out)) ||
        !contains(out, "linux-compat:") ||
        !contains(out, "path=/bin/busybox") ||
        !contains(out, "elf=rv64-little") ||
        !contains(out, "exec=real") ||
        !contains(out, "errno=38") ||
        contains(out, "BusyBox v")) {
        return fail("expected explicit linux launcher to fail closed in host unit");
    }

    if (!course_shell_run_line(&shell, "linux /usr/bin/git -h", out, sizeof(out)) ||
        !contains(out, "linux-compat:") ||
        !contains(out, "path=/usr/bin/git") ||
        !contains(out, "elf=rv64-little") ||
        !contains(out, "exec=real") ||
        !contains(out, "errno=38") ||
        contains(out, "usage: git")) {
        return fail("expected git launcher to fail closed in host unit");
    }

    if (!course_shell_run_line(&shell, "linux /nope", out, sizeof(out)) ||
        !contains(out, "linux-compat:") ||
        !contains(out, "path=/nope") ||
        !contains(out, "errno=2") ||
        !contains(out, "no such file")) {
        return fail("expected bad linux compat path to emit fail-closed diagnostic");
    }

    if (!course_shell_run_line(&shell, "help", out, sizeof(out)) ||
        !contains(out, "exec sh meminfo") ||
        !course_shell_run_line(&shell, "exec hello", out, sizeof(out)) ||
        !contains(out, "program=hello") ||
        !course_shell_run_line(&shell, "git -h", out, sizeof(out)) ||
        !contains(out, "linux-compat:") ||
        !contains(out, "path=/usr/bin/git") ||
        !contains(out, "exec=real") ||
        !contains(out, "errno=38")) {
        return fail("expected course commands first and direct git fallback diagnostic");
    }

    return 0;
}

static int test_ls_with_path_argument(void) {
    static course_shell_t shell;
    char out[1024];

    course_shell_init(&shell);

    course_fs_create(&shell.fs, "/tmp/ls_test.txt", false);
    course_fs_write(&shell.fs, "/tmp/ls_test.txt", 0U, "data", 4U);

    if (!course_shell_run_line(&shell, "ls /tmp", out, sizeof(out)) ||
        !contains(out, "ls_test.txt")) {
        return fail("expected ls /tmp to show created file");
    }

    course_fs_create(&shell.fs, "/home/user/readme.txt", false);
    if (!course_shell_run_line(&shell, "ls /home/user", out, sizeof(out)) ||
        !contains(out, "readme.txt")) {
        return fail("expected ls /home/user to show created file");
    }

    if (!course_shell_run_line(&shell, "cd /home/user", out, sizeof(out)) ||
        !course_shell_run_line(&shell, "ls", out, sizeof(out)) ||
        !contains(out, "readme.txt")) {
        return fail("expected ls with cwd to list current dir entries");
    }

    return 0;
}

static int test_mkfs_command_reinitializes_shell_filesystem(void) {
    static course_shell_t shell;
    char out[1024];

    course_shell_init(&shell);
    if (!course_shell_run_line(&shell, "echo payload > /tmp/payload.txt", out, sizeof(out)) ||
        !course_shell_run_line(&shell, "ls /tmp", out, sizeof(out)) ||
        !contains(out, "payload.txt")) {
        return fail("expected setup file before mkfs");
    }

    if (!course_shell_run_line(&shell, "mkfs", out, sizeof(out)) ||
        !contains(out, "mkfs: filesystem initialized")) {
        return fail("expected mkfs command to report filesystem reset");
    }
    if (!course_shell_run_line(&shell, "ls", out, sizeof(out)) ||
        strcmp(out, "\n") != 0) {
        return fail("expected ls after mkfs to show empty root directory");
    }

    return 0;
}

int main(void) {
    if (test_fs_mkfs_seek_unlink_rmdir_and_capacity() != 0 ||
        test_fd_seek_only_accepts_regular_files() != 0 ||
        test_shell_script_mode_success_and_failure_line() != 0 ||
        test_exec_cat_reads_file_through_user_program_path() != 0 ||
        test_linux_compat_launcher_is_explicit_and_fails_closed_without_runtime() != 0 ||
        test_ls_with_path_argument() != 0 ||
        test_mkfs_command_reinitializes_shell_filesystem() != 0) {
        return 1;
    }

    return 0;
}
