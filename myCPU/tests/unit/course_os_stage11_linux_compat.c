#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_shell.h"
#include "../../guest/include/linux_compat.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

void console_putc(char ch) {
    (void)ch;
}

static int dispatch(linux_compat_runtime_t* runtime,
                    linux_compat_syscall_request_t* request,
                    linux_compat_syscall_response_t* response,
                    linux_compat_trace_t* trace) {
    return linux_compat_syscall_dispatch(runtime, request, response, trace);
}

static int open_path(linux_compat_runtime_t* runtime,
                     const char* path,
                     uint32_t flags,
                     linux_compat_trace_t* trace) {
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = path;
    request.flags = flags;
    if (dispatch(runtime, &request, &response, trace) != LINUX_COMPAT_OK) {
        return -1000;
    }
    return (int)response.value;
}

static int stat_path(linux_compat_runtime_t* runtime,
                     const char* path,
                     linux_compat_stat_t* stat,
                     linux_compat_trace_t* trace) {
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    memset(stat, 0, sizeof(*stat));
    request.number = LINUX_COMPAT_SYS_NEWFSTATAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = path;
    request.stat = stat;
    if (dispatch(runtime, &request, &response, trace) != LINUX_COMPAT_OK) {
        return -1000;
    }
    return (int)response.value;
}

static int64_t write_fd(linux_compat_runtime_t* runtime,
                        int fd,
                        const void* buffer,
                        size_t length,
                        linux_compat_trace_t* trace) {
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    request.number = LINUX_COMPAT_SYS_WRITE;
    request.fd = fd;
    request.write_buffer = buffer;
    request.length = length;
    if (dispatch(runtime, &request, &response, trace) != LINUX_COMPAT_OK) {
        return -1000;
    }
    return response.value;
}

static bool dirents_include(const linux_compat_dirent_t* dirents,
                            size_t count,
                            const char* name,
                            uint8_t type) {
    size_t i = 0;

    for (i = 0; i < count; ++i) {
        if (strcmp(dirents[i].name, name) == 0 && dirents[i].type == type) {
            return true;
        }
    }
    return false;
}

static bool contains(const char* haystack, const char* needle) {
    return haystack != 0 && needle != 0 && strstr(haystack, needle) != 0;
}

static int test_create_write_lseek_readback_and_stat(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_stat_t stat;
    const char payload[] = "stage11 hello\n";
    char readback[sizeof(payload)] = {0};
    int fd = -1;

    linux_compat_runtime_init(&runtime);
    fd = open_path(&runtime,
                   "/stage11.txt",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_TRUNC |
                       LINUX_COMPAT_O_WRONLY,
                   &trace);
    if (fd < 3) {
        return fail("expected O_CREAT|O_TRUNC|O_WRONLY to create overlay file");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_WRITE;
    request.fd = fd;
    request.write_buffer = payload;
    request.length = strlen(payload);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != (int64_t)strlen(payload)) {
        return fail("expected write to append bytes into overlay file");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_FTRUNCATE;
    request.fd = fd;
    request.length = 6U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected ftruncate to shrink writable overlay file");
    }

    if (linux_compat_lseek(&runtime, fd, 0, 0U, &trace) != 0) {
        return fail("expected lseek to rewind overlay fd");
    }

    memset(&request, 0, sizeof(request));
    memset(&stat, 0, sizeof(stat));
    request.number = LINUX_COMPAT_SYS_FSTAT;
    request.fd = fd;
    request.stat = &stat;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        stat.size != 6U ||
        (stat.mode & LINUX_COMPAT_S_IFREG) == 0U) {
        return fail("expected fstat to report overlay file metadata");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_FSYNC;
    request.fd = fd;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected fsync on overlay fd to succeed as no-op");
    }

    if (linux_compat_close(&runtime, fd, &trace) != 0) {
        return fail("expected close on overlay fd to succeed");
    }

    if (stat_path(&runtime, "/stage11.txt", &stat, &trace) != 0 ||
        stat.size != 6U ||
        stat.directory ||
        (stat.mode & LINUX_COMPAT_S_IFREG) == 0U) {
        return fail("expected newfstatat to report overlay file metadata");
    }

    fd = open_path(&runtime, "/stage11.txt", LINUX_COMPAT_O_RDONLY, &trace);
    if (fd < 3 ||
        linux_compat_read(&runtime, fd, readback, sizeof(readback), &trace) !=
            6 ||
        memcmp(readback, "stage1", 6U) != 0) {
        return fail("expected readback to see current overlay bytes");
    }

    return 0;
}

static int test_overlay_shadows_lower_rootfs(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_stat_t stat;
    const char payload[] = "overlay git\n";
    const char patch[] = "GIT";
    char readback[sizeof(payload)] = {0};
    int fd = -1;

    linux_compat_runtime_init(&runtime);
    fd = open_path(&runtime,
                   "/usr/bin/git",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_TRUNC |
                       LINUX_COMPAT_O_WRONLY,
                   &trace);
    if (fd < 3 ||
        write_fd(&runtime, fd, payload, strlen(payload), &trace) !=
            (int64_t)strlen(payload) ||
        linux_compat_close(&runtime, fd, &trace) != 0) {
        return fail("expected overlay write to replace lower /usr/bin/git");
    }

    fd = open_path(&runtime, "/usr/bin/git", LINUX_COMPAT_O_WRONLY, &trace);
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_PWRITE64;
    request.fd = fd;
    request.write_buffer = patch;
    request.length = strlen(patch);
    request.offset = 8U;
    if (fd < 3 ||
        dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != (int64_t)strlen(patch) ||
        runtime.fds[fd].offset != 0U ||
        linux_compat_close(&runtime, fd, &trace) != 0) {
        return fail("expected pwrite64 to update overlay file without advancing offset");
    }

    if (stat_path(&runtime, "/usr/bin/git", &stat, &trace) != 0 ||
        stat.size != strlen(payload)) {
        return fail("expected overlay stat to shadow lower rootfs metadata");
    }

    fd = open_path(&runtime, "/usr/bin/git", LINUX_COMPAT_O_RDONLY, &trace);
    if (fd < 3 ||
        linux_compat_read(&runtime, fd, readback, strlen(payload), &trace) !=
            (int64_t)strlen(payload) ||
        memcmp(readback, "overlay GIT\n", strlen(payload)) != 0) {
        return fail("expected overlay read to shadow lower rootfs bytes");
    }

    return 0;
}

static int test_mkdir_dirents_rename_unlink_and_sync(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_dirent_t dirents[LINUX_COMPAT_MAX_DIRENTS];
    linux_compat_stat_t stat;
    int fd = -1;
    int dir_fd = -1;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MKDIRAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/stage11dir";
    request.flags = 0755U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected mkdirat to create overlay directory");
    }

    fd = open_path(&runtime,
                   "/stage11dir/old.txt",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_TRUNC |
                       LINUX_COMPAT_O_WRONLY,
                   &trace);
    if (fd < 3 ||
        write_fd(&runtime, fd, "x", 1U, &trace) != 1 ||
        linux_compat_close(&runtime, fd, &trace) != 0) {
        return fail("expected create inside overlay directory to succeed");
    }

    dir_fd = open_path(&runtime, "/stage11dir", LINUX_COMPAT_O_RDONLY, &trace);
    memset(dirents, 0, sizeof(dirents));
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_GETDENTS64;
    request.fd = dir_fd;
    request.dirents = dirents;
    request.dirent_capacity = LINUX_COMPAT_MAX_DIRENTS;
    if (dir_fd < 3 ||
        dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value < 1 ||
        !dirents_include(dirents,
                         (size_t)response.value,
                         "old.txt",
                         LINUX_COMPAT_DT_REG)) {
        return fail("expected getdents64 to include overlay file");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_RENAMEAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/stage11dir/old.txt";
    request.new_path = "/stage11dir/new.txt";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected renameat to move overlay file");
    }

    if (stat_path(&runtime, "/stage11dir/new.txt", &stat, &trace) != 0 ||
        stat.size != 1U) {
        return fail("expected renamed overlay file to be visible");
    }
    if (stat_path(&runtime, "/stage11dir/old.txt", &stat, &trace) != -1000) {
        return fail("expected old overlay file path to disappear after rename");
    }

    fd = open_path(&runtime, "/stage11dir/new.txt", LINUX_COMPAT_O_RDONLY,
                   &trace);
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_FDATASYNC;
    request.fd = fd;
    if (fd < 3 ||
        dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected fdatasync on overlay fd to succeed as no-op");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_UNLINKAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/stage11dir/new.txt";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        stat_path(&runtime, "/stage11dir/new.txt", &stat, &trace) != -1000) {
        return fail("expected unlinkat to remove overlay file");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_SYNC;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected sync to succeed as no-op");
    }

    return 0;
}

static int test_bad_path_bad_fd_and_lower_guardrails(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    int fd = -1;

    linux_compat_runtime_init(&runtime);

    fd = open_path(&runtime,
                   "/missing-parent/file.txt",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_WRONLY,
                   &trace);
    if (fd != -1000) {
        return fail("expected O_CREAT without parent directory to fail");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_FTRUNCATE;
    request.fd = 7;
    request.length = 0;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != -9) {
        return fail("expected ftruncate on bad fd to return EBADF");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_UNLINKAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/bin/busybox";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != -30) {
        return fail("expected unlinkat lower readonly provider guardrail");
    }

    return 0;
}

static int test_cwd_relative_paths_and_dot_slash(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_stat_t stat;
    char readback[8] = {0};
    int fd = -1;

    linux_compat_runtime_init(&runtime);
    if (!linux_compat_runtime_set_cwd(&runtime, "/repo")) {
        return fail("expected runtime cwd set to accept absolute working directory");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MKDIRAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/repo";
    request.flags = 0755U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected mkdirat to create cwd directory");
    }

    fd = open_path(&runtime,
                   "hello.c",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_TRUNC |
                       LINUX_COMPAT_O_WRONLY,
                   &trace);
    if (fd < 3 ||
        write_fd(&runtime, fd, "hello", 5U, &trace) != 5 ||
        linux_compat_close(&runtime, fd, &trace) != 0) {
        return fail("expected relative openat to resolve under runtime cwd");
    }

    if (stat_path(&runtime, "/repo/hello.c", &stat, &trace) != 0 ||
        stat.size != 5U ||
        stat_path(&runtime, "hello.c", &stat, &trace) != 0 ||
        stat.size != 5U) {
        return fail("expected relative stat to see cwd overlay file");
    }

    fd = open_path(&runtime, "./hello.c", LINUX_COMPAT_O_RDONLY, &trace);
    if (fd < 3 ||
        linux_compat_read(&runtime, fd, readback, sizeof(readback), &trace) !=
            5 ||
        memcmp(readback, "hello", 5U) != 0) {
        return fail("expected ./ path to resolve under runtime cwd");
    }

    return 0;
}

static int test_course_shell_minimal_and_chain(void) {
    static course_shell_t shell;
    char out[512];

    course_shell_init(&shell);
    if (!course_shell_run_line(&shell,
                               "echo left && echo right",
                               out,
                               sizeof(out)) ||
        !contains(out, "left\n") ||
        !contains(out, "right\n")) {
        return fail("expected simple && chain to run right side after success");
    }

    if (!course_shell_run_line(&shell,
                               "linux /nope && echo should-not-run",
                               out,
                               sizeof(out)) ||
        !contains(out, "path=/nope errno=2") ||
        contains(out, "should-not-run")) {
        return fail("expected && chain to stop after linux fail-closed output");
    }

    return 0;
}

static int test_process_exec_wait_and_pipe_syscalls(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    int32_t pipefds[2] = {-1, -1};
    int32_t status = -1;
    int64_t child_pid = 0;
    char readback[8] = {0};

    linux_compat_runtime_init(&runtime);
    if (!linux_compat_runtime_set_cwd(&runtime, "/repo")) {
        return fail("expected process test cwd setup to succeed");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_CLONE;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value <= 1 ||
        !contains(trace.message, "clone")) {
        return fail("expected clone to create a minimal Linux compat child");
    }
    child_pid = response.value;

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_WAIT4;
    request.fd = (int32_t)child_pid;
    request.read_buffer = &status;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != child_pid ||
        status != 0 ||
        !contains(trace.message, "wait4")) {
        return fail("expected wait4 to reap the minimal child exit status");
    }

    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != -10) {
        return fail("expected repeated wait4 to return ECHILD");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_EXECVE;
    request.path = "/bin/busybox";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        !contains(trace.message, "execve")) {
        return fail("expected execve to accept an existing Linux rootfs path");
    }

    request.path = "/nope";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != -2) {
        return fail("expected execve bad path to return ENOENT");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_PIPE2;
    request.read_buffer = pipefds;
    request.flags = LINUX_COMPAT_O_CLOEXEC;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        pipefds[0] < 3 ||
        pipefds[1] < 3 ||
        pipefds[0] == pipefds[1] ||
        runtime.fds[pipefds[0]].fd_flags != LINUX_COMPAT_FD_CLOEXEC ||
        runtime.fds[pipefds[1]].fd_flags != LINUX_COMPAT_FD_CLOEXEC) {
        return fail("expected pipe2 to allocate close-on-exec read/write fds");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_DUP3;
    request.fd = pipefds[0];
    request.command = 5U;
    request.flags = LINUX_COMPAT_O_CLOEXEC;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 5 ||
        runtime.fds[5].fd_flags != LINUX_COMPAT_FD_CLOEXEC) {
        return fail("expected dup3 to duplicate the pipe read end with close-on-exec");
    }

    if (linux_compat_close(&runtime, pipefds[0], &trace) != 0 ||
        write_fd(&runtime, pipefds[1], "pipe", 4U, &trace) != 4 ||
        linux_compat_read(&runtime, 5, readback, sizeof(readback), &trace) !=
            4 ||
        memcmp(readback, "pipe", 4U) != 0) {
        return fail("expected pipe data to flow through duplicated fd");
    }

    return 0;
}

int main(void) {
    if (test_create_write_lseek_readback_and_stat() != 0 ||
        test_overlay_shadows_lower_rootfs() != 0 ||
        test_mkdir_dirents_rename_unlink_and_sync() != 0 ||
        test_bad_path_bad_fd_and_lower_guardrails() != 0 ||
        test_cwd_relative_paths_and_dot_slash() != 0 ||
        test_course_shell_minimal_and_chain() != 0 ||
        test_process_exec_wait_and_pipe_syscalls() != 0) {
        return 1;
    }
    return 0;
}
