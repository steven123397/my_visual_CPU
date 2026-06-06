#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/linux_compat.h"
#include "../../guest/include/linux_compat_rootfs.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static char g_console_buffer[128];
static size_t g_console_size = 0;

void console_putc(char ch) {
    if (g_console_size + 1U < sizeof(g_console_buffer)) {
        g_console_buffer[g_console_size++] = ch;
        g_console_buffer[g_console_size] = '\0';
    }
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static uint64_t read_u64_le(const uint8_t* bytes, size_t offset) {
    uint64_t value = 0;
    size_t i = 0;

    for (i = 0; i < 8U; ++i) {
        value |= (uint64_t)bytes[offset + i] << (i * 8U);
    }
    return value;
}

static int test_builtin_rootfs_provider_reports_source(void) {
    if (strcmp(linux_compat_rootfs_source_name(), "builtin") != 0 ||
        linux_compat_rootfs_node_count() < 6U) {
        return fail("expected builtin Linux compat rootfs provider");
    }
    return 0;
}

static int test_rootfs_stat_reports_linux_metadata(void) {
    linux_compat_stat_t stat;
    linux_compat_trace_t trace;

    if (linux_compat_stat_path("/bin/busybox", &stat, &trace) !=
            LINUX_COMPAT_OK ||
        !contains(trace.message, "stat: ok") ||
        stat.directory ||
        !stat.executable ||
        stat.size == 0U ||
        (stat.mode & LINUX_COMPAT_S_IFREG) != LINUX_COMPAT_S_IFREG ||
        (stat.mode & LINUX_COMPAT_S_IXUSR) == 0U) {
        return fail("expected /bin/busybox stat metadata to look executable");
    }

    if (linux_compat_stat_path("/usr/bin", &stat, &trace) != LINUX_COMPAT_OK ||
        !stat.directory ||
        stat.executable ||
        stat.size != 0U ||
        (stat.mode & LINUX_COMPAT_S_IFDIR) != LINUX_COMPAT_S_IFDIR) {
        return fail("expected /usr/bin stat metadata to look like a directory");
    }

    if (linux_compat_stat_path("/missing", &stat, &trace) !=
            LINUX_COMPAT_ERR_NO_SUCH_FILE ||
        trace.errno_value != 2 ||
        !contains(trace.message, "stat: no such file")) {
        return fail("expected missing stat to fail with ENOENT");
    }

    return 0;
}

static int test_fd_openat_read_lseek_close_uses_rootfs_bytes(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    uint8_t bytes[4] = {0U, 0U, 0U, 0U};
    int32_t fd = -1;

    linux_compat_runtime_init(&runtime);
    g_console_size = 0;
    g_console_buffer[0] = '\0';
    fd = linux_compat_openat(&runtime,
                             LINUX_COMPAT_AT_FDCWD,
                             "/bin/busybox",
                             LINUX_COMPAT_O_RDONLY,
                             &trace);
    if (fd < 3 || !contains(trace.message, "openat: ok")) {
        return fail("expected openat to allocate a Linux compat fd");
    }

    if (linux_compat_read(&runtime, fd, bytes, sizeof(bytes), &trace) != 4 ||
        bytes[0] != 0x7fU ||
        bytes[1] != 'E' ||
        bytes[2] != 'L' ||
        bytes[3] != 'F') {
        return fail("expected read to consume ELF bytes from the rootfs entry");
    }

    if (linux_compat_lseek(&runtime, fd, 0, 0U, &trace) != 0 ||
        linux_compat_read(&runtime, fd, bytes, 1U, &trace) != 1 ||
        bytes[0] != 0x7fU) {
        return fail("expected lseek to reset the fd cursor");
    }

    if (linux_compat_close(&runtime, fd, &trace) != 0 ||
        linux_compat_read(&runtime, fd, bytes, 1U, &trace) != -9) {
        return fail("expected close to release the fd and later read to fail");
    }

    return 0;
}

static int test_syscall_dispatch_covers_help_output_minimum(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    uint8_t stat_buffer[128];
    linux_compat_dirent_t dirents[LINUX_COMPAT_MAX_DIRENTS];
    linux_compat_trace_t trace;
    uint8_t bytes[4] = {0U, 0U, 0U, 0U};
    const char message[] = "BusyBox v1.36.1\n";
    int32_t fd = -1;

    linux_compat_runtime_init(&runtime);
    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));

    request.number = LINUX_COMPAT_SYS_BRK;
    request.addr = 0U;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value < 0) {
        return fail("expected brk(0) to return the current program break");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MMAP;
    request.length = 4096U;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value < 0) {
        return fail("expected mmap to reserve a deterministic anonymous range");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_NEWFSTATAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/bin/busybox";
    memset(stat_buffer, 0, sizeof(stat_buffer));
    request.stat = (linux_compat_stat_t*)stat_buffer;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != 0 ||
        read_u64_le(stat_buffer, 48U) == 0U) {
        return fail("expected newfstatat to expose rootfs file metadata");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/bin/busybox";
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value < 3) {
        return fail("expected openat syscall to return a fd");
    }
    fd = (int32_t)response.value;

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_READ;
    request.fd = fd;
    request.read_buffer = bytes;
    request.length = sizeof(bytes);
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != 4 ||
        bytes[0] != 0x7fU) {
        return fail("expected read syscall to return rootfs bytes");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/usr/bin";
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value < 3) {
        return fail("expected openat syscall to open a rootfs directory");
    }
    fd = (int32_t)response.value;

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_GETDENTS64;
    request.fd = fd;
    request.dirents = dirents;
    request.dirent_capacity = LINUX_COMPAT_MAX_DIRENTS;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value < 1 ||
        strcmp(dirents[0].name, "git") != 0 ||
        dirents[0].type != LINUX_COMPAT_DT_REG) {
        return fail("expected getdents64 to enumerate /usr/bin");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_WRITE;
    request.fd = 1;
    request.write_buffer = message;
    request.length = strlen(message);
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != (int64_t)strlen(message) ||
        !contains(runtime.stdout_buffer, "BusyBox v") ||
        !contains(g_console_buffer, "BusyBox v")) {
        return fail("expected write syscall to append to stdout buffer and UART");
    }

    memset(&request, 0, sizeof(request));
    request.number = 9999U;
    request.addr = 0x1234U;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL ||
        response.value != -38 ||
        trace.syscall_number != 9999U ||
        trace.pc != 0x1234U ||
        runtime.trace_count == 0U ||
        runtime.trace_records[runtime.trace_count - 1U].number != 9999U ||
        runtime.trace_records[runtime.trace_count - 1U].return_value != -38 ||
        runtime.trace_records[runtime.trace_count - 1U].errno_value != 38 ||
        runtime.trace_records[runtime.trace_count - 1U].pc != 0x1234U ||
        !contains(runtime.trace_records[runtime.trace_count - 1U].message,
                  "unsupported syscall")) {
        return fail("expected unsupported syscall to fail closed with ENOSYS");
    }

    return 0;
}

static int test_linux_run_requires_real_exec_context(void) {
    const char* busybox_argv[] = {"/bin/busybox", "--help"};
    const char* git_argv[] = {"/usr/bin/git", "-h"};
    linux_compat_exec_request_t request;
    linux_compat_trace_t trace;
    char out[1024];

    memset(&request, 0, sizeof(request));
    request.path = "/bin/busybox";
    request.argc = 2U;
    request.argv = busybox_argv;
    if (linux_compat_run(&request, out, sizeof(out), &trace) !=
            LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL ||
        !contains(out, "loader=static") ||
        !contains(out, "interp=none") ||
        !contains(out, "segments=1") ||
        !contains(out, "stack=2/0/12") ||
        !contains(out, "real exec context missing") ||
        contains(out, "BusyBox v") ||
        contains(out, "trace=brk/mmap")) {
        return fail("expected busybox run to require real exec context");
    }

    request.path = "/usr/bin/git";
    request.argc = 2U;
    request.argv = git_argv;
    if (linux_compat_run(&request, out, sizeof(out), &trace) !=
            LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL ||
        !contains(out, "loader=static") ||
        !contains(out, "interp=none") ||
        !contains(out, "real exec context missing") ||
        contains(out, "usage: git") ||
        contains(out, "trace=brk/mmap")) {
        return fail("expected git run to require real exec context");
    }

    return 0;
}

static int test_linux_run_fails_closed_for_dynamic_elf_without_interp(void) {
    const char* argv[] = {"/usr/bin/dynamic-app"};
    linux_compat_exec_request_t request;
    linux_compat_trace_t trace;
    char out[1024];

    memset(&request, 0, sizeof(request));
    request.path = "/usr/bin/dynamic-app";
    request.argc = 1U;
    request.argv = argv;
    if (linux_compat_run(&request, out, sizeof(out), &trace) !=
            LINUX_COMPAT_ERR_NO_SUCH_FILE ||
        !contains(out, "loader=dynamic") ||
        !contains(out, "interp=/lib/ld-musl-riscv64.so.1") ||
        !contains(out, "errno=2") ||
        !contains(out, "loader reason=interp missing")) {
        return fail("expected missing dynamic interpreter to fail closed");
    }

    return 0;
}

int main(void) {
    if (test_builtin_rootfs_provider_reports_source() != 0 ||
        test_rootfs_stat_reports_linux_metadata() != 0 ||
        test_fd_openat_read_lseek_close_uses_rootfs_bytes() != 0 ||
        test_syscall_dispatch_covers_help_output_minimum() != 0 ||
        test_linux_run_requires_real_exec_context() != 0 ||
        test_linux_run_fails_closed_for_dynamic_elf_without_interp() != 0) {
        return 1;
    }

    return 0;
}
