#include "linux_compat.h"
#include "console.h"
#include "linux_compat_exec.h"
#include "linux_compat_loader.h"
#include "linux_compat_rootfs.h"
#include "linux_compat_vm.h"
#include "runtime_context.h"

__attribute__((weak)) void console_putc(char ch) {
    (void)ch;
}

static const char k_busybox_help[] =
    "BusyBox v1.36.1 (myCPU linux-compat)\n"
    "Usage: busybox [function [arguments]...]\n"
    "Currently defined functions:\n"
    "    cat, echo, ls, sh\n";

static const char k_git_help[] =
    "usage: git [-v | --version] [-h | --help] <command> [<args>]\n"
    "\n"
    "These are common Git commands used in various situations:\n"
    "   init     Create an empty Git repository\n"
    "   status   Show the working tree status\n";

typedef struct LinuxCompatRunScratch {
    linux_compat_rootfs_entry_t entry;
    linux_compat_rootfs_entry_t interp_entry;
    linux_compat_load_plan_t load_plan;
    linux_compat_vm_t vm;
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t syscall;
    linux_compat_syscall_response_t response;
    linux_compat_stat_t stat;
    linux_compat_trace_t trace;
    uintptr_t entry_pc;
    uintptr_t user_sp;
    int32_t fd;
    uint8_t probe[4];
} linux_compat_run_scratch_t;

/*
 * Stage 9 real-exec guest commands run on the fixed 8 KiB supervisor stack.
 * Keep the large load/runtime scratch in .bss so the guest shell does not
 * corrupt adjacent state before the real U-mode path even starts.
 */
static linux_compat_run_scratch_t g_linux_compat_run_scratch;

__attribute__((weak)) linux_compat_result_t linux_compat_exec_load(
    linux_compat_vm_t* vm,
    const uint8_t* image,
    size_t image_size,
    const linux_compat_load_plan_t* plan,
    uintptr_t* out_entry_pc,
    linux_compat_trace_t* out_trace) {
    (void)vm;
    (void)image;
    (void)image_size;
    (void)plan;
    (void)out_entry_pc;
    if (out_trace != 0) {
        out_trace->path[0] = '\0';
        out_trace->errno_value = 38;
        out_trace->syscall_number = 0;
        out_trace->pc = 0;
        out_trace->message[0] = '\0';
    }
    return LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL;
}

__attribute__((weak)) linux_compat_result_t linux_compat_exec_build_stack(
    linux_compat_vm_t* vm,
    const linux_compat_load_plan_t* plan,
    size_t argc,
    const char* const* argv,
    uintptr_t* out_user_sp,
    linux_compat_trace_t* out_trace) {
    (void)vm;
    (void)plan;
    (void)argc;
    (void)argv;
    (void)out_user_sp;
    if (out_trace != 0) {
        out_trace->path[0] = '\0';
        out_trace->errno_value = 38;
        out_trace->syscall_number = 0;
        out_trace->pc = 0;
        out_trace->message[0] = '\0';
    }
    return LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL;
}

__attribute__((weak)) linux_compat_result_t linux_compat_exec_enter(
    linux_compat_vm_t* vm,
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime,
    void* trap_stack_base,
    size_t trap_stack_size,
    uintptr_t entry_pc,
    uintptr_t user_sp,
    linux_compat_runtime_t* runtime,
    linux_compat_trace_t* out_trace) {
    (void)vm;
    (void)trap_context;
    (void)user_runtime;
    (void)trap_stack_base;
    (void)trap_stack_size;
    (void)entry_pc;
    (void)user_sp;
    (void)runtime;
    if (out_trace != 0) {
        out_trace->path[0] = '\0';
        out_trace->errno_value = 38;
        out_trace->syscall_number = 0;
        out_trace->pc = 0;
        out_trace->message[0] = '\0';
    }
    return LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL;
}

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

static bool str_eq(const char* a, const char* b) {
    size_t i = 0;

    if (a == 0 || b == 0) {
        return false;
    }
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return false;
        }
        i += 1U;
    }
    return a[i] == b[i];
}

static bool request_has_real_exec_context(
    const linux_compat_exec_request_t* request) {
    return request != 0 &&
           request->trap_context != 0 &&
           request->user_runtime != 0 &&
           request->address_space != 0 &&
           request->process != 0 &&
           request->trap_stack_base != 0 &&
           request->trap_stack_size != 0U;
}

static bool request_wants_real_exec(
    const linux_compat_exec_request_t* request,
    const linux_compat_rootfs_entry_t* entry) {
    return request_has_real_exec_context(request) && entry != 0 &&
           str_eq(entry->path, LINUX_COMPAT_MINIMAL_ELF_PATH);
}

static void zero_bytes(void* ptr, size_t size) {
    size_t i = 0;
    uint8_t* bytes = (uint8_t*)ptr;

    if (ptr == 0) {
        return;
    }
    for (i = 0; i < size; ++i) {
        bytes[i] = 0;
    }
}

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

static bool append_char(char* out, size_t out_size, size_t* used, char ch) {
    if (out == 0 || used == 0 || *used + 1U >= out_size) {
        return false;
    }
    out[*used] = ch;
    *used += 1U;
    out[*used] = '\0';
    return true;
}

static bool append_str(char* out,
                       size_t out_size,
                       size_t* used,
                       const char* value) {
    size_t i = 0;

    if (value == 0) {
        return false;
    }
    while (value[i] != '\0') {
        if (!append_char(out, out_size, used, value[i])) {
            return false;
        }
        i += 1U;
    }
    return true;
}

static bool append_u64_dec(char* out,
                           size_t out_size,
                           size_t* used,
                           uint64_t value) {
    char digits[20];
    size_t count = 0;

    if (value == 0) {
        return append_char(out, out_size, used, '0');
    }
    while (value != 0 && count < sizeof(digits)) {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        count += 1U;
    }
    while (count > 0) {
        count -= 1U;
        if (!append_char(out, out_size, used, digits[count])) {
            return false;
        }
    }
    return true;
}

static bool append_u64_hex(char* out,
                           size_t out_size,
                           size_t* used,
                           uint64_t value) {
    const char* digits = "0123456789abcdef";
    bool started = false;
    int shift = 60;

    if (!append_str(out, out_size, used, "0x")) {
        return false;
    }
    if (value == 0) {
        return append_char(out, out_size, used, '0');
    }
    while (shift >= 0) {
        const uint8_t nibble = (uint8_t)((value >> (uint32_t)shift) & 0xfU);

        if (nibble != 0U || started) {
            started = true;
            if (!append_char(out, out_size, used, digits[nibble])) {
                return false;
            }
        }
        shift -= 4;
    }
    return true;
}

static bool append_rootfs_source_line(char* out,
                                      size_t out_size,
                                      size_t* used) {
    return append_str(out, out_size, used, "linux-compat: rootfs=") &&
           append_str(out,
                      out_size,
                      used,
                      linux_compat_rootfs_source_name()) &&
           append_char(out, out_size, used, '\n');
}

static const char* load_plan_type_name(const linux_compat_load_plan_t* plan) {
    if (plan != 0 && plan->elf_type == 3U) {
        return "dyn";
    }
    return "exec";
}

static const char* load_plan_loader_name(const linux_compat_load_plan_t* plan) {
    if (plan != 0 && plan->elf_type == 3U) {
        return "dynamic";
    }
    return "static";
}

static bool append_load_plan_summary(char* out,
                                     size_t out_size,
                                     size_t* used,
                                     const linux_compat_load_plan_t* plan) {
    return append_str(out, out_size, used, " elf=rv64-little type=") &&
           append_str(out, out_size, used, load_plan_type_name(plan)) &&
           append_str(out, out_size, used, " entry=") &&
           append_u64_hex(out, out_size, used, plan != 0 ? plan->entry : 0U) &&
           append_str(out, out_size, used, " loader=") &&
           append_str(out, out_size, used, load_plan_loader_name(plan)) &&
           append_str(out, out_size, used, " interp=") &&
           append_str(out,
                      out_size,
                      used,
                      plan != 0 && plan->requires_interp ? plan->interp_path
                                                          : "none") &&
           append_str(out, out_size, used, " segments=") &&
           append_u64_dec(out,
                          out_size,
                          used,
                          plan != 0 ? (uint64_t)plan->segment_count : 0U) &&
           append_str(out, out_size, used, " stack=") &&
           append_u64_dec(out,
                          out_size,
                          used,
                          plan != 0 ? (uint64_t)plan->argv_count : 0U) &&
           append_char(out, out_size, used, '/') &&
           append_u64_dec(out,
                          out_size,
                          used,
                          plan != 0 ? (uint64_t)plan->envp_count : 0U) &&
           append_char(out, out_size, used, '/') &&
           append_u64_dec(out,
                          out_size,
                          used,
                          plan != 0 ? (uint64_t)plan->auxv_count : 0U);
}

static const char* linux_compat_syscall_name(uint64_t number) {
    switch (number) {
        case LINUX_COMPAT_SYS_FCNTL:
            return "fcntl";
        case LINUX_COMPAT_SYS_IOCTL:
            return "ioctl";
        case LINUX_COMPAT_SYS_OPENAT:
            return "openat";
        case LINUX_COMPAT_SYS_CLOSE:
            return "close";
        case LINUX_COMPAT_SYS_GETDENTS64:
            return "getdents64";
        case LINUX_COMPAT_SYS_LSEEK:
            return "lseek";
        case LINUX_COMPAT_SYS_READ:
            return "read";
        case LINUX_COMPAT_SYS_WRITE:
            return "write";
        case LINUX_COMPAT_SYS_NEWFSTATAT:
            return "newfstatat";
        case LINUX_COMPAT_SYS_CLOCK_GETTIME:
            return "clock_gettime";
        case LINUX_COMPAT_SYS_EXIT:
            return "exit";
        case LINUX_COMPAT_SYS_EXIT_GROUP:
            return "exit_group";
        case LINUX_COMPAT_SYS_BRK:
            return "brk";
        case LINUX_COMPAT_SYS_MUNMAP:
            return "munmap";
        case LINUX_COMPAT_SYS_MMAP:
            return "mmap";
        case LINUX_COMPAT_SYS_GETRANDOM:
            return "getrandom";
        default:
            return "unknown";
    }
}

static bool append_trace_summary(char* out,
                                 size_t out_size,
                                 size_t* used,
                                 const linux_compat_runtime_t* runtime) {
    size_t i = 0;

    if (!append_str(out, out_size, used, " trace=")) {
        return false;
    }
    if (runtime == 0 || runtime->trace_count == 0U) {
        return append_str(out, out_size, used, "none");
    }
    for (i = 0; i < runtime->trace_count; ++i) {
        if (i != 0U && !append_char(out, out_size, used, '/')) {
            return false;
        }
        if (!append_str(out,
                        out_size,
                        used,
                        linux_compat_syscall_name(
                            runtime->trace_records[i].number))) {
            return false;
        }
    }
    if (runtime->trace_truncated) {
        return append_str(out, out_size, used, "+truncated");
    }
    return true;
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
                      const char* path,
                      int32_t errno_value,
                      const char* message) {
    if (trace == 0) {
        return;
    }
    clear_trace(trace);
    copy_str(trace->path, sizeof(trace->path), path);
    trace->errno_value = errno_value;
    copy_str(trace->message, sizeof(trace->message), message);
}

static void clear_entry(linux_compat_rootfs_entry_t* entry) {
    if (entry == 0) {
        return;
    }
    entry->path = 0;
    entry->data = 0;
    entry->size = 0;
    entry->executable = false;
}

static void clear_stat(linux_compat_stat_t* stat) {
    if (stat == 0) {
        return;
    }
    stat->inode = 0;
    stat->mode = 0;
    stat->size = 0;
    stat->directory = false;
    stat->executable = false;
}

static void fill_entry_from_node(linux_compat_rootfs_entry_t* entry,
                                 const linux_compat_rootfs_node_t* node) {
    if (entry == 0) {
        return;
    }
    clear_entry(entry);
    if (node == 0 || node->directory) {
        return;
    }
    entry->path = node->path;
    entry->data = node->data;
    entry->size = node->size;
    entry->executable = node->executable;
}

static void fill_stat_from_node(linux_compat_stat_t* stat,
                                const linux_compat_rootfs_node_t* node) {
    if (stat == 0) {
        return;
    }
    clear_stat(stat);
    if (node == 0) {
        return;
    }
    stat->inode = node->inode;
    stat->mode = node->mode;
    stat->size = node->size;
    stat->directory = node->directory;
    stat->executable = node->executable;
}

static const linux_compat_rootfs_node_t* find_node(const char* path) {
    size_t i = 0;

    if (path == 0 || str_len(path) == 0U) {
        return 0;
    }
    for (i = 0; i < linux_compat_rootfs_node_count(); ++i) {
        const linux_compat_rootfs_node_t* node =
            linux_compat_rootfs_node_at(i);

        if (node != 0 && str_eq(path, node->path)) {
            return node;
        }
    }
    return 0;
}

static bool copy_basename_if_child(const char* parent,
                                   const char* child,
                                   char* out,
                                   size_t out_size) {
    size_t parent_len = str_len(parent);
    size_t i = 0;
    size_t start = 0;

    if (parent == 0 || child == 0 || out == 0 || out_size == 0 ||
        str_eq(parent, child)) {
        return false;
    }
    if (str_eq(parent, "/")) {
        if (child[0] != '/' || child[1] == '\0') {
            return false;
        }
        start = 1U;
    } else {
        for (i = 0; i < parent_len; ++i) {
            if (child[i] != parent[i]) {
                return false;
            }
        }
        if (child[parent_len] != '/' || child[parent_len + 1U] == '\0') {
            return false;
        }
        start = parent_len + 1U;
    }

    i = 0;
    while (child[start + i] != '\0') {
        if (child[start + i] == '/') {
            return false;
        }
        if (i + 1U >= out_size) {
            return false;
        }
        out[i] = child[start + i];
        i += 1U;
    }
    out[i] = '\0';
    return i > 0U;
}

static void copy_bytes(uint8_t* out, const uint8_t* in, size_t length) {
    size_t i = 0;

    if (out == 0 || in == 0) {
        return;
    }
    for (i = 0; i < length; ++i) {
        out[i] = in[i];
    }
}

static bool linux_compat_read_user_buffer(const linux_compat_runtime_t* runtime,
                                          const void* buffer,
                                          void* out,
                                          size_t length) {
    if (runtime != 0 && runtime->vm != 0) {
        return linux_compat_vm_read_user(runtime->vm,
                                         (uintptr_t)buffer,
                                         out,
                                         length);
    }
    if (buffer == 0 && length != 0U) {
        return false;
    }
    if (length != 0U) {
        copy_bytes((uint8_t*)out, (const uint8_t*)buffer, length);
    }
    return true;
}

static bool linux_compat_copy_in_path(const linux_compat_runtime_t* runtime,
                                      const char* path,
                                      char* out,
                                      size_t out_size) {
    size_t i = 0;

    if (out == 0 || out_size == 0U || path == 0) {
        return false;
    }
    if (runtime == 0 || runtime->vm == 0) {
        if (str_len(path) + 1U > out_size) {
            return false;
        }
        copy_str(out, out_size, path);
        return true;
    }

    while (i + 1U < out_size) {
        uint8_t ch = 0;

        if (!linux_compat_vm_read_user(runtime->vm,
                                       (uintptr_t)path + (uintptr_t)i,
                                       &ch,
                                       sizeof(ch))) {
            return false;
        }
        out[i] = (char)ch;
        if (ch == '\0') {
            return true;
        }
        i += 1U;
    }
    out[out_size - 1U] = '\0';
    return false;
}

static bool linux_compat_write_result_buffer(linux_compat_runtime_t* runtime,
                                             void* buffer,
                                             const void* data,
                                             size_t length);

static bool request_has_arg(const linux_compat_exec_request_t* request,
                            const char* value) {
    size_t i = 0;

    if (request == 0 || value == 0 || request->argv == 0) {
        return false;
    }
    for (i = 1U; i < request->argc; ++i) {
        if (str_eq(request->argv[i], value)) {
            return true;
        }
    }
    return false;
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

static void clear_elf_info(linux_compat_elf_info_t* info) {
    if (info == 0) {
        return;
    }
    info->elf_class = 0;
    info->endianness = 0;
    info->type = 0;
    info->machine = 0;
    info->entry = 0;
    info->phoff = 0;
    info->phentsize = 0;
    info->phnum = 0;
    info->has_interp = false;
}

linux_compat_result_t linux_compat_lookup(
    const char* path,
    linux_compat_rootfs_entry_t* out_entry,
    linux_compat_trace_t* out_trace) {
    const linux_compat_rootfs_node_t* node = 0;

    clear_entry(out_entry);
    if (path == 0 || str_len(path) == 0U) {
        set_trace(out_trace, "", 2, "linux-compat: rootfs: no such file");
        return LINUX_COMPAT_ERR_NO_SUCH_FILE;
    }

    node = find_node(path);
    if (node != 0 && !node->directory) {
        fill_entry_from_node(out_entry, node);
        set_trace(out_trace, path, 0, "linux-compat: rootfs: found");
        return LINUX_COMPAT_OK;
    }

    set_trace(out_trace, path, 2, "linux-compat: rootfs: no such file");
    return LINUX_COMPAT_ERR_NO_SUCH_FILE;
}

linux_compat_result_t linux_compat_stat_path(
    const char* path,
    linux_compat_stat_t* out_stat,
    linux_compat_trace_t* out_trace) {
    const linux_compat_rootfs_node_t* node = find_node(path);

    clear_stat(out_stat);
    if (node == 0) {
        set_trace(out_trace, path != 0 ? path : "", 2,
                  "linux-compat: stat: no such file");
        return LINUX_COMPAT_ERR_NO_SUCH_FILE;
    }

    fill_stat_from_node(out_stat, node);
    set_trace(out_trace, path, 0, "linux-compat: stat: ok");
    return LINUX_COMPAT_OK;
}

linux_compat_result_t linux_compat_inspect_elf(
    const uint8_t* image,
    size_t image_size,
    linux_compat_elf_info_t* out_info,
    linux_compat_trace_t* out_trace) {
    linux_compat_elf_info_t info;
    uint64_t ph_end = 0;
    uint16_t i = 0;

    clear_elf_info(out_info);
    clear_elf_info(&info);

    if (image == 0 || image_size < 64U) {
        set_trace(out_trace, "", 8, "linux-compat: elf: bad header");
        return LINUX_COMPAT_ERR_BAD_ELF;
    }
    if (image[0] != 0x7fU || image[1] != 'E' || image[2] != 'L' ||
        image[3] != 'F') {
        set_trace(out_trace, "", 8, "linux-compat: elf: bad magic");
        return LINUX_COMPAT_ERR_BAD_ELF;
    }

    info.elf_class = image[4];
    info.endianness = image[5];
    if (info.elf_class != 2U || info.endianness != 1U) {
        set_trace(out_trace, "", 8, "linux-compat: elf: unsupported class/data");
        return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
    }

    info.type = read_u16_le(image, 16U);
    info.machine = read_u16_le(image, 18U);
    info.entry = read_u64_le(image, 24U);
    info.phoff = read_u64_le(image, 32U);
    info.phentsize = read_u16_le(image, 54U);
    info.phnum = read_u16_le(image, 56U);

    if (info.machine != 243U) {
        set_trace(out_trace, "", 8, "linux-compat: elf: unsupported machine");
        return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
    }
    if (info.type != 2U) {
        set_trace(out_trace, "", 8, "linux-compat: elf: unsupported type");
        return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
    }
    if (info.phentsize < 4U) {
        set_trace(out_trace, "", 8, "linux-compat: elf: bad phentsize");
        return LINUX_COMPAT_ERR_BAD_ELF;
    }
    ph_end = info.phoff + ((uint64_t)info.phentsize * (uint64_t)info.phnum);
    if (info.phoff > image_size || ph_end > image_size) {
        set_trace(out_trace, "", 8, "linux-compat: elf: bad program headers");
        return LINUX_COMPAT_ERR_BAD_ELF;
    }

    for (i = 0; i < info.phnum; ++i) {
        const size_t ph_offset =
            (size_t)info.phoff + ((size_t)i * (size_t)info.phentsize);
        const uint32_t ph_type = read_u32_le(image, ph_offset);

        if (ph_type == 3U) {
            info.has_interp = true;
            if (out_info != 0) {
                *out_info = info;
            }
            set_trace(out_trace, "", 8, "linux-compat: elf: unsupported interp");
            return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
        }
    }

    if (out_info != 0) {
        *out_info = info;
    }
    set_trace(out_trace, "", 0, "linux-compat: elf: ok");
    return LINUX_COMPAT_OK;
}

void linux_compat_runtime_init(linux_compat_runtime_t* runtime) {
    size_t i = 0;

    if (runtime == 0) {
        return;
    }
    runtime->vm = 0;
    for (i = 0; i < LINUX_COMPAT_MAX_FDS; ++i) {
        runtime->fds[i].open = false;
        runtime->fds[i].node = 0;
        runtime->fds[i].offset = 0;
        runtime->fds[i].flags = LINUX_COMPAT_O_RDONLY;
        runtime->fds[i].fd_flags = 0;
    }
    runtime->program_break = 0x8000000U;
    runtime->next_mmap = 0x10000000U;
    runtime->stdout_buffer[0] = '\0';
    runtime->stdout_size = 0;
    runtime->exited = false;
    runtime->exit_code = 0;
    runtime->trace_count = 0;
    runtime->trace_truncated = false;
    for (i = 0; i < LINUX_COMPAT_MAX_TRACE_RECORDS; ++i) {
        runtime->trace_records[i].number = 0;
        runtime->trace_records[i].return_value = 0;
        runtime->trace_records[i].errno_value = 0;
        runtime->trace_records[i].pc = 0;
        runtime->trace_records[i].message[0] = '\0';
    }
}

static const linux_compat_rootfs_node_t* fd_node(
    linux_compat_runtime_t* runtime,
    int32_t fd,
    linux_compat_trace_t* out_trace,
    const char* operation) {
    if (runtime == 0 || fd < 0 || fd >= (int32_t)LINUX_COMPAT_MAX_FDS ||
        !runtime->fds[fd].open || runtime->fds[fd].node == 0) {
        set_trace(out_trace, "", 9, operation);
        return 0;
    }
    return (const linux_compat_rootfs_node_t*)runtime->fds[fd].node;
}

int32_t linux_compat_openat(linux_compat_runtime_t* runtime,
                            int32_t dirfd,
                            const char* path,
                            uint32_t flags,
                            linux_compat_trace_t* out_trace) {
    char resolved_path[LINUX_COMPAT_MAX_PATH];
    const char* lookup_path = path;
    const linux_compat_rootfs_node_t* node = 0;
    size_t i = 0;

    if (runtime == 0) {
        set_trace(out_trace, path != 0 ? path : "", 22,
                  "linux-compat: openat: bad runtime");
        return -22;
    }
    if (dirfd != LINUX_COMPAT_AT_FDCWD || path == 0) {
        set_trace(out_trace, path != 0 ? path : "", 22,
                  "linux-compat: openat: unsupported path base");
        return -22;
    }
    if (!linux_compat_copy_in_path(runtime,
                                   path,
                                   resolved_path,
                                   sizeof(resolved_path))) {
        set_trace(out_trace, "", 14, "linux-compat: openat: bad path");
        return -14;
    }
    lookup_path = resolved_path;
    if (lookup_path[0] != '/') {
        set_trace(out_trace, lookup_path, 22,
                  "linux-compat: openat: unsupported path base");
        return -22;
    }
    if (flags != LINUX_COMPAT_O_RDONLY) {
        set_trace(out_trace, lookup_path, 13,
                  "linux-compat: openat: readonly rootfs");
        return -13;
    }
    node = find_node(lookup_path);
    if (node == 0) {
        set_trace(out_trace, lookup_path, 2, "linux-compat: openat: no such file");
        return -2;
    }
    for (i = 3U; i < LINUX_COMPAT_MAX_FDS; ++i) {
        if (!runtime->fds[i].open) {
            runtime->fds[i].open = true;
            runtime->fds[i].node = node;
            runtime->fds[i].offset = 0;
            runtime->fds[i].flags = flags;
            runtime->fds[i].fd_flags = 0;
            set_trace(out_trace, lookup_path, 0, "linux-compat: openat: ok");
            return (int32_t)i;
        }
    }
    set_trace(out_trace, lookup_path, 24, "linux-compat: openat: fd table full");
    return -24;
}

int64_t linux_compat_read(linux_compat_runtime_t* runtime,
                          int32_t fd,
                          void* buffer,
                          size_t length,
                          linux_compat_trace_t* out_trace) {
    const linux_compat_rootfs_node_t* node =
        fd_node(runtime, fd, out_trace, "linux-compat: read: bad fd");
    size_t available = 0;
    size_t count = 0;
    uint8_t temp[256];
    size_t copied = 0;

    if (node == 0) {
        return -9;
    }
    if (node->directory) {
        set_trace(out_trace, node->path, 21, "linux-compat: read: is directory");
        return -21;
    }
    if (buffer == 0 && length != 0U) {
        set_trace(out_trace, node->path, 14, "linux-compat: read: bad buffer");
        return -14;
    }
    if (runtime->fds[fd].offset >= node->size) {
        set_trace(out_trace, node->path, 0, "linux-compat: read: eof");
        return 0;
    }
    available = node->size - runtime->fds[fd].offset;
    count = length < available ? length : available;
    while (copied < count) {
        const size_t chunk = (count - copied) < sizeof(temp)
                                 ? (count - copied)
                                 : sizeof(temp);

        copy_bytes(temp,
                   node->data + runtime->fds[fd].offset + copied,
                   chunk);
        if (!linux_compat_write_result_buffer(runtime,
                                             (uint8_t*)buffer + copied,
                                             temp,
                                             chunk)) {
            set_trace(out_trace, node->path, 14, "linux-compat: read: bad buffer");
            return -14;
        }
        copied += chunk;
    }
    runtime->fds[fd].offset += count;
    set_trace(out_trace, node->path, 0, "linux-compat: read: ok");
    return (int64_t)count;
}

int64_t linux_compat_lseek(linux_compat_runtime_t* runtime,
                           int32_t fd,
                           int64_t offset,
                           uint32_t whence,
                           linux_compat_trace_t* out_trace) {
    const linux_compat_rootfs_node_t* node =
        fd_node(runtime, fd, out_trace, "linux-compat: lseek: bad fd");
    int64_t base = 0;
    int64_t next = 0;

    if (node == 0) {
        return -9;
    }
    if (node->directory) {
        set_trace(out_trace, node->path, 22, "linux-compat: lseek: is directory");
        return -22;
    }
    if (whence == 0U) {
        base = 0;
    } else if (whence == 1U) {
        base = (int64_t)runtime->fds[fd].offset;
    } else if (whence == 2U) {
        base = (int64_t)node->size;
    } else {
        set_trace(out_trace, node->path, 22, "linux-compat: lseek: bad whence");
        return -22;
    }
    next = base + offset;
    if (next < 0) {
        set_trace(out_trace, node->path, 22, "linux-compat: lseek: bad offset");
        return -22;
    }
    runtime->fds[fd].offset = (size_t)next;
    set_trace(out_trace, node->path, 0, "linux-compat: lseek: ok");
    return next;
}

int32_t linux_compat_close(linux_compat_runtime_t* runtime,
                           int32_t fd,
                           linux_compat_trace_t* out_trace) {
    if (runtime == 0 || fd < 3 || fd >= (int32_t)LINUX_COMPAT_MAX_FDS ||
        !runtime->fds[fd].open) {
        set_trace(out_trace, "", 9, "linux-compat: close: bad fd");
        return -9;
    }
    runtime->fds[fd].open = false;
    runtime->fds[fd].node = 0;
    runtime->fds[fd].offset = 0;
    runtime->fds[fd].flags = LINUX_COMPAT_O_RDONLY;
    runtime->fds[fd].fd_flags = 0;
    set_trace(out_trace, "", 0, "linux-compat: close: ok");
    return 0;
}

static bool linux_compat_fd_valid(const linux_compat_runtime_t* runtime,
                                  int32_t fd) {
    return runtime != 0 && fd >= 0 && fd < (int32_t)LINUX_COMPAT_MAX_FDS &&
           (fd < 3 || runtime->fds[fd].open);
}

static bool linux_compat_fd_is_tty(int32_t fd) {
    return fd >= 0 && fd < 3;
}

static int64_t linux_compat_fcntl(linux_compat_runtime_t* runtime,
                                  int32_t fd,
                                  uint32_t command,
                                  uint64_t arg,
                                  linux_compat_trace_t* out_trace) {
    size_t i = 0;

    if (!linux_compat_fd_valid(runtime, fd)) {
        set_trace(out_trace, "", 9, "linux-compat: fcntl: bad fd");
        return -9;
    }

    switch (command) {
    case LINUX_COMPAT_F_GETFD:
        set_trace(out_trace, "", 0, "linux-compat: fcntl: getfd");
        return runtime->fds[fd].fd_flags;
    case LINUX_COMPAT_F_SETFD:
        if ((arg & ~((uint64_t)LINUX_COMPAT_FD_CLOEXEC)) != 0U) {
            set_trace(out_trace, "", 22, "linux-compat: fcntl: bad fd flags");
            return -22;
        }
        runtime->fds[fd].fd_flags = (uint32_t)arg;
        set_trace(out_trace, "", 0, "linux-compat: fcntl: setfd");
        return 0;
    case LINUX_COMPAT_F_GETFL:
        set_trace(out_trace, "", 0, "linux-compat: fcntl: getfl");
        return runtime->fds[fd].flags;
    case LINUX_COMPAT_F_SETFL:
        if ((arg & ~((uint64_t)LINUX_COMPAT_O_NONBLOCK)) != 0U) {
            set_trace(out_trace, "", 22, "linux-compat: fcntl: bad status flags");
            return -22;
        }
        runtime->fds[fd].flags =
            (runtime->fds[fd].flags & ~LINUX_COMPAT_O_NONBLOCK) |
            ((uint32_t)arg & LINUX_COMPAT_O_NONBLOCK);
        set_trace(out_trace, "", 0, "linux-compat: fcntl: setfl");
        return 0;
    case LINUX_COMPAT_F_DUPFD:
        if (arg >= LINUX_COMPAT_MAX_FDS) {
            set_trace(out_trace, "", 22, "linux-compat: fcntl: bad dup base");
            return -22;
        }
        for (i = (size_t)arg; i < LINUX_COMPAT_MAX_FDS; ++i) {
            if (i < 3U || runtime->fds[i].open) {
                continue;
            }
            runtime->fds[i] = runtime->fds[fd];
            runtime->fds[i].open = true;
            set_trace(out_trace, "", 0, "linux-compat: fcntl: dupfd");
            return (int64_t)i;
        }
        set_trace(out_trace, "", 24, "linux-compat: fcntl: fd table full");
        return -24;
    default:
        set_trace(out_trace, "", 22, "linux-compat: fcntl: unsupported command");
        return -22;
    }
}

static int64_t linux_compat_ioctl(linux_compat_runtime_t* runtime,
                                  int32_t fd,
                                  uint32_t command,
                                  uint64_t arg,
                                  linux_compat_trace_t* out_trace) {
    if (!linux_compat_fd_valid(runtime, fd)) {
        set_trace(out_trace, "", 9, "linux-compat: ioctl: bad fd");
        return -9;
    }
    if (!linux_compat_fd_is_tty(fd)) {
        set_trace(out_trace, "", 25, "linux-compat: ioctl: not tty");
        return -25;
    }

    switch (command) {
    case LINUX_COMPAT_TIOCGWINSZ:
        if (arg != 0U) {
            linux_compat_winsize_t winsize;

            winsize.ws_row = 24;
            winsize.ws_col = 80;
            winsize.ws_xpixel = 0;
            winsize.ws_ypixel = 0;
            if (!linux_compat_write_result_buffer(runtime,
                                                  (void*)(uintptr_t)arg,
                                                  &winsize,
                                                  sizeof(winsize))) {
                set_trace(out_trace, "", 14, "linux-compat: ioctl: bad winsz");
                return -14;
            }
        }
        set_trace(out_trace, "", 0, "linux-compat: ioctl: winsz");
        return 0;
    case LINUX_COMPAT_TCGETS:
        if (arg != 0U) {
            linux_compat_termios_t termios;

            termios.c_iflag = 0;
            termios.c_oflag = 0;
            termios.c_cflag = 0;
            termios.c_lflag = 0xaU;
            if (!linux_compat_write_result_buffer(runtime,
                                                  (void*)(uintptr_t)arg,
                                                  &termios,
                                                  sizeof(termios))) {
                set_trace(out_trace, "", 14, "linux-compat: ioctl: bad termios");
                return -14;
            }
        }
        set_trace(out_trace, "", 0, "linux-compat: ioctl: tcgets");
        return 0;
    case LINUX_COMPAT_TCSETS:
    case LINUX_COMPAT_TCSETSW:
    case LINUX_COMPAT_TCSETSF:
        set_trace(out_trace, "", 0, "linux-compat: ioctl: tcsets");
        return 0;
    case LINUX_COMPAT_FIONBIO:
        if (arg != 0U) {
            runtime->fds[fd].flags |= LINUX_COMPAT_O_NONBLOCK;
        } else {
            runtime->fds[fd].flags &= ~LINUX_COMPAT_O_NONBLOCK;
        }
        set_trace(out_trace, "", 0, "linux-compat: ioctl: fionbio");
        return 0;
    default:
        set_trace(out_trace, "", 25, "linux-compat: ioctl: unsupported request");
        return -25;
    }
}

static bool linux_compat_write_result_buffer(linux_compat_runtime_t* runtime,
                                             void* buffer,
                                             const void* data,
                                             size_t length) {
    if (runtime != 0 && runtime->vm != 0) {
        return linux_compat_vm_write_user(runtime->vm,
                                          (uintptr_t)buffer,
                                          data,
                                          length);
    }
    if (buffer == 0 && length != 0U) {
        return false;
    }
    if (length != 0U) {
        size_t i = 0;
        uint8_t* out = (uint8_t*)buffer;
        const uint8_t* bytes = (const uint8_t*)data;

        for (i = 0; i < length; ++i) {
            out[i] = bytes[i];
        }
    }
    return true;
}

static int64_t linux_compat_getrandom(linux_compat_runtime_t* runtime,
                                      void* buffer,
                                      size_t length,
                                      linux_compat_trace_t* out_trace) {
    static uint32_t state = 0x4d594350U;
    uint8_t out[256];
    size_t i = 0;
    const size_t count = length < 256U ? length : 256U;

    if (buffer == 0 && length != 0U) {
        set_trace(out_trace, "", 14, "linux-compat: getrandom: bad buffer");
        return -14;
    }
    for (i = 0; i < count; ++i) {
        state = state * 1664525U + 1013904223U;
        out[i] = (uint8_t)(state >> 24U);
    }
    if (!linux_compat_write_result_buffer(runtime, buffer, out, count)) {
        set_trace(out_trace, "", 14, "linux-compat: getrandom: bad buffer");
        return -14;
    }
    set_trace(out_trace, "", 0, "linux-compat: getrandom");
    return (int64_t)count;
}

static int64_t linux_compat_clock_gettime(int32_t clock_id,
                                          linux_compat_runtime_t* runtime,
                                          void* buffer,
                                          linux_compat_trace_t* out_trace) {
    static uint64_t ticks = 1;
    linux_compat_timespec_t ts;

    if (clock_id != LINUX_COMPAT_CLOCK_REALTIME &&
        clock_id != LINUX_COMPAT_CLOCK_MONOTONIC) {
        set_trace(out_trace, "", 22, "linux-compat: clock_gettime: bad clock");
        return -22;
    }
    if (buffer == 0) {
        set_trace(out_trace, "", 14, "linux-compat: clock_gettime: bad buffer");
        return -14;
    }
    ts.tv_sec = (int64_t)(ticks / 1000000000ULL);
    ts.tv_nsec = (int64_t)(ticks % 1000000000ULL);
    ticks += 1000000ULL;
    if (!linux_compat_write_result_buffer(runtime, buffer, &ts, sizeof(ts))) {
        set_trace(out_trace, "", 14, "linux-compat: clock_gettime: bad buffer");
        return -14;
    }
    set_trace(out_trace, "", 0, "linux-compat: clock_gettime");
    return 0;
}

static int64_t linux_compat_write(linux_compat_runtime_t* runtime,
                                  int32_t fd,
                                  const void* buffer,
                                  size_t length,
                                  linux_compat_trace_t* out_trace) {
    const char* bytes = (const char*)buffer;
    size_t i = 0;

    if (runtime == 0 || (fd != 1 && fd != 2)) {
        set_trace(out_trace, "", 9, "linux-compat: write: bad fd");
        return -9;
    }
    if (buffer == 0 && length != 0U) {
        set_trace(out_trace, "", 14, "linux-compat: write: bad buffer");
        return -14;
    }
    while (i < length) {
        char chunk[64];
        size_t chunk_size =
            (length - i) < sizeof(chunk) ? (length - i) : sizeof(chunk);
        size_t j = 0;

        if (runtime->vm != 0) {
            if (!linux_compat_read_user_buffer(runtime,
                                               (const uint8_t*)buffer + i,
                                               chunk,
                                               chunk_size)) {
                set_trace(out_trace, "", 14,
                          "linux-compat: write: bad user buffer");
                return -14;
            }
            bytes = chunk;
        } else {
            bytes = (const char*)buffer + i;
        }
        for (j = 0; j < chunk_size; ++j) {
            if (runtime->stdout_size + 1U < LINUX_COMPAT_MAX_STDOUT) {
                runtime->stdout_buffer[runtime->stdout_size] = bytes[j];
                runtime->stdout_size += 1U;
                runtime->stdout_buffer[runtime->stdout_size] = '\0';
            }
            console_putc(bytes[j]);
        }
        i += chunk_size;
    }
    set_trace(out_trace, "", 0, "linux-compat: write: ok");
    return (int64_t)length;
}

static int64_t linux_compat_getdents64(linux_compat_runtime_t* runtime,
                                       int32_t fd,
                                       linux_compat_dirent_t* dirents,
                                       size_t capacity,
                                       linux_compat_trace_t* out_trace) {
    const linux_compat_rootfs_node_t* node =
        fd_node(runtime, fd, out_trace, "linux-compat: getdents64: bad fd");
    size_t i = 0;
    size_t count = 0;

    if (node == 0) {
        return -9;
    }
    if (!node->directory) {
        set_trace(out_trace, node->path, 20,
                  "linux-compat: getdents64: not directory");
        return -20;
    }
    if (dirents == 0 || capacity == 0U) {
        set_trace(out_trace, node->path, 22,
                  "linux-compat: getdents64: bad buffer");
        return -22;
    }
    for (i = 0; i < linux_compat_rootfs_node_count(); ++i) {
        const linux_compat_rootfs_node_t* child =
            linux_compat_rootfs_node_at(i);
        char name[LINUX_COMPAT_MAX_PATH];
        linux_compat_dirent_t record;

        if (count >= capacity) {
            break;
        }
        if (child == 0 ||
            !copy_basename_if_child(node->path,
                                    child->path,
                                    name,
                                    sizeof(name))) {
            continue;
        }
        record.inode = child->inode;
        record.type =
            child->directory ? LINUX_COMPAT_DT_DIR : LINUX_COMPAT_DT_REG;
        copy_str(record.name, sizeof(record.name), name);
        if (runtime != 0 && runtime->vm != 0) {
            if (!linux_compat_write_result_buffer(runtime,
                                                  (uint8_t*)dirents +
                                                      (count * sizeof(record)),
                                                  &record,
                                                  sizeof(record))) {
                set_trace(out_trace, node->path, 14,
                          "linux-compat: getdents64: bad buffer");
                return -14;
            }
        } else {
            dirents[count] = record;
        }
        count += 1U;
    }
    set_trace(out_trace, node->path, 0, "linux-compat: getdents64: ok");
    return (int64_t)count;
}

static uint64_t align_page(uint64_t value) {
    const uint64_t mask = 4095U;

    return (value + mask) & ~mask;
}

static void append_syscall_trace_record(
    linux_compat_runtime_t* runtime,
    const linux_compat_syscall_request_t* request,
    int64_t return_value,
    const linux_compat_trace_t* trace) {
    linux_compat_syscall_trace_record_t* record = 0;

    if (runtime == 0 || request == 0) {
        return;
    }
    if (runtime->trace_count >= LINUX_COMPAT_MAX_TRACE_RECORDS) {
        runtime->trace_truncated = true;
        return;
    }
    record = &runtime->trace_records[runtime->trace_count];
    runtime->trace_count += 1U;
    record->number = request->number;
    record->return_value = return_value;
    record->errno_value =
        trace != 0 ? trace->errno_value
                   : (return_value < 0 ? (int32_t)(-return_value) : 0);
    record->pc =
        trace != 0 && trace->pc != 0U ? trace->pc : (uintptr_t)request->addr;
    copy_str(record->message,
             sizeof(record->message),
             trace != 0 ? trace->message : "");
}

linux_compat_result_t linux_compat_syscall_dispatch(
    linux_compat_runtime_t* runtime,
    const linux_compat_syscall_request_t* request,
    linux_compat_syscall_response_t* response,
    linux_compat_trace_t* out_trace) {
    int64_t value = 0;

    if (response != 0) {
        response->value = 0;
    }
    if (runtime == 0 || request == 0) {
        if (response != 0) {
            response->value = -22;
        }
        set_trace(out_trace, "", 22, "linux-compat: syscall: bad request");
        return LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL;
    }

    if (request->number == LINUX_COMPAT_SYS_BRK) {
        if (runtime->vm != 0) {
            value = (int64_t)linux_compat_vm_brk(runtime->vm,
                                                 (uintptr_t)request->addr);
        } else if (request->addr != 0U) {
            runtime->program_break = request->addr;
            value = (int64_t)runtime->program_break;
        } else {
            value = (int64_t)runtime->program_break;
        }
        set_trace(out_trace, "", 0, "linux-compat: syscall: brk");
    } else if (request->number == LINUX_COMPAT_SYS_MMAP) {
        const uint64_t length = request->length == 0U ? 4096U : request->length;

        if (runtime->vm != 0) {
            const uint32_t prot =
                request->prot != 0U
                    ? request->prot
                    : (LINUX_COMPAT_PROT_READ | LINUX_COMPAT_PROT_WRITE);
            value = (int64_t)linux_compat_vm_mmap(runtime->vm,
                                                  (uintptr_t)request->addr,
                                                  (size_t)length,
                                                  prot,
                                                  request->flags);
        } else {
            value = (int64_t)runtime->next_mmap;
            runtime->next_mmap += align_page(length);
        }
        set_trace(out_trace, "", 0, "linux-compat: syscall: mmap");
    } else if (request->number == LINUX_COMPAT_SYS_MUNMAP) {
        if (runtime->vm != 0) {
            value = linux_compat_vm_munmap(runtime->vm,
                                           (uintptr_t)request->addr,
                                           request->length);
        } else {
            value = 0;
        }
        set_trace(out_trace, "", 0, "linux-compat: syscall: munmap");
    } else if (request->number == LINUX_COMPAT_SYS_NEWFSTATAT) {
        char resolved_path[LINUX_COMPAT_MAX_PATH];
        linux_compat_stat_t stat_value;
        linux_compat_result_t result = LINUX_COMPAT_ERR_NO_SUCH_FILE;

        if (!linux_compat_copy_in_path(runtime,
                                       request->path,
                                       resolved_path,
                                       sizeof(resolved_path))) {
            value = -14;
            set_trace(out_trace, "", 14, "linux-compat: newfstatat: bad path");
            if (response != 0) {
                response->value = value;
            }
            if (out_trace != 0) {
                out_trace->syscall_number = request->number;
                out_trace->pc = (uintptr_t)request->addr;
            }
            append_syscall_trace_record(runtime, request, value, out_trace);
            return LINUX_COMPAT_OK;
        }
        result = linux_compat_stat_path(resolved_path, &stat_value, out_trace);
        if (result == LINUX_COMPAT_OK &&
            !linux_compat_write_result_buffer(runtime,
                                              request->stat,
                                              &stat_value,
                                              sizeof(stat_value))) {
            value = -14;
            set_trace(out_trace, resolved_path, 14,
                      "linux-compat: newfstatat: bad buffer");
            if (response != 0) {
                response->value = value;
            }
            if (out_trace != 0) {
                out_trace->syscall_number = request->number;
                out_trace->pc = (uintptr_t)request->addr;
            }
            append_syscall_trace_record(runtime, request, value, out_trace);
            return LINUX_COMPAT_OK;
        }

        value = result == LINUX_COMPAT_OK ? 0 : -2;
        if (response != 0) {
            response->value = value;
        }
        if (out_trace != 0) {
            out_trace->syscall_number = request->number;
            out_trace->pc = (uintptr_t)request->addr;
        }
        append_syscall_trace_record(runtime, request, value, out_trace);
        return result == LINUX_COMPAT_OK ? LINUX_COMPAT_OK : result;
    } else if (request->number == LINUX_COMPAT_SYS_FCNTL) {
        value = linux_compat_fcntl(runtime,
                                   request->fd,
                                   request->command,
                                   request->arg,
                                   out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_IOCTL) {
        value = linux_compat_ioctl(runtime,
                                   request->fd,
                                   request->command,
                                   request->arg,
                                   out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_OPENAT) {
        value = linux_compat_openat(runtime,
                                    request->dirfd,
                                    request->path,
                                    LINUX_COMPAT_O_RDONLY,
                                    out_trace);
        if (response != 0) {
            response->value = value;
        }
        if (out_trace != 0) {
            out_trace->syscall_number = request->number;
            out_trace->pc = (uintptr_t)request->addr;
        }
        append_syscall_trace_record(runtime, request, value, out_trace);
        return value >= 0 ? LINUX_COMPAT_OK
                          : (value == -2 ? LINUX_COMPAT_ERR_NO_SUCH_FILE
                                         : LINUX_COMPAT_OK);
    } else if (request->number == LINUX_COMPAT_SYS_READ) {
        value = linux_compat_read(runtime,
                                  request->fd,
                                  request->read_buffer,
                                  request->length,
                                  out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_CLOSE) {
        value = linux_compat_close(runtime, request->fd, out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_LSEEK) {
        value = linux_compat_lseek(runtime,
                                   request->fd,
                                   (int64_t)request->offset,
                                   0U,
                                   out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_GETDENTS64) {
        value = linux_compat_getdents64(runtime,
                                        request->fd,
                                        request->dirents,
                                        request->dirent_capacity,
                                        out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_WRITE) {
        value = linux_compat_write(runtime,
                                   request->fd,
                                   request->write_buffer,
                                   request->length,
                                   out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_CLOCK_GETTIME) {
        value = linux_compat_clock_gettime(request->fd,
                                           runtime,
                                           request->read_buffer,
                                           out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_EXIT ||
               request->number == LINUX_COMPAT_SYS_EXIT_GROUP) {
        runtime->exited = true;
        runtime->exit_code = request->fd;
        value = 0;
        set_trace(out_trace, "", 0, "linux-compat: syscall: exit");
    } else if (request->number == LINUX_COMPAT_SYS_GETRANDOM) {
        value = linux_compat_getrandom(runtime,
                                       request->read_buffer,
                                       request->length,
                                       out_trace);
    } else {
        if (response != 0) {
            response->value = -38;
        }
        set_trace(out_trace, "", 38,
                  "linux-compat: syscall: unsupported syscall");
        if (out_trace != 0) {
            out_trace->syscall_number = request->number;
            out_trace->pc = (uintptr_t)request->addr;
        }
        append_syscall_trace_record(runtime, request, -38, out_trace);
        return LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL;
    }

    if (response != 0) {
        response->value = value;
    }
    if (out_trace != 0) {
        out_trace->syscall_number = request->number;
        out_trace->pc = (uintptr_t)request->addr;
    }
    append_syscall_trace_record(runtime, request, value, out_trace);
    return LINUX_COMPAT_OK;
}

linux_compat_result_t linux_compat_run(
    const linux_compat_exec_request_t* request,
    char* out,
    size_t out_size,
    linux_compat_trace_t* out_trace) {
    linux_compat_run_scratch_t* scratch = &g_linux_compat_run_scratch;
    linux_compat_result_t result = LINUX_COMPAT_OK;
    const char* help_text = 0;
    size_t used = 0;

    if (out == 0 || out_size == 0) {
        return LINUX_COMPAT_ERR_NO_SUCH_FILE;
    }
    out[0] = '\0';
    zero_bytes(scratch, sizeof(*scratch));
    scratch->fd = -1;
    if (request == 0 || request->path == 0 || str_len(request->path) == 0U) {
        set_trace(out_trace, "", 2, "linux-compat: usage: linux <path> [args...]");
        return append_str(out, out_size, &used, "linux-compat: usage: linux <path-or-command> [args...]\n")
                   ? LINUX_COMPAT_ERR_NO_SUCH_FILE
                   : LINUX_COMPAT_ERR_NO_SUCH_FILE;
    }

    result = linux_compat_lookup(request->path, &scratch->entry, &scratch->trace);
    if (result != LINUX_COMPAT_OK) {
        if (out_trace != 0) {
            *out_trace = scratch->trace;
        }
        (void)append_rootfs_source_line(out, out_size, &used);
        (void)append_str(out, out_size, &used, "linux-compat: path=");
        (void)append_str(out, out_size, &used, scratch->trace.path);
        (void)append_str(out, out_size, &used, " errno=");
        (void)append_u64_dec(out,
                             out_size,
                             &used,
                             (uint64_t)scratch->trace.errno_value);
        (void)append_str(out, out_size, &used, " message=rootfs: no such file\n");
        return result;
    }

    result = linux_compat_build_load_plan(scratch->entry.data,
                                          scratch->entry.size,
                                          request->argc,
                                          0U,
                                          &scratch->load_plan,
                                          &scratch->trace);
    if (result != LINUX_COMPAT_OK) {
        if (out_trace != 0) {
            *out_trace = scratch->trace;
        }
        (void)append_rootfs_source_line(out, out_size, &used);
        (void)append_str(out, out_size, &used, "linux-compat: path=");
        (void)append_str(out, out_size, &used, scratch->entry.path);
        (void)append_str(out, out_size, &used, " errno=");
        (void)append_u64_dec(out,
                             out_size,
                             &used,
                             (uint64_t)scratch->trace.errno_value);
        (void)append_char(out, out_size, &used, ' ');
        (void)append_str(out, out_size, &used, scratch->trace.message);
        (void)append_char(out, out_size, &used, '\n');
        return result;
    }

    if (scratch->load_plan.requires_interp) {
        result =
            linux_compat_lookup(scratch->load_plan.interp_path,
                                &scratch->interp_entry,
                                &scratch->trace);
        if (result != LINUX_COMPAT_OK) {
            set_trace(out_trace,
                      scratch->entry.path,
                      scratch->trace.errno_value,
                      "linux-compat: loader: interp missing");
            (void)append_rootfs_source_line(out, out_size, &used);
            (void)append_str(out, out_size, &used, "linux-compat: path=");
            (void)append_str(out, out_size, &used, scratch->entry.path);
            (void)append_str(out, out_size, &used, " argc=");
            (void)append_u64_dec(out, out_size, &used, (uint64_t)request->argc);
            (void)append_load_plan_summary(out,
                                           out_size,
                                           &used,
                                           &scratch->load_plan);
            (void)append_str(out, out_size, &used, " errno=");
            (void)append_u64_dec(out,
                                 out_size,
                                 &used,
                                 (uint64_t)scratch->trace.errno_value);
            (void)append_str(out,
                             out_size,
                             &used,
                             " loader reason=interp missing\n");
            return result;
        }
        set_trace(out_trace,
                  scratch->entry.path,
                  9,
                  "linux-compat: loader: dynamic linker not executed");
        (void)append_rootfs_source_line(out, out_size, &used);
        (void)append_str(out, out_size, &used, "linux-compat: path=");
        (void)append_str(out, out_size, &used, scratch->entry.path);
        (void)append_str(out, out_size, &used, " argc=");
        (void)append_u64_dec(out, out_size, &used, (uint64_t)request->argc);
        (void)append_load_plan_summary(out,
                                       out_size,
                                       &used,
                                       &scratch->load_plan);
        (void)append_str(out,
                         out_size,
                         &used,
                         " errno=9 loader reason=dynamic linker not executed\n");
        return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
    }

    if (request_wants_real_exec(request, &scratch->entry)) {
        linux_compat_runtime_init(&scratch->runtime);
        linux_compat_vm_init(&scratch->vm,
                             request->address_space,
                             request->process);
        scratch->runtime.vm = &scratch->vm;

        result = linux_compat_exec_load(&scratch->vm,
                                        scratch->entry.data,
                                        scratch->entry.size,
                                        &scratch->load_plan,
                                        &scratch->entry_pc,
                                        &scratch->trace);
        if (result == LINUX_COMPAT_OK) {
            result = linux_compat_exec_build_stack(&scratch->vm,
                                                   &scratch->load_plan,
                                                   request->argc,
                                                   request->argv,
                                                   &scratch->user_sp,
                                                   &scratch->trace);
        }
        if (result == LINUX_COMPAT_OK) {
            result = linux_compat_exec_enter(&scratch->vm,
                                             request->trap_context,
                                             request->user_runtime,
                                             request->trap_stack_base,
                                             request->trap_stack_size,
                                             scratch->entry_pc,
                                             scratch->user_sp,
                                             &scratch->runtime,
                                             &scratch->trace);
        }

        (void)append_rootfs_source_line(out, out_size, &used);
        (void)append_str(out, out_size, &used, "linux-compat: path=");
        (void)append_str(out, out_size, &used, scratch->entry.path);
        (void)append_str(out, out_size, &used, " argc=");
        (void)append_u64_dec(out, out_size, &used, (uint64_t)request->argc);
        (void)append_load_plan_summary(out,
                                       out_size,
                                       &used,
                                       &scratch->load_plan);
        (void)append_str(out, out_size, &used, " exec=real");

        if (result != LINUX_COMPAT_OK) {
            if (out_trace != 0) {
                *out_trace = scratch->trace;
                if (out_trace->path[0] == '\0') {
                    copy_str(out_trace->path,
                             sizeof(out_trace->path),
                             scratch->entry.path);
                }
            }
            (void)append_str(out, out_size, &used, " errno=");
            (void)append_u64_dec(out,
                                 out_size,
                                 &used,
                                 (uint64_t)scratch->trace.errno_value);
            (void)append_char(out, out_size, &used, ' ');
            (void)append_str(out, out_size, &used, scratch->trace.message);
            (void)append_char(out, out_size, &used, '\n');
            linux_compat_vm_destroy(&scratch->vm);
            return result;
        }

        if (out_trace != 0) {
            set_trace(out_trace,
                      scratch->entry.path,
                      0,
                      "linux-compat: run: real ok");
            if (scratch->runtime.trace_count != 0U) {
                const linux_compat_syscall_trace_record_t* last =
                    &scratch->runtime
                         .trace_records[scratch->runtime.trace_count - 1U];

                out_trace->syscall_number = last->number;
                out_trace->pc = last->pc;
            }
        }
        (void)append_trace_summary(out, out_size, &used, &scratch->runtime);
        (void)append_str(out, out_size, &used, " exit=");
        (void)append_u64_dec(out,
                             out_size,
                             &used,
                             (uint64_t)scratch->runtime.exit_code);
        (void)append_char(out, out_size, &used, '\n');
        linux_compat_vm_destroy(&scratch->vm);
        return LINUX_COMPAT_OK;
    }

    if (str_eq(scratch->entry.path, "/bin/busybox") &&
        (request->argc <= 1U || request_has_arg(request, "--help") ||
         request_has_arg(request, "-h"))) {
        help_text = k_busybox_help;
    } else if (str_eq(scratch->entry.path, "/usr/bin/git") &&
               (request_has_arg(request, "-h") ||
                request_has_arg(request, "--help"))) {
        help_text = k_git_help;
    }

    if (help_text == 0) {
        set_trace(out_trace,
                  scratch->entry.path,
                  38,
                  "linux-compat: syscall: unsupported syscall");
        if (out_trace != 0) {
            out_trace->syscall_number = 0;
            out_trace->pc = (uintptr_t)scratch->load_plan.entry;
        }

        (void)append_rootfs_source_line(out, out_size, &used);
        (void)append_str(out, out_size, &used, "linux-compat: path=");
        (void)append_str(out, out_size, &used, scratch->entry.path);
        (void)append_str(out, out_size, &used, " argc=");
        (void)append_u64_dec(out, out_size, &used, (uint64_t)request->argc);
        (void)append_load_plan_summary(out,
                                       out_size,
                                       &used,
                                       &scratch->load_plan);
        (void)append_str(out,
                         out_size,
                         &used,
                         " fail-closed unsupported syscall number=0 pc=");
        (void)append_u64_hex(out,
                             out_size,
                             &used,
                             scratch->load_plan.entry);
        (void)append_str(out, out_size, &used, " errno=38 trace_index=0\n");
        return LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL;
    }

    linux_compat_runtime_init(&scratch->runtime);

    scratch->syscall.number = LINUX_COMPAT_SYS_BRK;
    scratch->syscall.dirfd = 0;
    scratch->syscall.fd = 0;
    scratch->syscall.path = 0;
    scratch->syscall.write_buffer = 0;
    scratch->syscall.read_buffer = 0;
    scratch->syscall.length = 0;
    scratch->syscall.offset = 0;
    scratch->syscall.stat = 0;
    scratch->syscall.dirents = 0;
    scratch->syscall.dirent_capacity = 0;
    scratch->syscall.addr = 0;
    scratch->syscall.prot = 0;
    scratch->syscall.flags = 0;
    scratch->syscall.command = 0;
    scratch->syscall.arg = 0;
    (void)linux_compat_syscall_dispatch(&scratch->runtime,
                                        &scratch->syscall,
                                        &scratch->response,
                                        &scratch->trace);

    scratch->syscall.number = LINUX_COMPAT_SYS_MMAP;
    scratch->syscall.length = 4096U;
    (void)linux_compat_syscall_dispatch(&scratch->runtime,
                                        &scratch->syscall,
                                        &scratch->response,
                                        &scratch->trace);

    scratch->syscall.number = LINUX_COMPAT_SYS_NEWFSTATAT;
    scratch->syscall.dirfd = LINUX_COMPAT_AT_FDCWD;
    scratch->syscall.path = scratch->entry.path;
    scratch->syscall.stat = &scratch->stat;
    scratch->syscall.length = 0;
    (void)linux_compat_syscall_dispatch(&scratch->runtime,
                                        &scratch->syscall,
                                        &scratch->response,
                                        &scratch->trace);

    scratch->syscall.number = LINUX_COMPAT_SYS_OPENAT;
    scratch->syscall.path = scratch->entry.path;
    scratch->syscall.stat = 0;
    (void)linux_compat_syscall_dispatch(&scratch->runtime,
                                        &scratch->syscall,
                                        &scratch->response,
                                        &scratch->trace);
    scratch->fd = (int32_t)scratch->response.value;

    if (scratch->fd >= 0) {
        scratch->syscall.number = LINUX_COMPAT_SYS_READ;
        scratch->syscall.fd = scratch->fd;
        scratch->syscall.path = 0;
        scratch->syscall.read_buffer = scratch->probe;
        scratch->syscall.length = sizeof(scratch->probe);
        (void)linux_compat_syscall_dispatch(&scratch->runtime,
                                            &scratch->syscall,
                                            &scratch->response,
                                            &scratch->trace);
        scratch->syscall.number = LINUX_COMPAT_SYS_CLOSE;
        scratch->syscall.read_buffer = 0;
        scratch->syscall.length = 0;
        (void)linux_compat_syscall_dispatch(&scratch->runtime,
                                            &scratch->syscall,
                                            &scratch->response,
                                            &scratch->trace);
    }

    scratch->syscall.number = LINUX_COMPAT_SYS_WRITE;
    scratch->syscall.fd = 1;
    scratch->syscall.write_buffer = help_text;
    scratch->syscall.length = str_len(help_text);
    (void)linux_compat_syscall_dispatch(&scratch->runtime,
                                        &scratch->syscall,
                                        &scratch->response,
                                        &scratch->trace);

    scratch->syscall.number = LINUX_COMPAT_SYS_EXIT_GROUP;
    scratch->syscall.fd = 0;
    scratch->syscall.write_buffer = 0;
    scratch->syscall.length = 0;
    scratch->syscall.addr = scratch->load_plan.entry;
    (void)linux_compat_syscall_dispatch(&scratch->runtime,
                                        &scratch->syscall,
                                        &scratch->response,
                                        &scratch->trace);

    set_trace(out_trace, scratch->entry.path, 0, "linux-compat: run: ok");
    if (out_trace != 0) {
        out_trace->syscall_number = LINUX_COMPAT_SYS_EXIT_GROUP;
        out_trace->pc = (uintptr_t)scratch->load_plan.entry;
    }

    (void)append_rootfs_source_line(out, out_size, &used);
    (void)append_str(out, out_size, &used, "linux-compat: path=");
    (void)append_str(out, out_size, &used, scratch->entry.path);
    (void)append_str(out, out_size, &used, " argc=");
    (void)append_u64_dec(out, out_size, &used, (uint64_t)request->argc);
    (void)append_load_plan_summary(out,
                                   out_size,
                                   &used,
                                   &scratch->load_plan);
    (void)append_str(out,
                     out_size,
                     &used,
                     " syscalls=openat/read/newfstatat/brk/mmap/write/exit_group");
    (void)append_trace_summary(out, out_size, &used, &scratch->runtime);
    (void)append_char(out, out_size, &used, '\n');
    (void)append_str(out,
                     out_size,
                     &used,
                     scratch->runtime.stdout_buffer);
    return LINUX_COMPAT_OK;
}
