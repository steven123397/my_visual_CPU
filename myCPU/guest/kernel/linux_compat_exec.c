#include "linux_compat_exec.h"

#include "memory.h"

/* Linux compat 执行层：把 load_plan 映射进 guest VM，构造 argv/envp/auxv 栈，
   最后通过 trap_user_runtime 进入 U-mode。默认路径仍是 Course OS shell 显式调用。 */

#ifdef __riscv
#include "riscv.h"
#endif

#ifndef RISCV_SSTATUS_FS_MASK
#define RISCV_SSTATUS_FS_MASK (3ULL << 13)
#endif
#ifndef RISCV_SSTATUS_FS_INITIAL
#define RISCV_SSTATUS_FS_INITIAL (1ULL << 13)
#endif

#define ELF64_HEADER_SIZE 64U
#define ELF64_PHENTSIZE 56U
#define PT_LOAD_FLAG_X 0x1U
#define PT_LOAD_FLAG_W 0x2U
#define PT_LOAD_FLAG_R 0x4U
#define AT_NULL 0U
#define AT_PHDR 3U
#define AT_PHENT 4U
#define AT_PHNUM 5U
#define AT_PAGESZ 6U
#define AT_BASE 7U
#define AT_ENTRY 9U
#define AT_UID 11U
#define AT_EUID 12U
#define AT_GID 13U
#define AT_EGID 14U
#define AT_SECURE 23U
#define AT_RANDOM 25U
#define LINUX_COMPAT_STACK_PAGES 8U
#define LINUX_COMPAT_EXEC_MAX_SEGMENT_CHUNK \
    ((size_t)(MEMORY_PAGE_SIZE / sizeof(uintptr_t)) * (size_t)MEMORY_PAGE_SIZE)

__attribute__((weak)) uint64_t linux_compat_exec_read_sstatus(void) {
#ifdef __riscv
    return riscv_read_sstatus();
#else
    return 0;
#endif
}

__attribute__((weak)) void linux_compat_exec_set_sstatus_bits(uint64_t value) {
#ifdef __riscv
    riscv_set_sstatus_bits(value);
#else
    (void)value;
#endif
}

__attribute__((weak)) void linux_compat_exec_clear_sstatus_bits(uint64_t value) {
#ifdef __riscv
    riscv_clear_sstatus_bits(value);
#else
    (void)value;
#endif
}

/* 把地址向下对齐到页边界。 */
static uintptr_t align_down_page(uintptr_t value) {
    return value & ~((uintptr_t)MEMORY_PAGE_SIZE - 1U);
}

/* 把地址向下对齐到 16 字节。 */
static uintptr_t align_down_16(uintptr_t value) {
    return value & ~(uintptr_t)15U;
}

/* 把 size 向上对齐到页大小。 */
static size_t align_up_page_size(size_t value) {
    const size_t mask = (size_t)MEMORY_PAGE_SIZE - 1U;

    return (value + mask) & ~mask;
}

/* 临时打开 Linux compat 浮点状态，返回原 fs 值。 */
static uint64_t enable_linux_compat_floating_state(void) {
    const uint64_t previous_fs =
        linux_compat_exec_read_sstatus() & RISCV_SSTATUS_FS_MASK;

    /* 部分外部 ELF 会碰浮点寄存器；进入前临时打开 FS，返回后恢复。 */
    linux_compat_exec_clear_sstatus_bits(RISCV_SSTATUS_FS_MASK);
    linux_compat_exec_set_sstatus_bits(RISCV_SSTATUS_FS_INITIAL);
    return previous_fs;
}

/* 恢复之前的浮点状态。 */
static void restore_linux_compat_floating_state(uint64_t previous_fs) {
    linux_compat_exec_clear_sstatus_bits(RISCV_SSTATUS_FS_MASK);
    if (previous_fs != 0U) {
        linux_compat_exec_set_sstatus_bits(previous_fs);
    }
}

/* 取 C 字符串长度。 */
static size_t str_len(const char* value) {
    size_t i = 0;

    if (value == 0) {
        return 0;
    }
    while (value[i] != '\0') {
        i += 1U;
    }
    return i;
}

/* 安全拷贝字符串到定长缓冲并补 NUL。 */
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

/* 设置 trace 为一次失败/边界记录。 */
static void set_trace(linux_compat_trace_t* trace,
                      int32_t errno_value,
                      const char* message) {
    if (trace == 0) {
        return;
    }
    trace->path[0] = '\0';
    trace->errno_value = errno_value;
    trace->syscall_number = 0;
    trace->pc = 0;
    copy_str(trace->message, sizeof(trace->message), message);
}

/* 区间 [offset, offset+length) 是否完全落在镜像内。 */
static bool range_in_image(uint64_t offset, uint64_t length, size_t image_size) {
    const uint64_t end = offset + length;

    return end >= offset && offset <= (uint64_t)image_size &&
           end <= (uint64_t)image_size;
}

/* 把 ELF 段 flags 转成 VM 权限 flags。 */
static uint32_t segment_prot(uint32_t flags) {
    uint32_t prot = 0;

    if ((flags & PT_LOAD_FLAG_R) != 0U) {
        prot |= LINUX_COMPAT_PROT_READ;
    }
    if ((flags & PT_LOAD_FLAG_W) != 0U) {
        prot |= LINUX_COMPAT_PROT_WRITE;
    }
    if ((flags & PT_LOAD_FLAG_X) != 0U) {
        prot |= LINUX_COMPAT_PROT_EXEC;
    }
    return prot;
}

/* 取两值较小者。 */
static size_t min_size(size_t a, size_t b) {
    return a < b ? a : b;
}

/* 取两值较大者。 */
static size_t max_size(size_t a, size_t b) {
    return a > b ? a : b;
}

/* 把镜像字节写入 VM 对象（按对象页解析后逐页写）。 */
static bool write_object_bytes(vm_object_t* object,
                               size_t object_offset,
                               const uint8_t* bytes,
                               size_t length) {
    size_t copied = 0;

    while (copied < length) {
        const size_t byte_offset = object_offset + copied;
        const size_t page_offset = align_down_page((uintptr_t)byte_offset);
        const size_t in_page = byte_offset - page_offset;
        const size_t remain_in_page = MEMORY_PAGE_SIZE - in_page;
        const size_t chunk =
            (length - copied) < remain_in_page ? (length - copied)
                                               : remain_in_page;
        uintptr_t page = 0;

        if (!vm_object_resolve_page_for_write(object, page_offset, &page)) {
            return false;
        }
        if (bytes != 0 && chunk != 0U) {
            size_t i = 0;
            uint8_t* out = (uint8_t*)(page + in_page);

            for (i = 0; i < chunk; ++i) {
                out[i] = bytes[copied + i];
            }
        }
        copied += chunk;
    }
    return true;
}

/* 按小端写 8 字节到 out+offset。 */
static void write_u64_le(uint8_t* out, size_t offset, uint64_t value) {
    size_t i = 0;

    for (i = 0; i < 8U; ++i) {
        out[offset + i] = (uint8_t)((value >> (i * 8U)) & 0xffU);
    }
}

/* 在栈页内 offset 处写一个 64 位值。 */
static bool stack_write_u64(uint8_t* stack_page,
                            uintptr_t stack_base,
                            uintptr_t addr,
                            uint64_t value) {
    if (addr < stack_base ||
        addr + sizeof(uint64_t) < addr) {
        return false;
    }
    write_u64_le(stack_page, 0U, value);
    return true;
}

/* 向栈对象压入一个 64 位值，返回压入后栈顶。 */
static bool push_u64(vm_object_t* object,
                     uintptr_t stack_base,
                     uintptr_t* cursor,
                     uint64_t value) {
    uint8_t word[sizeof(uint64_t)];

    if (cursor == 0 || *cursor < stack_base + sizeof(uint64_t)) {
        return false;
    }
    *cursor -= sizeof(uint64_t);
    if (!stack_write_u64(word, stack_base, *cursor, value)) {
        return false;
    }
    return write_object_bytes(object,
                              (size_t)(*cursor - stack_base),
                              word,
                              sizeof(word));
}

linux_compat_result_t linux_compat_exec_load(
    linux_compat_vm_t* vm,
    const uint8_t* image,
    size_t image_size,
    const linux_compat_load_plan_t* plan,
    uintptr_t* out_entry_pc,
    linux_compat_trace_t* out_trace) {
    size_t i = 0;

    if (out_entry_pc != 0) {
        *out_entry_pc = 0;
    }
    if (vm == 0 || image == 0 || plan == 0 || plan->segment_count == 0U) {
        set_trace(out_trace, 22, "linux-compat: exec: bad request");
        return LINUX_COMPAT_ERR_BAD_ELF;
    }
    for (i = 0; i < plan->segment_count; ++i) {
        const linux_compat_load_segment_t* segment = &plan->segments[i];
        const uintptr_t page_vaddr = align_down_page((uintptr_t)segment->vaddr);
        const size_t page_delta = (size_t)(segment->vaddr - page_vaddr);
        const size_t mapped_length =
            align_up_page_size(page_delta + (size_t)segment->memsz);
        size_t chunk_offset = 0;

        /* PT_LOAD 以页为单位注册匿名对象，filesz 之外的 memsz 区间自然为 BSS。 */
        if (segment->memsz == 0U ||
            segment->filesz > segment->memsz ||
            !range_in_image(segment->offset, segment->filesz, image_size)) {
            set_trace(out_trace, 8, "linux-compat: exec: bad segment");
            return LINUX_COMPAT_ERR_BAD_ELF;
        }

        while (chunk_offset < mapped_length) {
            const size_t chunk_length =
                min_size(mapped_length - chunk_offset,
                         LINUX_COMPAT_EXEC_MAX_SEGMENT_CHUNK);
            const uintptr_t chunk_vaddr =
                page_vaddr + (uintptr_t)chunk_offset;
            const size_t file_start = page_delta;
            const size_t file_end = page_delta + (size_t)segment->filesz;
            const size_t chunk_start = chunk_offset;
            const size_t chunk_end = chunk_offset + chunk_length;
            linux_compat_vm_region_t* region =
                linux_compat_vm_map_fixed(vm,
                                          chunk_vaddr,
                                          chunk_length,
                                          segment_prot(segment->flags),
                                          0U);

            if (region == 0) {
                set_trace(out_trace,
                          12,
                          "linux-compat: exec: map segment failed");
                return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
            }
            if (file_start < chunk_end && file_end > chunk_start) {
                const size_t copy_start = max_size(file_start, chunk_start);
                const size_t copy_end = min_size(file_end, chunk_end);
                const size_t copy_length = copy_end - copy_start;
                const size_t object_offset = copy_start - chunk_start;
                const size_t image_offset =
                    (size_t)segment->offset + (copy_start - file_start);

                if (!write_object_bytes(&region->object,
                                        object_offset,
                                        image + image_offset,
                                        copy_length)) {
                    set_trace(out_trace,
                              12,
                              "linux-compat: exec: map segment failed");
                    return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
                }
            }
            chunk_offset += chunk_length;
        }
    }

    if (out_entry_pc != 0) {
        *out_entry_pc = (uintptr_t)plan->entry;
    }
    set_trace(out_trace, 0, "linux-compat: exec: load ok");
    return LINUX_COMPAT_OK;
}

linux_compat_result_t linux_compat_exec_build_stack(
    linux_compat_vm_t* vm,
    const linux_compat_load_plan_t* plan,
    size_t argc,
    const char* const* argv,
    uintptr_t* out_user_sp,
    linux_compat_trace_t* out_trace) {
    uintptr_t stack_top = 0;
    uintptr_t stack_base = 0;
    size_t stack_size = 0;
    linux_compat_vm_region_t* stack_region = 0;
    uintptr_t cursor = 0;
    uintptr_t string_addrs[LINUX_COMPAT_MAX_ARGS];
    uintptr_t random_addr = 0;
    uint64_t aux_values[32];
    size_t aux_count = 0;
    size_t frame_words = 0;
    size_t frame_size = 0;
    size_t i = 0;

    if (out_user_sp != 0) {
        *out_user_sp = 0;
    }
    if (vm == 0 || plan == 0 || argc > LINUX_COMPAT_MAX_ARGS ||
        (argc != 0U && argv == 0)) {
        set_trace(out_trace, 22, "linux-compat: exec: bad stack request");
        return LINUX_COMPAT_ERR_BAD_ELF;
    }

    stack_top = (uintptr_t)(plan->stack_top != 0U ? plan->stack_top
                                                  : LINUX_COMPAT_STACK_TOP);
    stack_size = (size_t)LINUX_COMPAT_STACK_PAGES * MEMORY_PAGE_SIZE;
    stack_base = stack_top - (uintptr_t)stack_size;
    stack_region = linux_compat_vm_map_fixed(vm,
                                             stack_base,
                                             stack_size,
                                             LINUX_COMPAT_PROT_READ |
                                                 LINUX_COMPAT_PROT_WRITE,
                                             0U);
    if (stack_region == 0) {
        set_trace(out_trace, 12, "linux-compat: exec: stack map failed");
        return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
    }
    cursor = stack_top;
    for (i = argc; i > 0U; --i) {
        const char* arg = argv[i - 1U];
        const size_t length = str_len(arg) + 1U;

        if (arg == 0 || length > (size_t)(cursor - stack_base)) {
            set_trace(out_trace, 22, "linux-compat: exec: argv too large");
            return LINUX_COMPAT_ERR_BAD_ELF;
        }
        cursor -= (uintptr_t)length;
        string_addrs[i - 1U] = cursor;
        (void)write_object_bytes(&stack_region->object,
                                 (size_t)(cursor - stack_base),
                                 (const uint8_t*)arg,
                                 length);
    }

    if (cursor < stack_base + 16U) {
        set_trace(out_trace, 22, "linux-compat: exec: stack too small");
        return LINUX_COMPAT_ERR_BAD_ELF;
    }
    cursor -= 16U;
    random_addr = cursor;
    {
        uint8_t random_bytes[16];

        for (i = 0; i < 16U; ++i) {
            random_bytes[i] = (uint8_t)(0x5aU + (uint8_t)(i * 17U));
        }
        if (!write_object_bytes(&stack_region->object,
                                (size_t)(random_addr - stack_base),
                                random_bytes,
                                sizeof(random_bytes))) {
            set_trace(out_trace, 12, "linux-compat: exec: stack random failed");
            return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
        }
    }
    cursor = align_down_16(cursor);

    aux_values[aux_count++] = AT_PHDR;
    aux_values[aux_count++] =
        plan->phdr_vaddr != 0U ? plan->phdr_vaddr
                                  : 0U;
    aux_values[aux_count++] = AT_PHENT;
    aux_values[aux_count++] = ELF64_PHENTSIZE;
    aux_values[aux_count++] = AT_PHNUM;
    aux_values[aux_count++] = plan->phnum;
    aux_values[aux_count++] = AT_PAGESZ;
    aux_values[aux_count++] = MEMORY_PAGE_SIZE;
    if (plan->requires_interp) {
        if (plan->interp_load_bias == 0U || plan->interp_entry == 0U) {
            set_trace(out_trace, 22, "linux-compat: exec: missing interp auxv");
            return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
        }
        aux_values[aux_count++] = AT_BASE;
        aux_values[aux_count++] = plan->interp_load_bias;
    }
    aux_values[aux_count++] = AT_ENTRY;
    aux_values[aux_count++] = plan->entry;
    aux_values[aux_count++] = AT_UID;
    aux_values[aux_count++] = 0U;
    aux_values[aux_count++] = AT_EUID;
    aux_values[aux_count++] = 0U;
    aux_values[aux_count++] = AT_GID;
    aux_values[aux_count++] = 0U;
    aux_values[aux_count++] = AT_EGID;
    aux_values[aux_count++] = 0U;
    aux_values[aux_count++] = AT_SECURE;
    aux_values[aux_count++] = 0U;
    aux_values[aux_count++] = AT_RANDOM;
    aux_values[aux_count++] = random_addr;
    aux_values[aux_count++] = AT_NULL;
    aux_values[aux_count++] = 0U;

    frame_words = 1U + argc + 1U + 1U +
                  aux_count;
    frame_size = frame_words * sizeof(uint64_t);
    if (frame_size > (size_t)(cursor - stack_base)) {
        set_trace(out_trace, 22, "linux-compat: exec: stack frame too large");
        return LINUX_COMPAT_ERR_BAD_ELF;
    }
    cursor = align_down_16(cursor - frame_size) + frame_size;

    for (i = aux_count; i > 0U; --i) {
        if (!push_u64(&stack_region->object,
                      stack_base,
                      &cursor,
                      aux_values[i - 1U])) {
            return LINUX_COMPAT_ERR_BAD_ELF;
        }
    }
    if (!push_u64(&stack_region->object, stack_base, &cursor, 0U)) {
        return LINUX_COMPAT_ERR_BAD_ELF;
    }
    if (!push_u64(&stack_region->object, stack_base, &cursor, 0U)) {
        return LINUX_COMPAT_ERR_BAD_ELF;
    }
    for (i = argc; i > 0U; --i) {
        if (!push_u64(&stack_region->object,
                      stack_base,
                      &cursor,
                      string_addrs[i - 1U])) {
            return LINUX_COMPAT_ERR_BAD_ELF;
        }
    }
    if (!push_u64(&stack_region->object,
                  stack_base,
                  &cursor,
                  (uint64_t)argc)) {
        return LINUX_COMPAT_ERR_BAD_ELF;
    }

    if ((cursor & 15U) != 0U) {
        set_trace(out_trace, 22, "linux-compat: exec: unaligned stack");
        return LINUX_COMPAT_ERR_BAD_ELF;
    }
    if (out_user_sp != 0) {
        *out_user_sp = cursor;
    }
    set_trace(out_trace, 0, "linux-compat: exec: stack ok");
    return LINUX_COMPAT_OK;
}

linux_compat_result_t linux_compat_exec_enter(
    linux_compat_vm_t* vm,
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime,
    void* trap_stack_base,
    size_t trap_stack_size,
    uintptr_t entry_pc,
    uintptr_t user_sp,
    linux_compat_runtime_t* runtime,
    linux_compat_trace_t* out_trace) {
    uint64_t previous_fs = 0;
    bool floating_state_enabled = false;

    if (vm == 0 || vm->process == 0 || trap_context == 0 ||
        user_runtime == 0 || runtime == 0 || entry_pc == 0 || user_sp == 0) {
        set_trace(out_trace, 22, "linux-compat: exec: bad enter request");
        return LINUX_COMPAT_ERR_BAD_ELF;
    }

    if (!trap_user_runtime_prepare_standard(user_runtime,
                                            trap_context,
                                            vm->process,
                                            entry_pc,
                                            user_sp,
                                            user_sp,
                                            trap_stack_base,
                                            trap_stack_size,
                                            entry_pc,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0) ||
        !trap_context_install_linux_compat_syscall_policy(trap_context,
                                                          user_runtime,
                                                          runtime) ||
        !trap_user_runtime_activate(user_runtime)) {
        set_trace(out_trace, 22, "linux-compat: exec: enter prepare failed");
        return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
    }

    previous_fs = enable_linux_compat_floating_state();
    floating_state_enabled = true;
    if (!trap_user_runtime_enter(user_runtime)) {
        restore_linux_compat_floating_state(previous_fs);
        floating_state_enabled = false;
        (void)trap_user_runtime_deactivate(user_runtime);
        set_trace(out_trace, 22, "linux-compat: exec: user enter failed");
        return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
    }
    restore_linux_compat_floating_state(previous_fs);
    floating_state_enabled = false;
    if (!runtime->exited) {
        (void)trap_user_runtime_deactivate(user_runtime);
        set_trace(out_trace, 22, "linux-compat: exec: unexpected trap return");
        return LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL;
    }
    if (runtime->user_faulted) {
        (void)trap_user_runtime_deactivate(user_runtime);
        set_trace(out_trace, 14, "linux-compat: exec: user fault");
        return LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL;
    }
    if (!trap_user_runtime_deactivate(user_runtime)) {
        if (floating_state_enabled) {
            restore_linux_compat_floating_state(previous_fs);
        }
        set_trace(out_trace, 22, "linux-compat: exec: deactivate failed");
        return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
    }

    set_trace(out_trace, 0, "linux-compat: exec: enter ok");
    return LINUX_COMPAT_OK;
}
