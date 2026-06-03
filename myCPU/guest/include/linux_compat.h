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
#define LINUX_COMPAT_MAX_OVERLAY_NODES 32U
#define LINUX_COMPAT_MAX_OVERLAY_FILE_SIZE 2048U
#define LINUX_COMPAT_MINIMAL_ELF_PATH "/bin/minimal-elf"
#define LINUX_COMPAT_AT_FDCWD (-100)
#define LINUX_COMPAT_O_RDONLY 0U
#define LINUX_COMPAT_O_WRONLY 00000001U
#define LINUX_COMPAT_O_RDWR 00000002U
#define LINUX_COMPAT_O_ACCMODE 00000003U
#define LINUX_COMPAT_O_CREAT 00000100U
#define LINUX_COMPAT_O_TRUNC 00001000U
#define LINUX_COMPAT_DT_DIR 4U
#define LINUX_COMPAT_DT_REG 8U
#ifndef LINUX_COMPAT_PROT_READ
#define LINUX_COMPAT_PROT_READ 0x1U
#define LINUX_COMPAT_PROT_WRITE 0x2U
#define LINUX_COMPAT_PROT_EXEC 0x4U
#endif
#define LINUX_COMPAT_S_IFDIR 0040000U
#define LINUX_COMPAT_S_IFREG 0100000U
#define LINUX_COMPAT_S_IRUSR 0400U
#define LINUX_COMPAT_S_IWUSR 0200U
#define LINUX_COMPAT_S_IXUSR 0100U
#define LINUX_COMPAT_S_IRGRP 0040U
#define LINUX_COMPAT_S_IXGRP 0010U
#define LINUX_COMPAT_S_IROTH 0004U
#define LINUX_COMPAT_S_IXOTH 0001U

#define LINUX_COMPAT_O_NONBLOCK 00004000U
#define LINUX_COMPAT_F_DUPFD 0U
#define LINUX_COMPAT_F_GETFD 1U
#define LINUX_COMPAT_F_SETFD 2U
#define LINUX_COMPAT_F_GETFL 3U
#define LINUX_COMPAT_F_SETFL 4U
#define LINUX_COMPAT_FD_CLOEXEC 1U
#define LINUX_COMPAT_TIOCGWINSZ 0x5413U
#define LINUX_COMPAT_TCGETS 0x5401U
#define LINUX_COMPAT_TCSETS 0x5402U
#define LINUX_COMPAT_TCSETSW 0x5403U
#define LINUX_COMPAT_TCSETSF 0x5404U
#define LINUX_COMPAT_FIONBIO 0x5421U
#define LINUX_COMPAT_CLOCK_REALTIME 0U
#define LINUX_COMPAT_CLOCK_MONOTONIC 1U

#define LINUX_COMPAT_SYS_FCNTL 25U
#define LINUX_COMPAT_SYS_IOCTL 29U
#define LINUX_COMPAT_SYS_MKDIRAT 34U
#define LINUX_COMPAT_SYS_UNLINKAT 35U
#define LINUX_COMPAT_SYS_RENAMEAT 38U
#define LINUX_COMPAT_SYS_FTRUNCATE 46U
#define LINUX_COMPAT_SYS_FACCESSAT 48U
#define LINUX_COMPAT_SYS_OPENAT 56U
#define LINUX_COMPAT_SYS_CLOSE 57U
#define LINUX_COMPAT_SYS_GETDENTS64 61U
#define LINUX_COMPAT_SYS_LSEEK 62U
#define LINUX_COMPAT_SYS_READ 63U
#define LINUX_COMPAT_SYS_WRITE 64U
#define LINUX_COMPAT_SYS_WRITEV 66U
#define LINUX_COMPAT_SYS_PREAD64 67U
#define LINUX_COMPAT_SYS_PWRITE64 68U
#define LINUX_COMPAT_SYS_READLINKAT 78U
#define LINUX_COMPAT_SYS_NEWFSTATAT 79U
#define LINUX_COMPAT_SYS_FSTAT 80U
#define LINUX_COMPAT_SYS_SYNC 81U
#define LINUX_COMPAT_SYS_FSYNC 82U
#define LINUX_COMPAT_SYS_FDATASYNC 83U
#define LINUX_COMPAT_SYS_CLOCK_GETTIME 113U
#define LINUX_COMPAT_SYS_EXIT 93U
#define LINUX_COMPAT_SYS_EXIT_GROUP 94U
#define LINUX_COMPAT_SYS_SET_TID_ADDRESS 96U
#define LINUX_COMPAT_SYS_SET_ROBUST_LIST 99U
#define LINUX_COMPAT_SYS_RT_SIGACTION 134U
#define LINUX_COMPAT_SYS_RT_SIGPROCMASK 135U
#define LINUX_COMPAT_SYS_UNAME 160U
#define LINUX_COMPAT_SYS_BRK 214U
#define LINUX_COMPAT_SYS_MUNMAP 215U
#define LINUX_COMPAT_SYS_MMAP 222U
#define LINUX_COMPAT_SYS_MPROTECT 226U
#define LINUX_COMPAT_SYS_PRLIMIT64 261U
#define LINUX_COMPAT_SYS_RENAMEAT2 276U
#define LINUX_COMPAT_SYS_GETRANDOM 278U
#define LINUX_COMPAT_SYS_STATX 291U

struct LinuxCompatVm;
typedef struct TrapContext trap_context_t;
typedef struct TrapUserRuntime trap_user_runtime_t;
typedef struct VmAddressSpace vm_address_space_t;
typedef struct VmProcess vm_process_t;

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
    trap_context_t* trap_context;
    trap_user_runtime_t* user_runtime;
    vm_address_space_t* address_space;
    vm_process_t* process;
    void* trap_stack_base;
    size_t trap_stack_size;
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

typedef struct LinuxCompatIovec {
    const void* base;
    size_t length;
} linux_compat_iovec_t;

typedef struct LinuxCompatUtsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
} linux_compat_utsname_t;

typedef struct LinuxCompatRlimit {
    uint64_t current;
    uint64_t maximum;
} linux_compat_rlimit_t;

typedef struct LinuxCompatStatx {
    uint32_t mask;
    uint32_t blksize;
    uint64_t inode;
    uint64_t size;
    uint32_t mode;
} linux_compat_statx_t;

typedef struct LinuxCompatOverlayNode {
    bool used;
    bool directory;
    bool executable;
    bool dirty;
    uint64_t inode;
    uint32_t mode;
    uint64_t mtime;
    char path[LINUX_COMPAT_MAX_PATH];
    uint8_t data[LINUX_COMPAT_MAX_OVERLAY_FILE_SIZE];
    size_t size;
} linux_compat_overlay_node_t;

typedef struct LinuxCompatFd {
    bool open;
    const void* node;
    size_t offset;
    uint32_t flags;
    uint32_t fd_flags;
    bool overlay_node;
} linux_compat_fd_t;

typedef struct LinuxCompatTimespec {
    int64_t tv_sec;
    int64_t tv_nsec;
} linux_compat_timespec_t;

typedef struct LinuxCompatWinsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
} linux_compat_winsize_t;

typedef struct LinuxCompatTermios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
} linux_compat_termios_t;

typedef struct LinuxCompatSyscallTraceRecord {
    uint64_t number;
    int64_t return_value;
    int32_t errno_value;
    uintptr_t pc;
    char message[LINUX_COMPAT_MAX_MESSAGE];
} linux_compat_syscall_trace_record_t;

typedef struct LinuxCompatRuntime {
    struct LinuxCompatVm* vm;
    linux_compat_fd_t fds[LINUX_COMPAT_MAX_FDS];
    uint64_t program_break;
    uint64_t next_mmap;
    char stdout_buffer[LINUX_COMPAT_MAX_STDOUT];
    size_t stdout_size;
    bool exited;
    int32_t exit_code;
    linux_compat_overlay_node_t overlay_nodes[LINUX_COMPAT_MAX_OVERLAY_NODES];
    uint64_t next_overlay_inode;
    uint64_t next_overlay_mtime;
    linux_compat_syscall_trace_record_t trace_records[LINUX_COMPAT_MAX_TRACE_RECORDS];
    size_t trace_count;
    bool trace_truncated;
} linux_compat_runtime_t;

typedef struct LinuxCompatSyscallRequest {
    uint64_t number;
    int32_t dirfd;
    int32_t fd;
    const char* path;
    const char* new_path;
    const void* write_buffer;
    void* read_buffer;
    size_t length;
    uint64_t offset;
    linux_compat_stat_t* stat;
    linux_compat_statx_t* statx;
    linux_compat_dirent_t* dirents;
    size_t dirent_capacity;
    uint64_t addr;
    uint32_t prot;
    uint32_t flags;
    uint32_t command;
    uint64_t arg;
} linux_compat_syscall_request_t;

typedef struct LinuxCompatSyscallResponse {
    int64_t value;
} linux_compat_syscall_response_t;

linux_compat_result_t linux_compat_lookup(
    const char* path,
    linux_compat_rootfs_entry_t* out_entry,
    linux_compat_trace_t* out_trace);

linux_compat_result_t linux_compat_resolve_path(
    const char* command,
    char* out_path,
    size_t out_path_size,
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
