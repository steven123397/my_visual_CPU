#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/linux_compat_loader.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static void write_u16_le(uint8_t* image, size_t offset, uint16_t value) {
    image[offset] = (uint8_t)(value & 0xffU);
    image[offset + 1U] = (uint8_t)((value >> 8U) & 0xffU);
}

static void write_u32_le(uint8_t* image, size_t offset, uint32_t value) {
    image[offset] = (uint8_t)(value & 0xffU);
    image[offset + 1U] = (uint8_t)((value >> 8U) & 0xffU);
    image[offset + 2U] = (uint8_t)((value >> 16U) & 0xffU);
    image[offset + 3U] = (uint8_t)((value >> 24U) & 0xffU);
}

static void write_u64_le(uint8_t* image, size_t offset, uint64_t value) {
    size_t i = 0;

    for (i = 0; i < 8U; ++i) {
        image[offset + i] = (uint8_t)((value >> (i * 8U)) & 0xffU);
    }
}

static void clear_image(uint8_t* image, size_t size) {
    size_t i = 0;

    for (i = 0; i < size; ++i) {
        image[i] = 0U;
    }
}

static void make_elf_header(uint8_t* image,
                            size_t size,
                            uint16_t type,
                            uint16_t machine,
                            uint64_t entry,
                            uint16_t phnum) {
    clear_image(image, size);
    image[0] = 0x7fU;
    image[1] = 'E';
    image[2] = 'L';
    image[3] = 'F';
    image[4] = 2U;
    image[5] = 1U;
    image[6] = 1U;
    write_u16_le(image, 16U, type);
    write_u16_le(image, 18U, machine);
    write_u32_le(image, 20U, 1U);
    write_u64_le(image, 24U, entry);
    write_u64_le(image, 32U, 64U);
    write_u16_le(image, 52U, 64U);
    write_u16_le(image, 54U, 56U);
    write_u16_le(image, 56U, phnum);
}

static void write_program_header(uint8_t* image,
                                 size_t index,
                                 uint32_t type,
                                 uint32_t flags,
                                 uint64_t offset,
                                 uint64_t vaddr,
                                 uint64_t filesz,
                                 uint64_t memsz) {
    const size_t base = 64U + (index * 56U);

    write_u32_le(image, base + 0U, type);
    write_u32_le(image, base + 4U, flags);
    write_u64_le(image, base + 8U, offset);
    write_u64_le(image, base + 16U, vaddr);
    write_u64_le(image, base + 24U, vaddr);
    write_u64_le(image, base + 32U, filesz);
    write_u64_le(image, base + 40U, memsz);
    write_u64_le(image, base + 48U, 0x1000U);
}

static int test_exec_load_plan_records_pt_load_segment(void) {
    uint8_t image[256];
    linux_compat_load_plan_t plan;
    linux_compat_trace_t trace;

    make_elf_header(image, sizeof(image), 2U, 243U, 0x401234U, 1U);
    write_program_header(image, 0U, 1U, 5U, 0U, 0x400000U, 0x80U, 0x100U);

    if (linux_compat_build_load_plan(image,
                                     sizeof(image),
                                     2U,
                                     0U,
                                     &plan,
                                     &trace) != LINUX_COMPAT_OK ||
        plan.elf_type != 2U ||
        plan.entry != 0x401234U ||
        plan.load_bias != 0U ||
        plan.segment_count != 1U ||
        plan.segments[0].vaddr != 0x400000U ||
        plan.segments[0].offset != 0U ||
        plan.segments[0].filesz != 0x80U ||
        plan.segments[0].memsz != 0x100U ||
        plan.segments[0].flags != 5U ||
        plan.requires_interp ||
        plan.interp_path[0] != '\0' ||
        plan.stack_top == 0U ||
        plan.argv_count != 2U ||
        plan.envp_count != 0U ||
        plan.auxv_count == 0U ||
        !contains(plan.diagnostic, "loader: ok static")) {
        return fail("expected RV64 ET_EXEC PT_LOAD load plan");
    }

    return 0;
}

static int test_dyn_interp_load_plan_records_bias_and_interp_path(void) {
    uint8_t image[512];
    const char interp[] = "/lib/ld-musl-riscv64.so.1";
    linux_compat_load_plan_t plan;
    linux_compat_trace_t trace;

    make_elf_header(image, sizeof(image), 3U, 243U, 0x1200U, 2U);
    write_program_header(image, 0U, 1U, 5U, 0U, 0U, 0x90U, 0x200U);
    write_program_header(image,
                         1U,
                         3U,
                         4U,
                         0x180U,
                         0U,
                         sizeof(interp),
                         sizeof(interp));
    memcpy(image + 0x180U, interp, sizeof(interp));

    if (linux_compat_build_load_plan(image,
                                     sizeof(image),
                                     1U,
                                     2U,
                                     &plan,
                                     &trace) != LINUX_COMPAT_OK ||
        plan.elf_type != 3U ||
        plan.load_bias != LINUX_COMPAT_DYN_LOAD_BIAS ||
        plan.entry != LINUX_COMPAT_DYN_LOAD_BIAS + 0x1200U ||
        plan.segment_count != 1U ||
        plan.segments[0].vaddr != LINUX_COMPAT_DYN_LOAD_BIAS ||
        !plan.requires_interp ||
        strcmp(plan.interp_path, interp) != 0 ||
        plan.argv_count != 1U ||
        plan.envp_count != 2U ||
        !contains(plan.diagnostic, "loader: requires interp")) {
        return fail("expected ET_DYN PT_INTERP load plan with deterministic bias");
    }

    return 0;
}

static int test_loader_fails_closed_for_bad_or_unsupported_elf(void) {
    uint8_t image[256];
    linux_compat_load_plan_t plan;
    linux_compat_trace_t trace;

    make_elf_header(image, sizeof(image), 2U, 243U, 0x401000U, 1U);
    image[0] = 0U;
    if (linux_compat_build_load_plan(image,
                                     sizeof(image),
                                     1U,
                                     0U,
                                     &plan,
                                     &trace) != LINUX_COMPAT_ERR_BAD_ELF ||
        !contains(plan.diagnostic, "bad magic") ||
        !contains(trace.message, "bad magic")) {
        return fail("expected bad ELF magic to fail closed");
    }

    make_elf_header(image, sizeof(image), 2U, 62U, 0x401000U, 1U);
    if (linux_compat_build_load_plan(image,
                                     sizeof(image),
                                     1U,
                                     0U,
                                     &plan,
                                     &trace) != LINUX_COMPAT_ERR_UNSUPPORTED_ELF ||
        !contains(plan.diagnostic, "unsupported machine")) {
        return fail("expected non-RISCV ELF to fail closed");
    }

    make_elf_header(image, sizeof(image), 2U, 243U, 0x401000U, 1U);
    write_u64_le(image, 32U, sizeof(image) - 8U);
    if (linux_compat_build_load_plan(image,
                                     sizeof(image),
                                     1U,
                                     0U,
                                     &plan,
                                     &trace) != LINUX_COMPAT_ERR_BAD_ELF ||
        !contains(plan.diagnostic, "bad program headers")) {
        return fail("expected program header overflow to fail closed");
    }

    return 0;
}

static int test_loader_fails_closed_for_interp_path_too_long(void) {
    uint8_t image[512];
    size_t i = 0;
    linux_compat_load_plan_t plan;
    linux_compat_trace_t trace;

    make_elf_header(image, sizeof(image), 3U, 243U, 0x1000U, 1U);
    write_program_header(image, 0U, 3U, 4U, 0x100U, 0U, 80U, 80U);
    for (i = 0; i < 79U; ++i) {
        image[0x100U + i] = 'a';
    }
    image[0x100U + 79U] = '\0';

    if (linux_compat_build_load_plan(image,
                                     sizeof(image),
                                     1U,
                                     0U,
                                     &plan,
                                     &trace) != LINUX_COMPAT_ERR_UNSUPPORTED_ELF ||
        !contains(plan.diagnostic, "interp path too long")) {
        return fail("expected overlong PT_INTERP path to fail closed");
    }

    return 0;
}

static int test_syscall_trace_records_order_and_truncation(void) {
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    size_t i = 0;

    linux_compat_runtime_init(&runtime);
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_BRK;
    request.addr = 0x1000U;

    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        runtime.trace_count != 1U ||
        runtime.trace_records[0].number != LINUX_COMPAT_SYS_BRK ||
        runtime.trace_records[0].return_value != response.value ||
        runtime.trace_records[0].errno_value != 0 ||
        runtime.trace_records[0].pc != 0x1000U ||
        !contains(runtime.trace_records[0].message, "brk")) {
        return fail("expected brk syscall to append a trace record");
    }

    for (i = 0; i < LINUX_COMPAT_MAX_TRACE_RECORDS + 4U; ++i) {
        request.number = LINUX_COMPAT_SYS_MMAP;
        request.length = 4096U;
        request.addr = 0x2000U + i;
        if (linux_compat_syscall_dispatch(&runtime,
                                          &request,
                                          &response,
                                          &trace) != LINUX_COMPAT_OK) {
            return fail("expected repeated mmap trace records to stay supported");
        }
    }

    if (runtime.trace_count != LINUX_COMPAT_MAX_TRACE_RECORDS ||
        !runtime.trace_truncated ||
        runtime.trace_records[LINUX_COMPAT_MAX_TRACE_RECORDS - 1U].number !=
            LINUX_COMPAT_SYS_MMAP) {
        return fail("expected trace record buffer to cap and mark truncation");
    }

    return 0;
}

int main(void) {
    if (test_exec_load_plan_records_pt_load_segment() != 0 ||
        test_dyn_interp_load_plan_records_bias_and_interp_path() != 0 ||
        test_loader_fails_closed_for_bad_or_unsupported_elf() != 0 ||
        test_loader_fails_closed_for_interp_path_too_long() != 0 ||
        test_syscall_trace_records_order_and_truncation() != 0) {
        return 1;
    }

    return 0;
}
