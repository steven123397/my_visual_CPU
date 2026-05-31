#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINUX_COMPAT_MAX_PATH 64U
#define LINUX_COMPAT_MAX_MESSAGE 128U
#define LINUX_COMPAT_MAX_ARGS 8U

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
