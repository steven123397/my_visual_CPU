#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINUX_COMPAT_MAX_PATH 64U
#define LINUX_COMPAT_MAX_MESSAGE 128U
#define LINUX_COMPAT_MAX_ARGS 8U
#define LINUX_COMPAT_MAX_FDS 8U
#define LINUX_COMPAT_MAX_STDOUT 512U
#define LINUX_COMPAT_MAX_DIRENTS 8U
#define LINUX_COMPAT_MAX_TRACE_RECORDS 16U
#define LINUX_COMPAT_AT_FDCWD (-100)
#define LINUX_COMPAT_O_RDONLY 0U
#define LINUX_COMPAT_DT_DIR 4U
#define LINUX_COMPAT_DT_REG 8U
#define LINUX_COMPAT_S_IFDIR 0040000U
#define LINUX_COMPAT_S_IFREG 0100000U
#define LINUX_COMPAT_S_IRUSR 0400U
#define LINUX_COMPAT_S_IWUSR 0200U
#define LINUX_COMPAT_S_IXUSR 0100U
#define LINUX_COMPAT_S_IRGRP 0040U
#define LINUX_COMPAT_S_IXGRP 0010U
#define LINUX_COMPAT_S_IROTH 0004U
#define LINUX_COMPAT_S_IXOTH 0001U

#define LINUX_COMPAT_SYS_OPENAT 56U
#define LINUX_COMPAT_SYS_CLOSE 57U
#define LINUX_COMPAT_SYS_GETDENTS64 61U
#define LINUX_COMPAT_SYS_LSEEK 62U
#define LINUX_COMPAT_SYS_READ 63U
#define LINUX_COMPAT_SYS_WRITE 64U
#define LINUX_COMPAT_SYS_NEWFSTATAT 79U
#define LINUX_COMPAT_SYS_CLOCK_GETTIME 113U
#define LINUX_COMPAT_SYS_EXIT 93U
#define LINUX_COMPAT_SYS_EXIT_GROUP 94U
#define LINUX_COMPAT_SYS_BRK 214U
#define LINUX_COMPAT_SYS_MUNMAP 215U
#define LINUX_COMPAT_SYS_MMAP 222U

typedef enum LinuxCompatResult {
    LINUX_COMPAT_OK = 0,
    LINUX_COMPAT_ERR_NO_SUCH_FILE = -2,
    LINUX_COMPAT_ERR_BAD_ELF = -8,
    LINUX_COMPAT_ERR_UNSUPPORTED_ELF = -9,
    LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL = -38,
} linux_compat_result_t;

typedef struct LinuxCompatExecRequest {
    const char* path;
    size_t argc;
    const char* const* argv;
} linux_compat_exec_request_t;

typedef struct LinuxCompatTrace {
    char path[LINUX_COMPAT_MAX_PATH];
    int32_t errno_value;
    uint64_t syscall_number;
    uintptr_t pc;
    char message[LINUX_COMPAT_MAX_MESSAGE];
} linux_compat_trace_t;

typedef struct LinuxCompatRootfsEntry {
    const char* path;
    const uint8_t* data;
    size_t size;
    bool executable;
} linux_compat_rootfs_entry_t;

typedef struct LinuxCompatElfInfo {
    uint8_t elf_class;
    uint8_t endianness;
    uint16_t type;
    uint16_t machine;
    uint64_t entry;
    uint64_t phoff;
    uint16_t phentsize;
    uint16_t phnum;
    bool has_interp;
} linux_compat_elf_info_t;

typedef struct LinuxCompatStat {
    uint64_t inode;
    uint32_t mode;
    uint64_t size;
    bool directory;
    bool executable;
} linux_compat_stat_t;

typedef struct LinuxCompatDirent {
    uint64_t inode;
    uint8_t type;
    char name[LINUX_COMPAT_MAX_PATH];
} linux_compat_dirent_t;

typedef struct LinuxCompatFd {
    bool open;
    const void* node;
    size_t offset;
} linux_compat_fd_t;

typedef struct LinuxCompatSyscallTraceRecord {
    uint64_t number;
    int64_t return_value;
    int32_t errno_value;
    uintptr_t pc;
    char message[LINUX_COMPAT_MAX_MESSAGE];
} linux_compat_syscall_trace_record_t;

typedef struct LinuxCompatRuntime {
    linux_compat_fd_t fds[LINUX_COMPAT_MAX_FDS];
    uint64_t program_break;
    uint64_t next_mmap;
    char stdout_buffer[LINUX_COMPAT_MAX_STDOUT];
    size_t stdout_size;
    bool exited;
    int32_t exit_code;
    linux_compat_syscall_trace_record_t trace_records[LINUX_COMPAT_MAX_TRACE_RECORDS];
    size_t trace_count;
    bool trace_truncated;
} linux_compat_runtime_t;

typedef struct LinuxCompatSyscallRequest {
    uint64_t number;
    int32_t dirfd;
    int32_t fd;
    const char* path;
    const void* write_buffer;
    void* read_buffer;
    size_t length;
    uint64_t offset;
    linux_compat_stat_t* stat;
    linux_compat_dirent_t* dirents;
    size_t dirent_capacity;
    uint64_t addr;
} linux_compat_syscall_request_t;

typedef struct LinuxCompatSyscallResponse {
    int64_t value;
} linux_compat_syscall_response_t;

linux_compat_result_t linux_compat_lookup(
    const char* path,
    linux_compat_rootfs_entry_t* out_entry,
    linux_compat_trace_t* out_trace);

linux_compat_result_t linux_compat_inspect_elf(
    const uint8_t* image,
    size_t image_size,
    linux_compat_elf_info_t* out_info,
    linux_compat_trace_t* out_trace);

linux_compat_result_t linux_compat_run(
    const linux_compat_exec_request_t* request,
    char* out,
    size_t out_size,
    linux_compat_trace_t* out_trace);

linux_compat_result_t linux_compat_stat_path(
    const char* path,
    linux_compat_stat_t* out_stat,
    linux_compat_trace_t* out_trace);

void linux_compat_runtime_init(linux_compat_runtime_t* runtime);

int32_t linux_compat_openat(linux_compat_runtime_t* runtime,
                            int32_t dirfd,
                            const char* path,
                            uint32_t flags,
                            linux_compat_trace_t* out_trace);

int64_t linux_compat_read(linux_compat_runtime_t* runtime,
                          int32_t fd,
                          void* buffer,
                          size_t length,
                          linux_compat_trace_t* out_trace);

int64_t linux_compat_lseek(linux_compat_runtime_t* runtime,
                           int32_t fd,
                           int64_t offset,
                           uint32_t whence,
                           linux_compat_trace_t* out_trace);

int32_t linux_compat_close(linux_compat_runtime_t* runtime,
                           int32_t fd,
                           linux_compat_trace_t* out_trace);

linux_compat_result_t linux_compat_syscall_dispatch(
    linux_compat_runtime_t* runtime,
    const linux_compat_syscall_request_t* request,
    linux_compat_syscall_response_t* response,
    linux_compat_trace_t* out_trace);
