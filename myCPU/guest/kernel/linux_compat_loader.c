#include "linux_compat_loader.h"

#define ELF64_HEADER_SIZE 64U
#define ELF64_PHENTSIZE 56U
#define EM_RISCV 243U
#define ET_EXEC 2U
#define ET_DYN 3U
#define PT_LOAD 1U
#define PT_INTERP 3U

static void copy_str(char* out, size_t out_size, const char* value) {
    size_t i = 0;

    if (out == 0 || out_size == 0) {
        return;
    }
    if (value != 0) {
        while (value[i] != '\0' && i + 1U < out_size) {
            out[i] = value[i];
            i += 1U;
        }
    }
    out[i] = '\0';
}

static uint16_t read_u16_le(const uint8_t* image, size_t offset) {
    return (uint16_t)image[offset] |
           (uint16_t)((uint16_t)image[offset + 1U] << 8U);
}

static uint32_t read_u32_le(const uint8_t* image, size_t offset) {
    return (uint32_t)image[offset] |
           ((uint32_t)image[offset + 1U] << 8U) |
           ((uint32_t)image[offset + 2U] << 16U) |
           ((uint32_t)image[offset + 3U] << 24U);
}

static uint64_t read_u64_le(const uint8_t* image, size_t offset) {
    uint64_t value = 0;
    size_t i = 0;

    for (i = 0; i < 8U; ++i) {
        value |= (uint64_t)image[offset + i] << (i * 8U);
    }
    return value;
}

static bool add_u64(uint64_t a, uint64_t b, uint64_t* out) {
    const uint64_t value = a + b;

    if (value < a) {
        return false;
    }
    if (out != 0) {
        *out = value;
    }
    return true;
}

static bool range_in_image(uint64_t offset, uint64_t size, size_t image_size) {
    uint64_t end = 0;

    if (!add_u64(offset, size, &end)) {
        return false;
    }
    return offset <= (uint64_t)image_size && end <= (uint64_t)image_size;
}

static void clear_trace(linux_compat_trace_t* trace) {
    if (trace == 0) {
        return;
    }
    trace->path[0] = '\0';
    trace->errno_value = 0;
    trace->syscall_number = 0;
    trace->pc = 0;
    trace->message[0] = '\0';
}

static void set_trace(linux_compat_trace_t* trace,
                      int32_t errno_value,
                      const char* message) {
    if (trace == 0) {
        return;
    }
    clear_trace(trace);
    trace->errno_value = errno_value;
    copy_str(trace->message, sizeof(trace->message), message);
}

static void clear_load_plan(linux_compat_load_plan_t* plan) {
    size_t i = 0;

    if (plan == 0) {
        return;
    }
    plan->elf_type = 0;
    plan->entry = 0;
    plan->load_bias = 0;
    plan->segment_count = 0;
    for (i = 0; i < LINUX_COMPAT_MAX_LOAD_SEGMENTS; ++i) {
        plan->segments[i].vaddr = 0;
        plan->segments[i].memsz = 0;
        plan->segments[i].filesz = 0;
        plan->segments[i].offset = 0;
        plan->segments[i].flags = 0;
    }
    plan->requires_interp = false;
    plan->interp_path[0] = '\0';
    plan->stack_top = 0;
    plan->argv_count = 0;
    plan->envp_count = 0;
    plan->auxv_count = 0;
    plan->diagnostic[0] = '\0';
}

static linux_compat_result_t finish_error(linux_compat_load_plan_t* out_plan,
                                          linux_compat_load_plan_t* plan,
                                          linux_compat_trace_t* out_trace,
                                          linux_compat_result_t result,
                                          int32_t errno_value,
                                          const char* diagnostic) {
    copy_str(plan->diagnostic, sizeof(plan->diagnostic), diagnostic);
    if (out_plan != 0) {
        *out_plan = *plan;
    }
    set_trace(out_trace, errno_value, diagnostic);
    return result;
}

static bool copy_interp_path(const uint8_t* image,
                             uint64_t offset,
                             uint64_t filesz,
                             char* out,
                             size_t out_size) {
    size_t i = 0;

    if (out == 0 || out_size == 0 || filesz == 0U) {
        return false;
    }
    while (i < filesz) {
        const char ch = (char)image[(size_t)offset + i];

        if (ch == '\0') {
            out[i] = '\0';
            return i > 0U;
        }
        if (i + 1U >= out_size) {
            out[0] = '\0';
            return false;
        }
        out[i] = ch;
        i += 1U;
    }
    out[0] = '\0';
    return false;
}

linux_compat_result_t linux_compat_build_load_plan(
    const uint8_t* image,
    size_t image_size,
    size_t argc,
    size_t envp_count,
    linux_compat_load_plan_t* out_plan,
    linux_compat_trace_t* out_trace) {
    linux_compat_load_plan_t plan;
    uint16_t machine = 0;
    uint64_t raw_entry = 0;
    uint64_t phoff = 0;
    uint16_t phentsize = 0;
    uint16_t phnum = 0;
    uint64_t ph_end = 0;
    uint16_t i = 0;

    clear_load_plan(out_plan);
    clear_load_plan(&plan);

    if (image == 0 || image_size < ELF64_HEADER_SIZE) {
        return finish_error(out_plan,
                            &plan,
                            out_trace,
                            LINUX_COMPAT_ERR_BAD_ELF,
                            8,
                            "linux-compat: loader: bad header");
    }
    if (image[0] != 0x7fU || image[1] != 'E' || image[2] != 'L' ||
        image[3] != 'F') {
        return finish_error(out_plan,
                            &plan,
                            out_trace,
                            LINUX_COMPAT_ERR_BAD_ELF,
                            8,
                            "linux-compat: loader: bad magic");
    }
    if (image[4] != 2U || image[5] != 1U) {
        return finish_error(out_plan,
                            &plan,
                            out_trace,
                            LINUX_COMPAT_ERR_UNSUPPORTED_ELF,
                            9,
                            "linux-compat: loader: unsupported class/data");
    }

    plan.elf_type = read_u16_le(image, 16U);
    machine = read_u16_le(image, 18U);
    raw_entry = read_u64_le(image, 24U);
    phoff = read_u64_le(image, 32U);
    phentsize = read_u16_le(image, 54U);
    phnum = read_u16_le(image, 56U);

    if (machine != EM_RISCV) {
        return finish_error(out_plan,
                            &plan,
                            out_trace,
                            LINUX_COMPAT_ERR_UNSUPPORTED_ELF,
                            9,
                            "linux-compat: loader: unsupported machine");
    }
    if (plan.elf_type != ET_EXEC && plan.elf_type != ET_DYN) {
        return finish_error(out_plan,
                            &plan,
                            out_trace,
                            LINUX_COMPAT_ERR_UNSUPPORTED_ELF,
                            9,
                            "linux-compat: loader: unsupported type");
    }
    if (phentsize < ELF64_PHENTSIZE) {
        return finish_error(out_plan,
                            &plan,
                            out_trace,
                            LINUX_COMPAT_ERR_BAD_ELF,
                            8,
                            "linux-compat: loader: bad phentsize");
    }
    if (!add_u64(phoff, (uint64_t)phentsize * (uint64_t)phnum, &ph_end) ||
        phoff > (uint64_t)image_size || ph_end > (uint64_t)image_size) {
        return finish_error(out_plan,
                            &plan,
                            out_trace,
                            LINUX_COMPAT_ERR_BAD_ELF,
                            8,
                            "linux-compat: loader: bad program headers");
    }

    plan.load_bias = plan.elf_type == ET_DYN ? LINUX_COMPAT_DYN_LOAD_BIAS : 0U;
    if (!add_u64(raw_entry, plan.load_bias, &plan.entry)) {
        return finish_error(out_plan,
                            &plan,
                            out_trace,
                            LINUX_COMPAT_ERR_BAD_ELF,
                            8,
                            "linux-compat: loader: bad entry");
    }
    plan.stack_top = LINUX_COMPAT_STACK_TOP;
    plan.argv_count = argc;
    plan.envp_count = envp_count;
    plan.auxv_count = LINUX_COMPAT_AUXV_COUNT;

    for (i = 0; i < phnum; ++i) {
        const size_t ph_offset =
            (size_t)phoff + ((size_t)i * (size_t)phentsize);
        const uint32_t ph_type = read_u32_le(image, ph_offset + 0U);
        const uint32_t ph_flags = read_u32_le(image, ph_offset + 4U);
        const uint64_t p_offset = read_u64_le(image, ph_offset + 8U);
        const uint64_t p_vaddr = read_u64_le(image, ph_offset + 16U);
        const uint64_t p_filesz = read_u64_le(image, ph_offset + 32U);
        const uint64_t p_memsz = read_u64_le(image, ph_offset + 40U);

        if (ph_type == PT_LOAD) {
            linux_compat_load_segment_t* segment = 0;

            if (plan.segment_count >= LINUX_COMPAT_MAX_LOAD_SEGMENTS) {
                return finish_error(out_plan,
                                    &plan,
                                    out_trace,
                                    LINUX_COMPAT_ERR_UNSUPPORTED_ELF,
                                    9,
                                    "linux-compat: loader: too many load segments");
            }
            if (p_filesz > p_memsz ||
                !range_in_image(p_offset, p_filesz, image_size)) {
                return finish_error(out_plan,
                                    &plan,
                                    out_trace,
                                    LINUX_COMPAT_ERR_BAD_ELF,
                                    8,
                                    "linux-compat: loader: bad load segment");
            }
            segment = &plan.segments[plan.segment_count];
            if (!add_u64(p_vaddr, plan.load_bias, &segment->vaddr)) {
                return finish_error(out_plan,
                                    &plan,
                                    out_trace,
                                    LINUX_COMPAT_ERR_BAD_ELF,
                                    8,
                                    "linux-compat: loader: bad segment address");
            }
            segment->offset = p_offset;
            segment->filesz = p_filesz;
            segment->memsz = p_memsz;
            segment->flags = ph_flags;
            plan.segment_count += 1U;
        } else if (ph_type == PT_INTERP) {
            if (!range_in_image(p_offset, p_filesz, image_size)) {
                return finish_error(out_plan,
                                    &plan,
                                    out_trace,
                                    LINUX_COMPAT_ERR_BAD_ELF,
                                    8,
                                    "linux-compat: loader: bad interp segment");
            }
            if (!copy_interp_path(image,
                                  p_offset,
                                  p_filesz,
                                  plan.interp_path,
                                  sizeof(plan.interp_path))) {
                return finish_error(out_plan,
                                    &plan,
                                    out_trace,
                                    LINUX_COMPAT_ERR_UNSUPPORTED_ELF,
                                    9,
                                    "linux-compat: loader: interp path too long");
            }
            plan.requires_interp = true;
        }
    }

    if (plan.segment_count == 0U) {
        return finish_error(out_plan,
                            &plan,
                            out_trace,
                            LINUX_COMPAT_ERR_UNSUPPORTED_ELF,
                            9,
                            "linux-compat: loader: no load segments");
    }

    if (plan.requires_interp) {
        copy_str(plan.diagnostic,
                 sizeof(plan.diagnostic),
                 "linux-compat: loader: requires interp");
    } else {
        copy_str(plan.diagnostic,
                 sizeof(plan.diagnostic),
                 "linux-compat: loader: ok static");
    }
    if (out_plan != 0) {
        *out_plan = plan;
    }
    set_trace(out_trace, 0, plan.diagnostic);
    return LINUX_COMPAT_OK;
}
