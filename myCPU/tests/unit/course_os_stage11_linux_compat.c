#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_shell.h"
#include "../../guest/include/linux_compat.h"
#include "../../guest/include/linux_compat_minimal_elf_asset.h"
#include "../../guest/include/linux_compat_process.h"
#include "../../guest/include/linux_compat_vm.h"

#ifndef COURSE_SHELL_COMMAND_OUTPUT_SIZE
#define COURSE_SHELL_COMMAND_OUTPUT_SIZE 0U
#endif

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

void console_putc(char ch) {
    (void)ch;
}

static const uint8_t* g_uart_input = 0;
static size_t g_uart_input_size = 0;
static size_t g_uart_input_offset = 0;

static void set_uart_input(const uint8_t* input, size_t size) {
    g_uart_input = input;
    g_uart_input_size = size;
    g_uart_input_offset = 0;
}

uint64_t platform_uart_rx_ready(void) {
    return g_uart_input != 0 && g_uart_input_offset < g_uart_input_size ? 1U : 0U;
}

uint8_t platform_uart_getc(void) {
    if (g_uart_input == 0 || g_uart_input_offset >= g_uart_input_size) {
        return 0U;
    }
    return g_uart_input[g_uart_input_offset++];
}

static int dispatch(linux_compat_runtime_t* runtime,
                    linux_compat_syscall_request_t* request,
                    linux_compat_syscall_response_t* response,
                    linux_compat_trace_t* trace) {
    return linux_compat_syscall_dispatch(runtime, request, response, trace);
}

static uint32_t read_u32_le(const uint8_t* bytes, size_t offset);
static uint64_t read_u64_le(const uint8_t* bytes, size_t offset);

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
    enum {
        kLinuxStatInodeOffset = 8,
        kLinuxStatModeOffset = 16,
        kLinuxStatNlinkOffset = 20,
        kLinuxStatSizeOffset = 48,
        kLinuxStatSize = 128
    };
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    uint8_t stat_buffer[kLinuxStatSize];
    int result = 0;

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    memset(stat_buffer, 0, sizeof(stat_buffer));
    memset(stat, 0, sizeof(*stat));
    request.number = LINUX_COMPAT_SYS_NEWFSTATAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = path;
    request.stat = (linux_compat_stat_t*)stat_buffer;
    if (dispatch(runtime, &request, &response, trace) != LINUX_COMPAT_OK) {
        return -1000;
    }
    result = (int)response.value;
    if (result == 0) {
        stat->inode = read_u64_le(stat_buffer, kLinuxStatInodeOffset);
        stat->mode = read_u32_le(stat_buffer, kLinuxStatModeOffset);
        stat->nlink = read_u32_le(stat_buffer, kLinuxStatNlinkOffset);
        stat->size = read_u64_le(stat_buffer, kLinuxStatSizeOffset);
        stat->directory =
            (stat->mode & LINUX_COMPAT_S_IFDIR) == LINUX_COMPAT_S_IFDIR;
        stat->executable =
            (stat->mode & LINUX_COMPAT_S_IXUSR) == LINUX_COMPAT_S_IXUSR;
    }
    return result;
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

static uint32_t read_u32_le(const uint8_t* bytes, size_t offset) {
    return (uint32_t)bytes[offset] |
           ((uint32_t)bytes[offset + 1U] << 8U) |
           ((uint32_t)bytes[offset + 2U] << 16U) |
           ((uint32_t)bytes[offset + 3U] << 24U);
}

static uint64_t read_u64_le(const uint8_t* bytes, size_t offset) {
    uint64_t value = 0;
    size_t i = 0;

    for (i = 0; i < 8U; ++i) {
        value |= (uint64_t)bytes[offset + i] << (i * 8U);
    }
    return value;
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

    if (stat_path(&runtime, "/stage11.txt", &stat, &trace) != 0 ||
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

static int test_fstat_and_newfstatat_write_linux_abi_stat_layout(void) {
    enum {
        kLinuxStatModeOffset = 16,
        kLinuxStatSizeOffset = 48,
        kLinuxStatSize = 128
    };
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    uint8_t stat_buffer[kLinuxStatSize];
    const char payload[] = "stage11 git config\n";
    int fd = -1;

    linux_compat_runtime_init(&runtime);
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MKDIRAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/stage11repo";
    request.flags = 0755U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected stage11repo setup directory to succeed");
    }
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MKDIRAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/stage11repo/.git";
    request.flags = 0755U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected .git setup directory to succeed");
    }

    fd = open_path(&runtime,
                   "/stage11repo/.git/config",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_TRUNC |
                       LINUX_COMPAT_O_RDWR,
                   &trace);
    if (fd < 3 ||
        write_fd(&runtime, fd, payload, strlen(payload), &trace) !=
            (int64_t)strlen(payload)) {
        return fail("expected git config overlay file setup to succeed");
    }

    memset(stat_buffer, 0xa5, sizeof(stat_buffer));
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_FSTAT;
    request.fd = fd;
    request.stat = (linux_compat_stat_t*)stat_buffer;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        (read_u32_le(stat_buffer, kLinuxStatModeOffset) &
         LINUX_COMPAT_S_IFREG) == 0U ||
        read_u64_le(stat_buffer, kLinuxStatSizeOffset) != strlen(payload)) {
        return fail("expected fstat to write Linux ABI stat mode and size fields");
    }

    memset(stat_buffer, 0xa5, sizeof(stat_buffer));
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_NEWFSTATAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/stage11repo/.git/config";
    request.stat = (linux_compat_stat_t*)stat_buffer;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        (read_u32_le(stat_buffer, kLinuxStatModeOffset) &
         LINUX_COMPAT_S_IFREG) == 0U ||
        read_u64_le(stat_buffer, kLinuxStatSizeOffset) != strlen(payload)) {
        return fail("expected newfstatat to write Linux ABI stat mode and size fields");
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
    request.dirent_capacity = sizeof(dirents);
    if (dir_fd < 3 ||
        dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value < (int64_t)sizeof(linux_compat_dirent_t) ||
        !dirents_include(dirents,
                         (size_t)response.value /
                             sizeof(linux_compat_dirent_t),
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

static int test_opened_overlay_fd_survives_rename_unlink_until_close(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_stat_t stat;
    linux_compat_overlay_node_t* renamed_node = 0;
    linux_compat_overlay_node_t* unlinked_node = 0;
    char readback[8] = {0};
    uint64_t renamed_inode = 0;
    uint64_t renamed_mtime = 0;
    int fd = -1;
    int reuse_fd = -1;

    linux_compat_runtime_init(&runtime);
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MKDIRAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/stage11dir";
    request.flags = 0755U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected opened-fd test directory setup to succeed");
    }

    fd = open_path(&runtime,
                   "/stage11dir/open.txt",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_TRUNC |
                       LINUX_COMPAT_O_RDWR,
                   &trace);
    if (fd < 3 ||
        write_fd(&runtime, fd, "rename", 6U, &trace) != 6) {
        return fail("expected opened rename file setup to succeed");
    }
    renamed_node = (linux_compat_overlay_node_t*)runtime.fds[fd].node;
    renamed_inode = renamed_node->inode;
    renamed_mtime = renamed_node->mtime;
    if (renamed_node->nlink != 1U || renamed_node->open_count != 1U) {
        return fail("expected opened overlay file to start linked and open");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_RENAMEAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/stage11dir/open.txt";
    request.new_path = "/stage11dir/renamed.txt";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected opened fd rename syscall to succeed");
    }
    if (stat_path(&runtime, "/stage11dir/open.txt", &stat, &trace) != -1000) {
        return fail("expected old path to disappear after opened fd rename");
    }
    if (stat_path(&runtime, "/stage11dir/renamed.txt", &stat, &trace) != 0) {
        return fail("expected new path to appear after opened fd rename");
    }
    if (stat.inode != renamed_inode) {
        return fail("expected rename to preserve overlay inode");
    }
    if (stat.nlink != 1U) {
        return fail("expected rename to keep one overlay link");
    }
    if (renamed_node->mtime <= renamed_mtime) {
        return fail("expected rename to advance overlay mtime");
    }
    if (linux_compat_lseek(&runtime, fd, 0, 0U, &trace) != 0 ||
        linux_compat_read(&runtime, fd, readback, 6U, &trace) != 6 ||
        memcmp(readback, "rename", 6U) != 0 ||
        linux_compat_close(&runtime, fd, &trace) != 0) {
        return fail("expected old fd to remain readable after rename");
    }

    fd = open_path(&runtime,
                   "/stage11dir/unlink.txt",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_TRUNC |
                       LINUX_COMPAT_O_RDWR,
                   &trace);
    if (fd < 3 ||
        write_fd(&runtime, fd, "oldfd", 5U, &trace) != 5) {
        return fail("expected opened unlink file setup to succeed");
    }
    unlinked_node = (linux_compat_overlay_node_t*)runtime.fds[fd].node;

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_UNLINKAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/stage11dir/unlink.txt";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        stat_path(&runtime, "/stage11dir/unlink.txt", &stat, &trace) != -1000 ||
        !unlinked_node->used ||
        unlinked_node->nlink != 0U) {
        return fail("expected unlink to hide path while preserving opened node");
    }

    reuse_fd = open_path(&runtime,
                         "/stage11dir/reuse.txt",
                         LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_TRUNC |
                             LINUX_COMPAT_O_RDWR,
                         &trace);
    if (reuse_fd < 3 ||
        runtime.fds[reuse_fd].node == unlinked_node ||
        write_fd(&runtime, reuse_fd, "new", 3U, &trace) != 3 ||
        linux_compat_lseek(&runtime, fd, 0, 0U, &trace) != 0 ||
        linux_compat_read(&runtime, fd, readback, 5U, &trace) != 5 ||
        memcmp(readback, "oldfd", 5U) != 0) {
        return fail("expected opened unlinked fd to keep bytes until close");
    }
    if (linux_compat_close(&runtime, fd, &trace) != 0 ||
        unlinked_node->used ||
        linux_compat_close(&runtime, reuse_fd, &trace) != 0) {
        return fail("expected close after unlink to recycle overlay node");
    }

    return 0;
}

static int test_getdents64_advances_directory_offset_to_eof(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_dirent_t dirents[LINUX_COMPAT_MAX_DIRENTS];
    int fd = -1;

    linux_compat_runtime_init(&runtime);
    fd = open_path(&runtime, "/usr/bin", LINUX_COMPAT_O_RDONLY, &trace);
    if (fd < 3) {
        return fail("expected openat to open /usr/bin for getdents64 EOF test");
    }

    memset(dirents, 0, sizeof(dirents));
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_GETDENTS64;
    request.fd = fd;
    request.dirents = dirents;
    request.dirent_capacity = sizeof(dirents);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value < (int64_t)sizeof(linux_compat_dirent_t) ||
        (response.value % (int64_t)sizeof(linux_compat_dirent_t)) != 0 ||
        !dirents_include(dirents,
                         (size_t)response.value /
                             sizeof(linux_compat_dirent_t),
                         "git",
                         LINUX_COMPAT_DT_REG)) {
        return fail("expected getdents64 to return byte-sized /usr/bin dirents");
    }
    if (runtime.fds[fd].offset !=
        (size_t)response.value / sizeof(linux_compat_dirent_t)) {
        return fail("expected getdents64 to advance directory fd offset");
    }

    memset(dirents, 0, sizeof(dirents));
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        runtime.fds[fd].offset == 0U) {
        return fail("expected repeated getdents64 on consumed directory to return EOF");
    }

    return 0;
}

static int test_git_object_rename_supports_long_overlay_path(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_stat_t stat;
    const char* dirs[] = {
        "/stage11repo",
        "/stage11repo/.git",
        "/stage11repo/.git/objects",
        "/stage11repo/.git/objects/4b",
    };
    const char* tmp_path = "/stage11repo/.git/objects/4b/tmp_obj_kr78Np";
    const char* final_path =
        "/stage11repo/.git/objects/4b/"
        "825dc642cb6eb9a060e54bf8d69288fbee4904";
    size_t i = 0;
    int fd = -1;

    linux_compat_runtime_init(&runtime);
    for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
        memset(&request, 0, sizeof(request));
        request.number = LINUX_COMPAT_SYS_MKDIRAT;
        request.dirfd = LINUX_COMPAT_AT_FDCWD;
        request.path = dirs[i];
        request.flags = 0755U;
        if (dispatch(&runtime, &request, &response, &trace) !=
                LINUX_COMPAT_OK ||
            response.value != 0) {
            return fail("expected git object parent mkdirat setup to succeed");
        }
    }

    fd = open_path(&runtime,
                   tmp_path,
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_EXCL |
                       LINUX_COMPAT_O_WRONLY,
                   &trace);
    if (fd < 3 ||
        write_fd(&runtime, fd, "x", 1U, &trace) != 1 ||
        linux_compat_close(&runtime, fd, &trace) != 0) {
        return fail("expected git temporary object write setup to succeed");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_RENAMEAT2;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = tmp_path;
    request.command = LINUX_COMPAT_AT_FDCWD;
    request.new_path = final_path;
    request.flags = 0;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected git object renameat2 to accept long final path");
    }

    if (stat_path(&runtime, final_path, &stat, &trace) != 0 ||
        stat.size != 1U ||
        stat_path(&runtime, tmp_path, &stat, &trace) != -1000) {
        return fail("expected renamed git object to be visible at long final path");
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

static int test_getcwd_and_chdir_update_runtime_cwd(void) {
    const uint64_t kSysGetcwd = 17U;
    const uint64_t kSysChdir = 49U;
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    char cwd[LINUX_COMPAT_MAX_PATH];

    linux_compat_runtime_init(&runtime);

    memset(cwd, 0, sizeof(cwd));
    memset(&request, 0, sizeof(request));
    request.number = kSysGetcwd;
    request.read_buffer = cwd;
    request.length = sizeof(cwd);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 2 ||
        strcmp(cwd, "/") != 0) {
        return fail("expected getcwd to report initial root cwd");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MKDIRAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/repo";
    request.flags = 0755U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected mkdirat to create chdir target");
    }

    memset(&request, 0, sizeof(request));
    request.number = kSysChdir;
    request.path = "repo";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        strcmp(linux_compat_runtime_cwd(&runtime), "/repo") != 0) {
        return fail("expected relative chdir to update runtime cwd");
    }

    memset(cwd, 0, sizeof(cwd));
    memset(&request, 0, sizeof(request));
    request.number = kSysGetcwd;
    request.read_buffer = cwd;
    request.length = sizeof(cwd);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 6 ||
        strcmp(cwd, "/repo") != 0) {
        return fail("expected getcwd to report updated cwd");
    }

    memset(&request, 0, sizeof(request));
    request.number = kSysChdir;
    request.path = "/missing";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != -2 ||
        strcmp(linux_compat_runtime_cwd(&runtime), "/repo") != 0) {
        return fail("expected chdir missing directory to fail without changing cwd");
    }

    return 0;
}

static int test_getpid_and_fchmodat_for_overlay_file(void) {
    const uint64_t kSysFchmodat = 53U;
    const uint64_t kSysGetpid = 172U;
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_stat_t stat;
    int fd = -1;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = kSysGetpid;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 1) {
        return fail("expected getpid to return current Linux compat pid");
    }

    fd = open_path(&runtime,
                   "/config.lock",
                   LINUX_COMPAT_O_RDWR | LINUX_COMPAT_O_CREAT |
                       LINUX_COMPAT_O_EXCL,
                   &trace);
    if (fd < 3 || linux_compat_close(&runtime, fd, &trace) != 0) {
        return fail("expected config.lock overlay setup for fchmodat");
    }

    memset(&request, 0, sizeof(request));
    request.number = kSysFchmodat;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/config.lock";
    request.flags = 0644U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        stat_path(&runtime, "/config.lock", &stat, &trace) != 0 ||
        (stat.mode & 0777U) != 0644U ||
        (stat.mode & LINUX_COMPAT_S_IFREG) == 0U) {
        return fail("expected fchmodat to update overlay file mode bits");
    }

    memset(&request, 0, sizeof(request));
    request.number = kSysFchmodat;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/missing";
    request.flags = 0644U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != -2) {
        return fail("expected fchmodat missing path to return ENOENT");
    }

    return 0;
}

static int test_course_shell_minimal_and_chain(void) {
    static course_shell_t shell;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    course_shell_command_t command;
    char out[4096];
    int fd = -1;

    if (!course_shell_parse(
            "git -C stage11repo -c user.name=stage11 "
            "-c user.email=stage11@example.invalid commit -m init",
            &command) ||
        command.pipeline_len != 1U ||
        command.pipeline[0].argc != 10U ||
        strcmp(command.pipeline[0].argv[0], "git") != 0 ||
        strcmp(command.pipeline[0].argv[9], "init") != 0) {
        return fail("expected Stage 11 git commit command to fit shell argv budget");
    }

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
                               "echo one && echo two && echo three",
                               out,
                               sizeof(out)) ||
        !contains(out, "one\n") ||
        !contains(out, "two\n") ||
        !contains(out, "three\n")) {
        return fail("expected nested && chain to run iteratively left to right");
    }
    if (!course_shell_run_line(&shell, "echo piped | cat", out, sizeof(out)) ||
        !contains(out, "piped\n")) {
        return fail("expected pipe input scratch to survive right command output reset");
    }

    if (!course_shell_run_line(&shell,
                               "linux /nope && echo should-not-run",
                               out,
                               sizeof(out)) ||
        !contains(out, "path=/nope errno=2") ||
        contains(out, "should-not-run")) {
        return fail("expected && chain to stop after linux fail-closed output");
    }

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    request.number = LINUX_COMPAT_SYS_MKDIRAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/repo";
    request.flags = 0755U;
    if (dispatch(&shell.linux_compat_runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected shell test setup to create Linux overlay repo");
    }
    static linux_compat_vm_t stale_vm;
    shell.linux_compat_runtime.vm = &stale_vm;
    if (!linux_compat_runtime_set_cwd(&shell.linux_compat_runtime, "/repo")) {
        return fail("expected shell test setup to simulate child Linux cwd");
    }
    if (!course_shell_run_line(&shell,
                               "cd repo && pwd",
                               out,
                               sizeof(out)) ||
        !contains(out, "/repo\n")) {
        return fail("expected shell cd to resolve from shell cwd, not stale child cwd");
    }
    shell.linux_compat_runtime.vm = 0;
    fd = open_path(&shell.linux_compat_runtime,
                   "/repo/a.out",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_TRUNC |
                       LINUX_COMPAT_O_WRONLY,
                   &trace);
    if (fd < 3 ||
        write_fd(&shell.linux_compat_runtime,
                 fd,
                 g_linux_compat_minimal_elf_asset,
                 LINUX_COMPAT_MINIMAL_ELF_ASSET_SIZE,
                 &trace) != (int64_t)LINUX_COMPAT_MINIMAL_ELF_ASSET_SIZE ||
        linux_compat_close(&shell.linux_compat_runtime, fd, &trace) != 0) {
        return fail("expected shell relative exec test setup to write a.out");
    }
    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    request.number = LINUX_COMPAT_SYS_FCHMODAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/repo/a.out";
    request.flags = 0755U;
    if (dispatch(&shell.linux_compat_runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected shell relative exec test setup to chmod a.out");
    }
    if (!course_shell_run_line(&shell, "./a.out", out, sizeof(out)) ||
        !contains(out, "path=/repo/a.out") ||
        contains(out, "errno=2")) {
        return fail("expected shell fallback to route ./a.out through cwd overlay exec");
    }

    return 0;
}

static int test_course_shell_stage11_output_buffer_contract(void) {
    if (COURSE_SHELL_COMMAND_OUTPUT_SIZE < 16384U) {
        return fail("expected Stage 11 shell command buffer to fit gcc Linux compat summaries");
    }

    return 0;
}

static int test_linux_run_uses_session_overlay_for_exec_lookup(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    linux_compat_exec_request_t exec_request;
    const char* argv[] = {"/tmp/a.out"};
    const char* relative_argv[] = {"./a.out"};
    char out[512];
    int fd = -1;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MKDIRAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/tmp";
    request.flags = 0755U;
    if (dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected session overlay /tmp setup to succeed");
    }

    fd = open_path(&runtime,
                   "/tmp/a.out",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_TRUNC |
                       LINUX_COMPAT_O_WRONLY,
                   &trace);
    if (fd < 3 ||
        write_fd(&runtime,
                 fd,
                 g_linux_compat_minimal_elf_asset,
                 LINUX_COMPAT_MINIMAL_ELF_ASSET_SIZE,
                 &trace) != (int64_t)LINUX_COMPAT_MINIMAL_ELF_ASSET_SIZE ||
        linux_compat_close(&runtime, fd, &trace) != 0) {
        return fail("expected session overlay ELF write to succeed");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_FCHMODAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/tmp/a.out";
    request.flags = 0755U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected session overlay ELF chmod to succeed");
    }

    memset(&exec_request, 0, sizeof(exec_request));
    exec_request.path = "/tmp/a.out";
    exec_request.argc = 1U;
    exec_request.argv = argv;
    exec_request.session_runtime = &runtime;
    if (linux_compat_run(&exec_request, out, sizeof(out), &trace) !=
            LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL ||
        !contains(out, "linux-compat: path=/tmp/a.out") ||
        !contains(out, "loader=static") ||
        !contains(out, "real exec context missing") ||
        contains(out, "errno=2")) {
        return fail("expected linux_compat_run to find session overlay executable");
    }

    memset(&exec_request, 0, sizeof(exec_request));
    memset(out, 0, sizeof(out));
    exec_request.path = "./a.out";
    exec_request.cwd = "/tmp";
    exec_request.argc = 1U;
    exec_request.argv = relative_argv;
    exec_request.session_runtime = &runtime;
    if (linux_compat_run(&exec_request, out, sizeof(out), &trace) !=
            LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL ||
        !contains(out, "linux-compat: path=/tmp/a.out") ||
        !contains(out, "loader=static") ||
        !contains(out, "real exec context missing") ||
        contains(out, "errno=2")) {
        return fail("expected relative linux_compat_run to resolve cwd overlay executable");
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

static int test_process_table_reparents_orphan_to_init(void) {
    linux_compat_process_table_t table;
    linux_compat_process_t* parent = 0;
    linux_compat_process_t* child = 0;
    const linux_compat_process_t* observed_child = 0;
    const uint32_t init_pid = 1U;
    uint32_t parent_pid = 0;
    uint32_t child_pid = 0;

    linux_compat_process_table_init(&table, "/");
    parent = linux_compat_process_table_spawn_helper(&table,
                                                     init_pid,
                                                     "/usr/bin/git",
                                                     "/repo",
                                                     0);
    if (parent == 0 || parent->pid <= init_pid || parent->ppid != init_pid) {
        return fail("expected process table to create child of init");
    }
    parent_pid = parent->pid;

    child = linux_compat_process_table_spawn_helper(&table,
                                                    parent_pid,
                                                    "/usr/bin/gcc",
                                                    "/repo",
                                                    0);
    if (child == 0 || child->pid <= parent_pid || child->ppid != parent_pid) {
        return fail("expected process table to create grandchild under parent");
    }
    child_pid = child->pid;

    if (!linux_compat_process_table_mark_exited(&table,
                                                parent_pid,
                                                7,
                                                init_pid)) {
        return fail("expected parent exit to be recorded in process table");
    }

    observed_child = linux_compat_process_table_find(&table, child_pid);
    if (observed_child == 0 || observed_child->ppid != init_pid) {
        return fail("expected orphaned child to be reparented to init");
    }
    parent = linux_compat_process_table_find_mut(&table, parent_pid);
    if (parent == 0 || !parent->exited || parent->exit_code != 7) {
        return fail("expected exited parent to remain observable as zombie");
    }

    return 0;
}

static int test_execve_clone_inherits_cwd_path_and_reports_cloexec(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    const linux_compat_process_t* child = 0;
    int cloexec_fd = -1;
    int stable_fd = -1;
    int64_t child_pid = 0;

    linux_compat_runtime_init(&runtime);
    if (!linux_compat_runtime_set_cwd(&runtime, "/repo")) {
        return fail("expected exec inheritance test cwd setup to succeed");
    }

    cloexec_fd = open_path(&runtime,
                           "/bin/busybox",
                           LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_CLOEXEC,
                           &trace);
    stable_fd = open_path(&runtime, "/usr/bin/git", LINUX_COMPAT_O_RDONLY, &trace);
    if (cloexec_fd < 3 || stable_fd < 3) {
        return fail("expected exec inheritance test to open rootfs files");
    }

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    request.number = LINUX_COMPAT_SYS_EXECVE;
    request.path = "/bin/busybox";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        runtime.fds[cloexec_fd].open ||
        !runtime.fds[stable_fd].open ||
        !contains(trace.message, "cloexec_closed=1")) {
        return fail("expected execve to close CLOEXEC fd and report it in trace");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_CLONE;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value <= 1) {
        return fail("expected clone after execve to create helper child");
    }
    child_pid = response.value;
    child = linux_compat_process_table_find(&runtime.process_table,
                                            (uint32_t)child_pid);
    if (child == 0 ||
        strcmp(child->cwd, "/repo") != 0 ||
        strcmp(child->path, "/bin/busybox") != 0) {
        return fail("expected clone child to inherit cwd and exec path");
    }

    return 0;
}

static int test_thread_clone_is_rejected_in_single_thread_compat(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    const uint32_t thread_clone_flags = 0x7d0f00U;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_CLONE;
    request.flags = thread_clone_flags;
        request.addr = 0x6fff0000U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != -38 ||
        runtime.process_table.clone_count != 0U ||
        runtime.process_table.last_clone_flags != thread_clone_flags ||
        runtime.process_table.last_clone_stack != request.addr ||
        !contains(trace.message, "unsupported thread") ||
        !runtime.latest_error_trace_valid ||
        runtime.latest_error_trace_record.number != LINUX_COMPAT_SYS_CLONE ||
        !contains(runtime.latest_error_trace_record.message, "flags=8195840") ||
        !contains(runtime.latest_error_trace_record.message, "stack=0x6fff0000") ||
        !contains(runtime.latest_error_trace_record.message, "ret=-38")) {
        return fail("expected thread-style clone to fail instead of creating a fake child");
    }

    return 0;
}

static int test_openat_cloexec_sets_fd_flag(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    int fd = -1;

    linux_compat_runtime_init(&runtime);

    fd = open_path(&runtime,
                   "/bin/busybox",
                   LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_CLOEXEC,
                   &trace);
    if (fd < 3 ||
        runtime.fds[fd].fd_flags != LINUX_COMPAT_FD_CLOEXEC ||
        (runtime.fds[fd].flags & LINUX_COMPAT_O_CLOEXEC) != 0U) {
        return fail("expected openat O_CLOEXEC to open readonly fd with close-on-exec");
    }

    return 0;
}

static int test_openat_largefile_is_accepted_as_low_effect_status_flag(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    int fd = -1;

    linux_compat_runtime_init(&runtime);

    fd = open_path(&runtime,
                   "/bin/busybox",
                   LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_CLOEXEC |
                       LINUX_COMPAT_O_LARGEFILE,
                   &trace);
    if (fd < 3 ||
        runtime.fds[fd].fd_flags != LINUX_COMPAT_FD_CLOEXEC ||
        (runtime.fds[fd].flags & LINUX_COMPAT_O_CLOEXEC) != 0U ||
        (runtime.fds[fd].flags & LINUX_COMPAT_O_LARGEFILE) !=
            LINUX_COMPAT_O_LARGEFILE) {
        return fail("expected openat O_LARGEFILE to be accepted without swallowing O_CLOEXEC");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/stage11repo/.gitignore";
    request.flags = LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_LARGEFILE |
                    LINUX_COMPAT_O_NOFOLLOW;
    if (dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_ERR_NO_SUCH_FILE ||
        response.value != -2 ||
        !contains(runtime.latest_trace_record.message,
                  "flags=163840")) {
        return fail("expected openat O_NOFOLLOW|O_LARGEFILE missing file to return ENOENT not EINVAL");
    }

    return 0;
}

static int test_openat_append_writes_at_end_for_git_reflog(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    char readback[8] = {0};
    int fd = -1;

    linux_compat_runtime_init(&runtime);

    fd = open_path(&runtime,
                   "/stage11repo.git.log",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_WRONLY |
                       LINUX_COMPAT_O_LARGEFILE,
                   &trace);
    if (fd < 3 ||
        write_fd(&runtime, fd, "old", 3U, &trace) != 3 ||
        linux_compat_close(&runtime, fd, &trace) != 0) {
        return fail("expected append test file setup to succeed");
    }

    fd = open_path(&runtime,
                   "/stage11repo.git.log",
                   LINUX_COMPAT_O_WRONLY | LINUX_COMPAT_O_APPEND |
                       LINUX_COMPAT_O_LARGEFILE,
                   &trace);
    if (fd < 3 ||
        (runtime.fds[fd].flags & LINUX_COMPAT_O_APPEND) == 0U ||
        linux_compat_lseek(&runtime, fd, 0, 0U, &trace) != 0 ||
        write_fd(&runtime, fd, "new", 3U, &trace) != 3 ||
        runtime.fds[fd].offset != 6U ||
        linux_compat_close(&runtime, fd, &trace) != 0) {
        return fail("expected O_APPEND write to append at overlay file end");
    }

    fd = open_path(&runtime, "/stage11repo.git.log", LINUX_COMPAT_O_RDONLY,
                   &trace);
    if (fd < 3 ||
        linux_compat_read(&runtime, fd, readback, 6U, &trace) != 6 ||
        memcmp(readback, "oldnew", 6U) != 0) {
        return fail("expected O_APPEND write to preserve existing reflog bytes");
    }

    return 0;
}

static int test_openat_directory_and_exclusive_create_flags(void) {
    const uint32_t kOExcl = 00000200U;
    const uint32_t kODirectory = 00200000U;
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    int fd = -1;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MKDIRAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/repo";
    request.flags = 0755U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected mkdirat to prepare O_DIRECTORY target");
    }

    fd = open_path(&runtime,
                   "/repo",
                   LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_CLOEXEC |
                       LINUX_COMPAT_O_LARGEFILE | kODirectory,
                   &trace);
    if (fd < 3 || !runtime.fds[fd].overlay_node ||
        (runtime.fds[fd].flags & kODirectory) != kODirectory) {
        return fail("expected O_DIRECTORY to open an existing directory");
    }
    (void)linux_compat_close(&runtime, fd, &trace);

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/missing/templates/";
    request.flags = LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_CLOEXEC |
                    LINUX_COMPAT_O_LARGEFILE | kODirectory;
    if (dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_ERR_NO_SUCH_FILE ||
        response.value != -2) {
        return fail("expected O_DIRECTORY missing path to return ENOENT not EINVAL");
    }

    fd = open_path(&runtime,
                   "/bin/busybox",
                   LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_CREAT | kOExcl,
                   &trace);
    if (fd != -17) {
        return fail("expected O_CREAT|O_EXCL on an existing lower file to fail with EEXIST");
    }

    fd = open_path(&runtime,
                   "/repo/config.lock",
                   LINUX_COMPAT_O_RDWR | LINUX_COMPAT_O_CREAT | kOExcl |
                       LINUX_COMPAT_O_LARGEFILE,
                   &trace);
    if (fd < 3 || !runtime.fds[fd].overlay_node) {
        return fail("expected O_CREAT|O_EXCL to create a missing overlay file");
    }
    (void)linux_compat_close(&runtime, fd, &trace);

    fd = open_path(&runtime,
                   "/repo/config.lock",
                   LINUX_COMPAT_O_RDWR | LINUX_COMPAT_O_CREAT | kOExcl |
                       LINUX_COMPAT_O_LARGEFILE,
                   &trace);
    if (fd != -17) {
        return fail("expected O_CREAT|O_EXCL on an existing overlay file to fail with EEXIST");
    }

    return 0;
}

static int test_dev_null_open_read_write_and_fstat(void) {
    enum {
        kLinuxStatModeOffset = 16,
        kLinuxStatSizeOffset = 48,
        kLinuxStatSize = 128
    };
    const uint32_t kSIfChr = 0020000U;
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    uint8_t stat_buffer[kLinuxStatSize];
    char readback[4] = {0};
    uint8_t random_bytes[8] = {0};
    int fd = -1;

    linux_compat_runtime_init(&runtime);

    fd = open_path(&runtime,
                   "/dev/null",
                   LINUX_COMPAT_O_RDWR | LINUX_COMPAT_O_LARGEFILE,
                   &trace);
    if (fd < 3) {
        return fail("expected /dev/null to open read-write");
    }

    if (write_fd(&runtime, fd, "drop", 4U, &trace) != 4) {
        return fail("expected /dev/null write to discard bytes");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_READ;
    request.fd = fd;
    request.read_buffer = readback;
    request.length = sizeof(readback);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        memcmp(readback, "\0\0\0\0", sizeof(readback)) != 0) {
        return fail("expected /dev/null read to return eof");
    }

    memset(&request, 0, sizeof(request));
    memset(stat_buffer, 0, sizeof(stat_buffer));
    request.number = LINUX_COMPAT_SYS_FSTAT;
    request.fd = fd;
    request.stat = (linux_compat_stat_t*)stat_buffer;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        read_u64_le(stat_buffer, kLinuxStatSizeOffset) != 0U ||
        (read_u32_le(stat_buffer, kLinuxStatModeOffset) & kSIfChr) == 0U) {
        return fail("expected /dev/null fstat to report char device metadata");
    }

    fd = open_path(&runtime,
                   "/dev/urandom",
                   LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_LARGEFILE,
                   &trace);
    if (fd < 3) {
        return fail("expected /dev/urandom to open read-only");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_READ;
    request.fd = fd;
    request.read_buffer = random_bytes;
    request.length = sizeof(random_bytes);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != (int64_t)sizeof(random_bytes) ||
        memcmp(random_bytes, "\0\0\0\0\0\0\0\0", sizeof(random_bytes)) == 0) {
        return fail("expected /dev/urandom read to fill deterministic bytes");
    }

    memset(&request, 0, sizeof(request));
    memset(stat_buffer, 0, sizeof(stat_buffer));
    request.number = LINUX_COMPAT_SYS_FSTAT;
    request.fd = fd;
    request.stat = (linux_compat_stat_t*)stat_buffer;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        read_u64_le(stat_buffer, kLinuxStatSizeOffset) != 0U ||
        (read_u32_le(stat_buffer, kLinuxStatModeOffset) & kSIfChr) == 0U) {
        return fail("expected /dev/urandom fstat to report char device metadata");
    }

    return 0;
}

static int test_pseudo_paths_are_explicit_and_fail_closed(void) {
    enum {
        kLinuxStatModeOffset = 16,
        kLinuxStatSizeOffset = 48,
        kLinuxStatSize = 128
    };
    const char* proc_self_exe = "linux-compat:/proc/self/exe";
    const uint32_t kSIfChr = 0020000U;
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    linux_compat_stat_t stat;
    uint8_t stat_buffer[kLinuxStatSize];
    char link_target[64];
    uint8_t random_bytes[4] = {0};
    int fd = -1;

    linux_compat_runtime_init(&runtime);

    fd = open_path(&runtime,
                   "/dev/null",
                   LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_DIRECTORY,
                   &trace);
    if (fd != -20) {
        return fail("expected /dev/null O_DIRECTORY to fail as not-directory");
    }

    fd = open_path(&runtime, "/dev/random", LINUX_COMPAT_O_WRONLY, &trace);
    if (fd != -13) {
        return fail("expected /dev/random writable open to fail with EACCES");
    }

    fd = open_path(&runtime,
                   "/dev/random",
                   LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_LARGEFILE,
                   &trace);
    if (fd < 3) {
        return fail("expected /dev/random to open read-only");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_READ;
    request.fd = fd;
    request.read_buffer = random_bytes;
    request.length = sizeof(random_bytes);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != (int64_t)sizeof(random_bytes) ||
        memcmp(random_bytes, "\0\0\0\0", sizeof(random_bytes)) == 0) {
        return fail("expected /dev/random read to fill deterministic bytes");
    }
    (void)linux_compat_close(&runtime, fd, &trace);

    memset(&request, 0, sizeof(request));
    memset(link_target, 0, sizeof(link_target));
    request.number = LINUX_COMPAT_SYS_READLINKAT;
    request.path = "/proc/self/exe";
    request.read_buffer = link_target;
    request.length = sizeof(link_target);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != (int64_t)strlen(proc_self_exe) ||
        strcmp(link_target, proc_self_exe) != 0) {
        return fail("expected /proc/self/exe readlinkat to expose course evidence path");
    }

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/proc/self/exe";
    request.flags = LINUX_COMPAT_O_RDONLY;
    if (dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_ERR_NO_SUCH_FILE ||
        response.value != -2) {
        return fail("expected /proc/self/exe openat to remain fail-closed");
    }

    memset(&request, 0, sizeof(request));
    memset(link_target, 0, sizeof(link_target));
    request.number = LINUX_COMPAT_SYS_READLINKAT;
    request.path = "/proc/self/fd/0";
    request.read_buffer = link_target;
    request.length = sizeof(link_target);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != -2) {
        return fail("expected unsupported /proc paths to fail closed");
    }

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/tmp";
    request.flags = LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_DIRECTORY;
    if (dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_ERR_NO_SUCH_FILE ||
        response.value != -2) {
        return fail("expected /tmp to remain overlay-created, not builtin");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MKDIRAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/tmp";
    request.flags = 0777U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        stat_path(&runtime, "/tmp", &stat, &trace) != 0 ||
        !stat.directory ||
        stat.nlink != 1U) {
        return fail("expected /tmp to work as an explicit overlay directory");
    }

    fd = open_path(&runtime,
                   "/tmp",
                   LINUX_COMPAT_O_RDONLY | LINUX_COMPAT_O_DIRECTORY,
                   &trace);
    if (fd < 3) {
        return fail("expected overlay /tmp to open with O_DIRECTORY after mkdir");
    }

    memset(&request, 0, sizeof(request));
    memset(stat_buffer, 0, sizeof(stat_buffer));
    request.number = LINUX_COMPAT_SYS_FSTAT;
    request.fd = fd;
    request.stat = (linux_compat_stat_t*)stat_buffer;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        read_u64_le(stat_buffer, kLinuxStatSizeOffset) != 0U ||
        (read_u32_le(stat_buffer, kLinuxStatModeOffset) & kSIfChr) != 0U) {
        return fail("expected overlay /tmp fstat to report directory metadata");
    }
    (void)linux_compat_close(&runtime, fd, &trace);

    return 0;
}

static int test_stdin_read_consumes_uart_queue_raw_bytes(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    uint8_t readback[8] = {0};
    const uint8_t input[] = {'i', 'h', 'e', 'l', 'l', 'o', 0x1b, '\r'};

    linux_compat_runtime_init(&runtime);
    set_uart_input(input, sizeof(input));

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_READ;
    request.fd = 0;
    request.read_buffer = readback;
    request.length = sizeof(readback);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != (int64_t)sizeof(input) ||
        memcmp(readback, input, sizeof(input)) != 0) {
        return fail("expected stdin read to consume raw UART bytes");
    }

    return 0;
}

static int test_stdin_read_consumes_piped_text_before_uart(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    char readback[8] = {0};
    const char input[] = "abc";

    linux_compat_runtime_init(&runtime);
    runtime.stdin_text = input;
    runtime.stdin_size = 3U;
    runtime.stdin_offset = 0U;

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_READ;
    request.fd = 0;
    request.read_buffer = readback;
    request.length = sizeof(readback);
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 3 ||
        memcmp(readback, "abc", 3U) != 0 ||
        runtime.stdin_offset != 3U) {
        return fail("expected stdin read to consume piped shell text");
    }

    memset(readback, 0, sizeof(readback));
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected exhausted piped stdin text to report EOF");
    }

    return 0;
}

static int test_pselect6_reports_stdin_ready_from_uart_queue(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    uint64_t readfds = 1U;
    uint64_t writefds = 0U;
    uint64_t exceptfds = 0U;
    const uint8_t input[] = {'\r'};

    linux_compat_runtime_init(&runtime);
    set_uart_input(input, sizeof(input));

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_PSELECT6;
    request.length = 1U;
    request.read_buffer = &readfds;
    request.write_buffer = &writefds;
    request.stat = (linux_compat_stat_t*)&exceptfds;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 1 ||
        readfds != 1U ||
        writefds != 0U ||
        exceptfds != 0U ||
        !contains(trace.message, "pselect6")) {
        return fail("expected pselect6 to report queued stdin input as readable");
    }

    return 0;
}

static int test_futex_wait_and_wake_are_low_effect_not_unsupported(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    uint32_t futex_word = 1U;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_FUTEX;
    request.addr = (uint64_t)(uintptr_t)&futex_word;
    request.command = LINUX_COMPAT_FUTEX_WAIT |
                      LINUX_COMPAT_FUTEX_PRIVATE_FLAG;
    request.arg = 2U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != -11 ||
        !contains(trace.message, "futex") ||
        !contains(runtime.latest_trace_record.message, "addr=") ||
        !contains(runtime.latest_trace_record.message, "op=128") ||
        !contains(runtime.latest_trace_record.message, "val=2") ||
        !contains(runtime.latest_trace_record.message, "waiters=0") ||
        !contains(runtime.latest_trace_record.message, "wait_count=1") ||
        !contains(runtime.latest_trace_record.message, "wake_count=0")) {
        return fail("expected FUTEX_WAIT with mismatched value to return EAGAIN");
    }

    request.arg = 1U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != -11 ||
        !contains(trace.message, "futex") ||
        !contains(runtime.latest_trace_record.message, "addr=") ||
        !contains(runtime.latest_trace_record.message, "op=128") ||
        !contains(runtime.latest_trace_record.message, "val=1") ||
        !contains(runtime.latest_trace_record.message, "waiters=0") ||
        !contains(runtime.latest_trace_record.message, "wait_count=2") ||
        !contains(runtime.latest_trace_record.message, "wake_count=0")) {
        return fail("expected matching FUTEX_WAIT to avoid blocking in single-thread compat");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_FUTEX;
    request.addr = (uint64_t)(uintptr_t)&futex_word;
    request.command = LINUX_COMPAT_FUTEX_WAKE |
                      LINUX_COMPAT_FUTEX_PRIVATE_FLAG;
    request.arg = 1U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        !contains(trace.message, "futex") ||
        !contains(runtime.latest_trace_record.message, "addr=") ||
        !contains(runtime.latest_trace_record.message, "op=129") ||
        !contains(runtime.latest_trace_record.message, "val=1") ||
        !contains(runtime.latest_trace_record.message, "waiters=0") ||
        !contains(runtime.latest_trace_record.message, "wait_count=2") ||
        !contains(runtime.latest_trace_record.message, "wake_count=1")) {
        return fail("expected FUTEX_WAKE to report low-effect waiter diagnostics");
    }

    return 0;
}

static int test_syscall_trace_records_include_stage11_diagnostics(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    const linux_compat_syscall_trace_record_t* record = 0;
    int fd = -1;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MKDIRAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/repo";
    request.flags = 0755U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected mkdirat setup for trace diagnostics to succeed");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/repo/config.lock";
    request.flags = LINUX_COMPAT_O_RDWR | LINUX_COMPAT_O_CREAT |
                    LINUX_COMPAT_O_EXCL | LINUX_COMPAT_O_CLOEXEC;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value < 3) {
        return fail("expected openat setup for trace diagnostics to succeed");
    }
    fd = (int)response.value;
    if (!runtime.latest_trace_valid) {
        return fail("expected truncated trace to expose latest trace record");
    }
    record = &runtime.latest_trace_record;
    if (record->number != LINUX_COMPAT_SYS_OPENAT ||
        !contains(record->message, "path=/repo/config.lock") ||
        !contains(record->message, "dirfd=-100") ||
        !contains(record->message, "fd=") ||
        !contains(record->message, "flags=") ||
        !contains(record->message, "cloexec=1")) {
        return fail("expected openat trace record to include path fd flags and cloexec diagnostics");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_RENAMEAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/repo/config.lock";
    request.new_path = "/repo/config";
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0) {
        return fail("expected renameat setup for trace diagnostics to succeed");
    }
    record = &runtime.latest_trace_record;
    if (record->number != LINUX_COMPAT_SYS_RENAMEAT ||
        !contains(record->message, "path=/repo/config.lock") ||
        !contains(record->message, "new=/repo/config")) {
        return fail("expected renameat trace record to include safe copied new path");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_WRITE;
    request.fd = fd;
    request.write_buffer = "abc";
    request.length = 3U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 3) {
        return fail("expected write setup for trace diagnostics to succeed");
    }
    if (!runtime.latest_trace_valid) {
        return fail("expected truncated trace to expose latest trace record");
    }
    record = &runtime.latest_trace_record;
    if (record->number != LINUX_COMPAT_SYS_WRITE ||
        !contains(record->message, "fd=") ||
        !contains(record->message, "count=3") ||
        !contains(record->message, "offset=0") ||
        !contains(record->message, "ret=3") ||
        !contains(record->message, "errno=0")) {
        return fail("expected write trace record to include fd count offset ret errno diagnostics");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MMAP;
    request.addr = 0x500000U;
    request.length = 8192U;
    request.prot = LINUX_COMPAT_PROT_READ | LINUX_COMPAT_PROT_WRITE;
    request.flags = 0x22U;
    request.fd = -1;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value <= 0) {
        return fail("expected mmap setup for trace diagnostics to succeed");
    }
    record = &runtime.trace_records[runtime.trace_count - 1U];
    if (record->number != LINUX_COMPAT_SYS_MMAP ||
        !contains(record->message, "addr=0x500000") ||
        !contains(record->message, "len=8192") ||
        !contains(record->message, "prot=3") ||
        !contains(record->message, "flags=34") ||
        !contains(record->message, "fd=-1")) {
        return fail("expected mmap trace record to include addr len prot flags fd diagnostics");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_CLONE;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value <= 1) {
        return fail("expected clone setup for trace diagnostics to succeed");
    }
    record = &runtime.trace_records[runtime.trace_count - 1U];
    if (record->number != LINUX_COMPAT_SYS_CLONE ||
        !contains(record->message, "pid=1") ||
        !contains(record->message, "flags=") ||
        !contains(record->message, "stack=0x") ||
        !contains(record->message, "child=") ||
        !contains(record->message, "ret=")) {
        return fail("expected clone trace record to include flags stack child and ret diagnostics");
    }

    return 0;
}

static int test_syscall_stub_policy_records_bypass_errno_and_unsupported(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    linux_compat_statx_t statx;
    const linux_compat_syscall_trace_record_t* record = 0;
    const uint64_t kUnknownSyscall = 9999U;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_SET_ROBUST_LIST;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 0 ||
        !runtime.latest_trace_valid) {
        return fail("expected set_robust_list bypass policy call to succeed");
    }
    record = &runtime.latest_trace_record;
    if (record->number != LINUX_COMPAT_SYS_SET_ROBUST_LIST ||
        record->return_value != 0 ||
        record->errno_value != 0 ||
        !contains(record->message, "set_robust_list") ||
        !contains(record->message, "policy=bypass") ||
        !contains(record->message, "ret=0") ||
        !contains(record->message, "errno=0")) {
        return fail("expected bypass syscall policy trace to be explicit");
    }

    memset(&request, 0, sizeof(request));
    memset(&statx, 0, sizeof(statx));
    request.number = LINUX_COMPAT_SYS_STATX;
    request.path = "/missing-statx-policy";
    request.statx = &statx;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != -2 ||
        !runtime.latest_error_trace_valid) {
        return fail("expected statx missing path to return stable errno");
    }
    record = &runtime.latest_error_trace_record;
    if (record->number != LINUX_COMPAT_SYS_STATX ||
        record->return_value != -2 ||
        record->errno_value != 2 ||
        !contains(record->message, "statx") ||
        !contains(record->message, "path=/missing-statx-policy") ||
        !contains(record->message, "policy=errno") ||
        !contains(record->message, "ret=-2") ||
        !contains(record->message, "errno=2")) {
        return fail("expected explicit errno syscall policy trace to be stable");
    }

    memset(&request, 0, sizeof(request));
    request.number = kUnknownSyscall;
    if (dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL ||
        response.value != -38 ||
        !runtime.latest_error_trace_valid) {
        return fail("expected unknown syscall to use unsupported policy");
    }
    record = &runtime.latest_error_trace_record;
    if (record->number != kUnknownSyscall ||
        record->return_value != -38 ||
        record->errno_value != 38 ||
        !contains(record->message, "unsupported syscall") ||
        !contains(record->message, "policy=unsupported") ||
        !contains(record->message, "ret=-38") ||
        !contains(record->message, "errno=38")) {
        return fail("expected unsupported syscall policy trace to fail closed");
    }

    return 0;
}

static int test_lseek_syscall_dispatch_uses_whence_argument(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    int fd = -1;

    linux_compat_runtime_init(&runtime);
    fd = open_path(&runtime,
                   "/stage11-lseek.txt",
                   LINUX_COMPAT_O_CREAT | LINUX_COMPAT_O_TRUNC |
                       LINUX_COMPAT_O_RDWR,
                   &trace);
    if (fd < 3 ||
        write_fd(&runtime, fd, "0123456789", 10U, &trace) != 10) {
        return fail("expected lseek test file setup to succeed");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_LSEEK;
    request.fd = fd;
    request.offset = (uint64_t)-3;
    request.command = 2U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 7 ||
        runtime.fds[fd].offset != 7U) {
        return fail("expected lseek syscall dispatch to honor SEEK_END whence");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_LSEEK;
    request.fd = fd;
    request.offset = 2U;
    request.command = 1U;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value != 9 ||
        runtime.fds[fd].offset != 9U) {
        return fail("expected lseek syscall dispatch to honor SEEK_CUR whence");
    }

    return 0;
}

static int test_syscall_trace_keeps_latest_record_after_truncation(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    size_t i = 0;
    const size_t call_count = LINUX_COMPAT_MAX_TRACE_RECORDS + 3U;
    uintptr_t first_recent_pc = 0;
    uintptr_t final_pc = 0;
    const linux_compat_syscall_trace_record_t* record = NULL;

    linux_compat_runtime_init(&runtime);
    for (i = 0; i < call_count; ++i) {
        memset(&request, 0, sizeof(request));
        request.number = LINUX_COMPAT_SYS_BRK;
        request.addr = runtime.program_break + 4096U;
        if (i + LINUX_COMPAT_MAX_TRACE_RECORDS == call_count) {
            first_recent_pc = (uintptr_t)request.addr;
        }
        final_pc = (uintptr_t)request.addr;
        if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK) {
            return fail("expected repeated brk trace setup to succeed");
        }
    }

    if (!runtime.trace_truncated ||
        runtime.trace_count != LINUX_COMPAT_MAX_TRACE_RECORDS) {
        return fail("expected repeated syscalls to mark trace as truncated at capacity");
    }
    if (!runtime.latest_trace_valid) {
        return fail("expected truncated trace to expose latest trace record");
    }
    record = &runtime.latest_trace_record;
    if (record->number != LINUX_COMPAT_SYS_BRK ||
        record->return_value != response.value ||
        record->pc != request.addr ||
        !contains(record->message, "addr=")) {
        return fail("expected truncated trace to retain latest syscall diagnostics");
    }
    if (runtime.trace_records[0].pc != first_recent_pc ||
        runtime.trace_records[LINUX_COMPAT_MAX_TRACE_RECORDS - 1U].pc !=
            final_pc) {
        return fail("expected truncated trace ring to retain the most recent records");
    }

    return 0;
}

static int test_syscall_trace_keeps_latest_error_after_success(void) {
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    const linux_compat_syscall_trace_record_t* record = NULL;

    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/missing";
    if (dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_ERR_NO_SUCH_FILE ||
        response.value != -2) {
        return fail("expected missing openat setup for latest-error trace");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_GETPID;
    if (dispatch(&runtime, &request, &response, &trace) != LINUX_COMPAT_OK ||
        response.value <= 0) {
        return fail("expected successful getpid after latest-error trace setup");
    }

    if (!runtime.latest_error_trace_valid) {
        return fail("expected trace to retain latest negative-return syscall");
    }
    record = &runtime.latest_error_trace_record;
    if (record->number != LINUX_COMPAT_SYS_OPENAT ||
        record->return_value != -2 ||
        record->errno_value != 2 ||
        !contains(record->message, "path=/missing")) {
        return fail("expected latest error trace to retain missing openat diagnostics");
    }

    return 0;
}

int main(void) {
    if (test_create_write_lseek_readback_and_stat() != 0 ||
        test_fstat_and_newfstatat_write_linux_abi_stat_layout() != 0 ||
        test_overlay_shadows_lower_rootfs() != 0 ||
        test_mkdir_dirents_rename_unlink_and_sync() != 0 ||
        test_opened_overlay_fd_survives_rename_unlink_until_close() != 0 ||
        test_getdents64_advances_directory_offset_to_eof() != 0 ||
        test_git_object_rename_supports_long_overlay_path() != 0 ||
        test_bad_path_bad_fd_and_lower_guardrails() != 0 ||
        test_cwd_relative_paths_and_dot_slash() != 0 ||
        test_getcwd_and_chdir_update_runtime_cwd() != 0 ||
        test_getpid_and_fchmodat_for_overlay_file() != 0 ||
        test_course_shell_minimal_and_chain() != 0 ||
        test_course_shell_stage11_output_buffer_contract() != 0 ||
        test_linux_run_uses_session_overlay_for_exec_lookup() != 0 ||
        test_process_exec_wait_and_pipe_syscalls() != 0 ||
        test_process_table_reparents_orphan_to_init() != 0 ||
        test_execve_clone_inherits_cwd_path_and_reports_cloexec() != 0 ||
        test_thread_clone_is_rejected_in_single_thread_compat() != 0 ||
        test_openat_cloexec_sets_fd_flag() != 0 ||
        test_openat_largefile_is_accepted_as_low_effect_status_flag() != 0 ||
        test_openat_append_writes_at_end_for_git_reflog() != 0 ||
        test_openat_directory_and_exclusive_create_flags() != 0 ||
        test_dev_null_open_read_write_and_fstat() != 0 ||
        test_pseudo_paths_are_explicit_and_fail_closed() != 0 ||
        test_stdin_read_consumes_uart_queue_raw_bytes() != 0 ||
        test_stdin_read_consumes_piped_text_before_uart() != 0 ||
        test_pselect6_reports_stdin_ready_from_uart_queue() != 0 ||
        test_futex_wait_and_wake_are_low_effect_not_unsupported() != 0 ||
        test_syscall_trace_records_include_stage11_diagnostics() != 0 ||
        test_syscall_stub_policy_records_bypass_errno_and_unsupported() != 0 ||
        test_lseek_syscall_dispatch_uses_whence_argument() != 0 ||
        test_syscall_trace_keeps_latest_record_after_truncation() != 0 ||
        test_syscall_trace_keeps_latest_error_after_success() != 0) {
        return 1;
    }
    return 0;
}
