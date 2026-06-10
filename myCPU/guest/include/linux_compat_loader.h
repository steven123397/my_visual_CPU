#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "linux_compat.h"

#define LINUX_COMPAT_MAX_LOAD_SEGMENTS 8U
#define LINUX_COMPAT_DYN_LOAD_BIAS UINT64_C(0x40000000)
#define LINUX_COMPAT_INTERP_LOAD_BIAS UINT64_C(0x50000000)
#define LINUX_COMPAT_STACK_TOP UINT64_C(0x70000000)
#define LINUX_COMPAT_AUXV_COUNT 12U

typedef struct LinuxCompatLoadSegment {
    uint64_t vaddr;
    uint64_t memsz;
    uint64_t filesz;
    uint64_t offset;
    uint32_t flags;
} linux_compat_load_segment_t;

typedef struct LinuxCompatLoadPlan {
    uint16_t elf_type;
    uint64_t entry;
    uint64_t load_bias;
    uint64_t phdr_vaddr;
    uint16_t phnum;
    size_t segment_count;
    linux_compat_load_segment_t segments[LINUX_COMPAT_MAX_LOAD_SEGMENTS];
    bool requires_interp;
    char interp_path[LINUX_COMPAT_MAX_PATH];
    uint64_t interp_load_bias;
    uint64_t interp_entry;
    uint64_t stack_top;
    size_t argv_count;
    size_t envp_count;
    size_t auxv_count;
    char diagnostic[LINUX_COMPAT_MAX_MESSAGE];
} linux_compat_load_plan_t;

linux_compat_result_t linux_compat_build_load_plan(
    const uint8_t* image,
    size_t image_size,
    size_t argc,
    size_t envp_count,
    linux_compat_load_plan_t* out_plan,
    linux_compat_trace_t* out_trace);
