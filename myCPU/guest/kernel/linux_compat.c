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

typedef struct LinuxCompatRunScratch {
    linux_compat_rootfs_entry_t entry;
    linux_compat_rootfs_entry_t interp_entry;
    linux_compat_load_plan_t load_plan;
    linux_compat_load_plan_t interp_plan;
    linux_compat_vm_t vm;
    linux_compat_runtime_t runtime;
    linux_compat_trace_t trace;
    uintptr_t entry_pc;
    uintptr_t interp_entry_pc;
    uintptr_t user_sp;
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
           entry->executable;
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
        case LINUX_COMPAT_SYS_MKDIRAT:
            return "mkdirat";
        case LINUX_COMPAT_SYS_UNLINKAT:
            return "unlinkat";
        case LINUX_COMPAT_SYS_RENAMEAT:
            return "renameat";
        case LINUX_COMPAT_SYS_FTRUNCATE:
            return "ftruncate";
        case LINUX_COMPAT_SYS_FACCESSAT:
            return "faccessat";
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
        case LINUX_COMPAT_SYS_WRITEV:
            return "writev";
        case LINUX_COMPAT_SYS_PREAD64:
            return "pread64";
        case LINUX_COMPAT_SYS_PWRITE64:
            return "pwrite64";
        case LINUX_COMPAT_SYS_READLINKAT:
            return "readlinkat";
        case LINUX_COMPAT_SYS_NEWFSTATAT:
            return "newfstatat";
        case LINUX_COMPAT_SYS_FSTAT:
            return "fstat";
        case LINUX_COMPAT_SYS_SYNC:
            return "sync";
        case LINUX_COMPAT_SYS_FSYNC:
            return "fsync";
        case LINUX_COMPAT_SYS_FDATASYNC:
            return "fdatasync";
        case LINUX_COMPAT_SYS_CLOCK_GETTIME:
            return "clock_gettime";
        case LINUX_COMPAT_SYS_EXIT:
            return "exit";
        case LINUX_COMPAT_SYS_EXIT_GROUP:
            return "exit_group";
        case LINUX_COMPAT_SYS_SET_TID_ADDRESS:
            return "set_tid_address";
        case LINUX_COMPAT_SYS_SET_ROBUST_LIST:
            return "set_robust_list";
        case LINUX_COMPAT_SYS_RT_SIGACTION:
            return "rt_sigaction";
        case LINUX_COMPAT_SYS_RT_SIGPROCMASK:
            return "rt_sigprocmask";
        case LINUX_COMPAT_SYS_UNAME:
            return "uname";
        case LINUX_COMPAT_SYS_BRK:
            return "brk";
        case LINUX_COMPAT_SYS_MUNMAP:
            return "munmap";
        case LINUX_COMPAT_SYS_MMAP:
            return "mmap";
        case LINUX_COMPAT_SYS_MPROTECT:
            return "mprotect";
        case LINUX_COMPAT_SYS_PRLIMIT64:
            return "prlimit64";
        case LINUX_COMPAT_SYS_RENAMEAT2:
            return "renameat2";
        case LINUX_COMPAT_SYS_GETRANDOM:
            return "getrandom";
        case LINUX_COMPAT_SYS_STATX:
            return "statx";
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

static bool rebase_load_plan(linux_compat_load_plan_t* plan,
                             uint64_t new_load_bias) {
    uint64_t old_load_bias = 0;
    size_t i = 0;

    if (plan == 0 || plan->elf_type != 3U) {
        return false;
    }
    old_load_bias = plan->load_bias;
    if (new_load_bias == old_load_bias) {
        return true;
    }
    if (plan->entry < old_load_bias) {
        return false;
    }
    plan->entry = (plan->entry - old_load_bias) + new_load_bias;
    for (i = 0; i < plan->segment_count; ++i) {
        if (plan->segments[i].vaddr < old_load_bias) {
            return false;
        }
        plan->segments[i].vaddr =
            (plan->segments[i].vaddr - old_load_bias) + new_load_bias;
    }
    plan->load_bias = new_load_bias;
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

static bool path_contains_slash(const char* value) {
    size_t i = 0;

    if (value == 0) {
        return false;
    }
    while (value[i] != '\0') {
        if (value[i] == '/') {
            return true;
        }
        i += 1U;
    }
    return false;
}

static bool fallback_name_allowed(const char* value) {
    return str_eq(value, "busybox") || str_eq(value, "git") ||
           str_eq(value, "vim") || str_eq(value, "gcc") ||
           str_eq(value, "rustc");
}

static const char* fallback_canonical_prefix(const char* value) {
    if (str_eq(value, "busybox")) {
        return "/bin/";
    }
    if (str_eq(value, "git") || str_eq(value, "vim") ||
        str_eq(value, "gcc") || str_eq(value, "rustc")) {
        return "/usr/bin/";
    }
    return 0;
}

static bool build_path_candidate(char* out,
                                 size_t out_size,
                                 const char* prefix,
                                 const char* name) {
    size_t used = 0;

    if (out == 0 || out_size == 0 || prefix == 0 || name == 0) {
        return false;
    }
    out[0] = '\0';
    return append_str(out, out_size, &used, prefix) &&
           append_str(out, out_size, &used, name);
}

linux_compat_result_t linux_compat_resolve_path(
    const char* command,
    char* out_path,
    size_t out_path_size,
    linux_compat_trace_t* out_trace) {
    static const char* k_path_prefixes[] = {"/bin/", "/usr/bin/"};
    linux_compat_rootfs_entry_t entry;
    linux_compat_trace_t trace;
    size_t i = 0;

    if (out_path != 0 && out_path_size != 0U) {
        out_path[0] = '\0';
    }
    if (command == 0 || str_len(command) == 0U || out_path == 0 ||
        out_path_size == 0U) {
        set_trace(out_trace, "", 2, "linux-compat: path: empty");
        return LINUX_COMPAT_ERR_NO_SUCH_FILE;
    }

    if (path_contains_slash(command)) {
        const linux_compat_result_t result =
            linux_compat_lookup(command, &entry, &trace);

        if (result != LINUX_COMPAT_OK) {
            if (out_trace != 0) {
                *out_trace = trace;
            }
            return result;
        }
        copy_str(out_path, out_path_size, entry.path);
        set_trace(out_trace, entry.path, 0, "linux-compat: path: explicit");
        return LINUX_COMPAT_OK;
    }

    if (!fallback_name_allowed(command)) {
        set_trace(out_trace, command, 2, "linux-compat: path: no fallback");
        return LINUX_COMPAT_ERR_NO_SUCH_FILE;
    }

    for (i = 0; i < sizeof(k_path_prefixes) / sizeof(k_path_prefixes[0]); ++i) {
        char candidate[LINUX_COMPAT_MAX_PATH];
        const linux_compat_result_t result =
            build_path_candidate(candidate,
                                 sizeof(candidate),
                                 k_path_prefixes[i],
                                 command)
                ? linux_compat_lookup(candidate, &entry, &trace)
                : LINUX_COMPAT_ERR_NO_SUCH_FILE;

        if (result == LINUX_COMPAT_OK) {
            copy_str(out_path, out_path_size, entry.path);
            set_trace(out_trace,
                      entry.path,
                      0,
                      "linux-compat: path: fallback");
            return LINUX_COMPAT_OK;
        }
    }

    if (build_path_candidate(out_path,
                             out_path_size,
                             fallback_canonical_prefix(command),
                             command)) {
        set_trace(out_trace, out_path, 0, "linux-compat: path: fallback");
        return LINUX_COMPAT_OK;
    }

    set_trace(out_trace, command, 2, "linux-compat: path: no such file");
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
        runtime->fds[i].overlay_node = false;
    }
    runtime->program_break = 0x8000000U;
    runtime->next_mmap = 0x10000000U;
    runtime->stdout_buffer[0] = '\0';
    runtime->stdout_size = 0;
    runtime->exited = false;
    runtime->exit_code = 0;
    runtime->next_overlay_inode = 0x100000U;
    runtime->next_overlay_mtime = 1U;
    for (i = 0; i < LINUX_COMPAT_MAX_OVERLAY_NODES; ++i) {
        runtime->overlay_nodes[i].used = false;
        runtime->overlay_nodes[i].directory = false;
        runtime->overlay_nodes[i].executable = false;
        runtime->overlay_nodes[i].dirty = false;
        runtime->overlay_nodes[i].inode = 0;
        runtime->overlay_nodes[i].mode = 0;
        runtime->overlay_nodes[i].mtime = 0;
        runtime->overlay_nodes[i].path[0] = '\0';
        runtime->overlay_nodes[i].size = 0;
        zero_bytes(runtime->overlay_nodes[i].data,
                   sizeof(runtime->overlay_nodes[i].data));
    }
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

static uint32_t linux_compat_file_mode(bool executable) {
    uint32_t mode = LINUX_COMPAT_S_IFREG | LINUX_COMPAT_S_IRUSR |
                    LINUX_COMPAT_S_IWUSR | LINUX_COMPAT_S_IRGRP |
                    LINUX_COMPAT_S_IROTH;

    if (executable) {
        mode |= LINUX_COMPAT_S_IXUSR | LINUX_COMPAT_S_IXGRP |
                LINUX_COMPAT_S_IXOTH;
    }
    return mode;
}

static uint32_t linux_compat_dir_mode(void) {
    return LINUX_COMPAT_S_IFDIR | LINUX_COMPAT_S_IRUSR |
           LINUX_COMPAT_S_IWUSR | LINUX_COMPAT_S_IXUSR |
           LINUX_COMPAT_S_IRGRP | LINUX_COMPAT_S_IXGRP |
           LINUX_COMPAT_S_IROTH | LINUX_COMPAT_S_IXOTH;
}

static bool linux_compat_open_flags_supported(uint32_t flags) {
    const uint32_t supported = LINUX_COMPAT_O_ACCMODE | LINUX_COMPAT_O_CREAT |
                               LINUX_COMPAT_O_TRUNC |
                               LINUX_COMPAT_O_NONBLOCK;
    return (flags & ~supported) == 0U &&
           (flags & LINUX_COMPAT_O_ACCMODE) <= LINUX_COMPAT_O_RDWR;
}

static bool linux_compat_flags_writable(uint32_t flags) {
    const uint32_t access = flags & LINUX_COMPAT_O_ACCMODE;

    return access == LINUX_COMPAT_O_WRONLY || access == LINUX_COMPAT_O_RDWR;
}

static bool linux_compat_flags_readable(uint32_t flags) {
    const uint32_t access = flags & LINUX_COMPAT_O_ACCMODE;

    return access == LINUX_COMPAT_O_RDONLY || access == LINUX_COMPAT_O_RDWR;
}

static bool linux_compat_split_parent(const char* path,
                                      char* out_parent,
                                      size_t parent_size,
                                      char* out_name,
                                      size_t name_size) {
    size_t len = str_len(path);
    size_t slash = 0;
    size_t i = 0;

    if (path == 0 || path[0] != '/' || len <= 1U ||
        path[len - 1U] == '/' || out_parent == 0 || parent_size == 0U ||
        out_name == 0 || name_size == 0U) {
        return false;
    }
    for (i = 1U; i < len; ++i) {
        if (path[i] == '/') {
            slash = i;
        }
    }
    if (slash == 0U) {
        copy_str(out_parent, parent_size, "/");
        copy_str(out_name, name_size, path + 1U);
        return str_len(out_name) != 0U;
    }
    if (slash + 1U >= len || slash + 1U >= parent_size ||
        len - slash > name_size) {
        return false;
    }
    for (i = 0; i < slash; ++i) {
        out_parent[i] = path[i];
    }
    out_parent[slash] = '\0';
    copy_str(out_name, name_size, path + slash + 1U);
    return str_len(out_name) != 0U;
}

static linux_compat_overlay_node_t* find_overlay_node(
    linux_compat_runtime_t* runtime,
    const char* path) {
    size_t i = 0;

    if (runtime == 0 || path == 0) {
        return 0;
    }
    for (i = 0; i < LINUX_COMPAT_MAX_OVERLAY_NODES; ++i) {
        linux_compat_overlay_node_t* node = &runtime->overlay_nodes[i];

        if (node->used && str_eq(node->path, path)) {
            return node;
        }
    }
    return 0;
}

static bool path_is_directory(linux_compat_runtime_t* runtime,
                              const char* path) {
    linux_compat_overlay_node_t* overlay = find_overlay_node(runtime, path);
    const linux_compat_rootfs_node_t* lower = 0;

    if (overlay != 0) {
        return overlay->directory;
    }
    lower = find_node(path);
    return lower != 0 && lower->directory;
}

static bool path_exists(linux_compat_runtime_t* runtime, const char* path) {
    return find_overlay_node(runtime, path) != 0 || find_node(path) != 0;
}

static bool parent_directory_exists(linux_compat_runtime_t* runtime,
                                    const char* path) {
    char parent[LINUX_COMPAT_MAX_PATH];
    char name[LINUX_COMPAT_MAX_PATH];

    if (!linux_compat_split_parent(path,
                                   parent,
                                   sizeof(parent),
                                   name,
                                   sizeof(name))) {
        return false;
    }
    return path_is_directory(runtime, parent);
}

static linux_compat_overlay_node_t* alloc_overlay_node(
    linux_compat_runtime_t* runtime,
    const char* path,
    bool directory,
    uint32_t mode) {
    size_t i = 0;

    if (runtime == 0 || path == 0 || str_len(path) >= LINUX_COMPAT_MAX_PATH) {
        return 0;
    }
    for (i = 0; i < LINUX_COMPAT_MAX_OVERLAY_NODES; ++i) {
        linux_compat_overlay_node_t* node = &runtime->overlay_nodes[i];

        if (node->used) {
            continue;
        }
        node->used = true;
        node->directory = directory;
        node->executable = (mode & LINUX_COMPAT_S_IXUSR) != 0U;
        node->dirty = true;
        node->inode = runtime->next_overlay_inode;
        runtime->next_overlay_inode += 1U;
        node->mode = mode;
        node->mtime = runtime->next_overlay_mtime;
        runtime->next_overlay_mtime += 1U;
        copy_str(node->path, sizeof(node->path), path);
        zero_bytes(node->data, sizeof(node->data));
        node->size = 0;
        return node;
    }
    return 0;
}

static void fill_stat_from_overlay(linux_compat_stat_t* stat,
                                   const linux_compat_overlay_node_t* node) {
    if (stat == 0) {
        return;
    }
    clear_stat(stat);
    if (node == 0) {
        return;
    }
    stat->inode = node->inode;
    stat->mode = node->mode;
    stat->size = node->directory ? 0U : node->size;
    stat->directory = node->directory;
    stat->executable = node->executable;
}

static linux_compat_result_t linux_compat_stat_path_runtime(
    linux_compat_runtime_t* runtime,
    const char* path,
    linux_compat_stat_t* out_stat,
    linux_compat_trace_t* out_trace) {
    linux_compat_overlay_node_t* overlay = find_overlay_node(runtime, path);
    const linux_compat_rootfs_node_t* lower = 0;

    clear_stat(out_stat);
    if (overlay != 0) {
        fill_stat_from_overlay(out_stat, overlay);
        set_trace(out_trace, path, 0, "linux-compat: stat: overlay");
        return LINUX_COMPAT_OK;
    }
    lower = find_node(path);
    if (lower == 0) {
        set_trace(out_trace, path != 0 ? path : "", 2,
                  "linux-compat: stat: no such file");
        return LINUX_COMPAT_ERR_NO_SUCH_FILE;
    }
    fill_stat_from_node(out_stat, lower);
    set_trace(out_trace, path, 0, "linux-compat: stat: ok");
    return LINUX_COMPAT_OK;
}

static bool fd_is_valid_file(linux_compat_runtime_t* runtime, int32_t fd) {
    return runtime != 0 && fd >= 3 && fd < (int32_t)LINUX_COMPAT_MAX_FDS &&
           runtime->fds[fd].open && runtime->fds[fd].node != 0;
}

static const char* fd_path(linux_compat_runtime_t* runtime, int32_t fd) {
    if (!fd_is_valid_file(runtime, fd)) {
        return "";
    }
    if (runtime->fds[fd].overlay_node) {
        return ((const linux_compat_overlay_node_t*)runtime->fds[fd].node)
            ->path;
    }
    return ((const linux_compat_rootfs_node_t*)runtime->fds[fd].node)->path;
}

static bool fd_is_directory(linux_compat_runtime_t* runtime, int32_t fd) {
    if (!fd_is_valid_file(runtime, fd)) {
        return false;
    }
    if (runtime->fds[fd].overlay_node) {
        return ((const linux_compat_overlay_node_t*)runtime->fds[fd].node)
            ->directory;
    }
    return ((const linux_compat_rootfs_node_t*)runtime->fds[fd].node)
        ->directory;
}

static size_t fd_size(linux_compat_runtime_t* runtime, int32_t fd) {
    if (!fd_is_valid_file(runtime, fd)) {
        return 0;
    }
    if (runtime->fds[fd].overlay_node) {
        return ((const linux_compat_overlay_node_t*)runtime->fds[fd].node)
            ->size;
    }
    return ((const linux_compat_rootfs_node_t*)runtime->fds[fd].node)->size;
}

static const uint8_t* fd_data(linux_compat_runtime_t* runtime, int32_t fd) {
    if (!fd_is_valid_file(runtime, fd)) {
        return 0;
    }
    if (runtime->fds[fd].overlay_node) {
        return ((const linux_compat_overlay_node_t*)runtime->fds[fd].node)
            ->data;
    }
    return ((const linux_compat_rootfs_node_t*)runtime->fds[fd].node)->data;
}

int32_t linux_compat_openat(linux_compat_runtime_t* runtime,
                            int32_t dirfd,
                            const char* path,
                            uint32_t flags,
                            linux_compat_trace_t* out_trace) {
    char resolved_path[LINUX_COMPAT_MAX_PATH];
    const char* lookup_path = path;
    linux_compat_overlay_node_t* overlay = 0;
    const linux_compat_rootfs_node_t* node = 0;
    const bool create = (flags & LINUX_COMPAT_O_CREAT) != 0U;
    const bool truncate = (flags & LINUX_COMPAT_O_TRUNC) != 0U;
    const bool writable = linux_compat_flags_writable(flags);
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
    if (!linux_compat_open_flags_supported(flags)) {
        set_trace(out_trace, lookup_path, 22,
                  "linux-compat: openat: unsupported flags");
        return -22;
    }
    overlay = find_overlay_node(runtime, lookup_path);
    node = overlay == 0 ? find_node(lookup_path) : 0;
    if (overlay == 0 && node == 0 && !create) {
        set_trace(out_trace, lookup_path, 2, "linux-compat: openat: no such file");
        return -2;
    }
    if (overlay == 0 && node == 0 && create) {
        if (!parent_directory_exists(runtime, lookup_path)) {
            set_trace(out_trace, lookup_path, 2,
                      "linux-compat: openat: parent missing");
            return -2;
        }
        overlay = alloc_overlay_node(runtime,
                                     lookup_path,
                                     false,
                                     linux_compat_file_mode(false));
        if (overlay == 0) {
            set_trace(out_trace, lookup_path, 28,
                      "linux-compat: openat: overlay full");
            return -28;
        }
    } else if (overlay == 0 && node != 0 && writable) {
        if (node->directory) {
            set_trace(out_trace, lookup_path, 21,
                      "linux-compat: openat: is directory");
            return -21;
        }
        overlay = alloc_overlay_node(runtime,
                                     lookup_path,
                                     false,
                                     node->mode);
        if (overlay == 0) {
            set_trace(out_trace, lookup_path, 28,
                      "linux-compat: openat: overlay full");
            return -28;
        }
        if (!truncate) {
            overlay->size = node->size < sizeof(overlay->data)
                                ? node->size
                                : sizeof(overlay->data);
            copy_bytes(overlay->data, node->data, overlay->size);
        }
    }
    if (overlay != 0 && overlay->directory && writable) {
        set_trace(out_trace, lookup_path, 21,
                  "linux-compat: openat: is directory");
        return -21;
    }
    if (overlay != 0 && truncate) {
        overlay->size = 0;
        overlay->dirty = true;
        overlay->mtime = runtime->next_overlay_mtime;
        runtime->next_overlay_mtime += 1U;
    }
    for (i = 3U; i < LINUX_COMPAT_MAX_FDS; ++i) {
        if (!runtime->fds[i].open) {
            runtime->fds[i].open = true;
            runtime->fds[i].node = overlay != 0 ? (const void*)overlay
                                                : (const void*)node;
            runtime->fds[i].offset = 0;
            runtime->fds[i].flags = flags;
            runtime->fds[i].fd_flags = 0;
            runtime->fds[i].overlay_node = overlay != 0;
            set_trace(out_trace,
                      lookup_path,
                      0,
                      overlay != 0 ? "linux-compat: openat: overlay"
                                   : "linux-compat: openat: ok");
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
    const char* path = fd_path(runtime, fd);
    const size_t node_size = fd_size(runtime, fd);
    const uint8_t* data = fd_data(runtime, fd);
    size_t available = 0;
    size_t count = 0;
    uint8_t temp[256];
    size_t copied = 0;

    if (!fd_is_valid_file(runtime, fd)) {
        set_trace(out_trace, "", 9, "linux-compat: read: bad fd");
        return -9;
    }
    if (!linux_compat_flags_readable(runtime->fds[fd].flags)) {
        set_trace(out_trace, path, 9, "linux-compat: read: fd not readable");
        return -9;
    }
    if (fd_is_directory(runtime, fd)) {
        set_trace(out_trace, path, 21, "linux-compat: read: is directory");
        return -21;
    }
    if (data == 0) {
        set_trace(out_trace, path, 9, "linux-compat: read: bad fd");
        return -9;
    }
    if (buffer == 0 && length != 0U) {
        set_trace(out_trace, path, 14, "linux-compat: read: bad buffer");
        return -14;
    }
    if (runtime->fds[fd].offset >= node_size) {
        set_trace(out_trace, path, 0, "linux-compat: read: eof");
        return 0;
    }
    available = node_size - runtime->fds[fd].offset;
    count = length < available ? length : available;
    while (copied < count) {
        const size_t chunk = (count - copied) < sizeof(temp)
                                 ? (count - copied)
                                 : sizeof(temp);

        copy_bytes(temp,
                   data + runtime->fds[fd].offset + copied,
                   chunk);
        if (!linux_compat_write_result_buffer(runtime,
                                             (uint8_t*)buffer + copied,
                                             temp,
                                             chunk)) {
            set_trace(out_trace, path, 14, "linux-compat: read: bad buffer");
            return -14;
        }
        copied += chunk;
    }
    runtime->fds[fd].offset += count;
    set_trace(out_trace, path, 0, "linux-compat: read: ok");
    return (int64_t)count;
}

int64_t linux_compat_lseek(linux_compat_runtime_t* runtime,
                           int32_t fd,
                           int64_t offset,
                           uint32_t whence,
                           linux_compat_trace_t* out_trace) {
    const char* path = fd_path(runtime, fd);
    int64_t base = 0;
    int64_t next = 0;

    if (!fd_is_valid_file(runtime, fd)) {
        set_trace(out_trace, "", 9, "linux-compat: lseek: bad fd");
        return -9;
    }
    if (fd_is_directory(runtime, fd)) {
        set_trace(out_trace, path, 22, "linux-compat: lseek: is directory");
        return -22;
    }
    if (whence == 0U) {
        base = 0;
    } else if (whence == 1U) {
        base = (int64_t)runtime->fds[fd].offset;
    } else if (whence == 2U) {
        base = (int64_t)fd_size(runtime, fd);
    } else {
        set_trace(out_trace, path, 22, "linux-compat: lseek: bad whence");
        return -22;
    }
    next = base + offset;
    if (next < 0) {
        set_trace(out_trace, path, 22, "linux-compat: lseek: bad offset");
        return -22;
    }
    runtime->fds[fd].offset = (size_t)next;
    set_trace(out_trace, path, 0, "linux-compat: lseek: ok");
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
    runtime->fds[fd].overlay_node = false;
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

    if (runtime == 0) {
        set_trace(out_trace, "", 9, "linux-compat: write: bad fd");
        return -9;
    }
    if (buffer == 0 && length != 0U) {
        set_trace(out_trace, "", 14, "linux-compat: write: bad buffer");
        return -14;
    }
    if (fd >= 3) {
        linux_compat_overlay_node_t* node = 0;
        size_t end = 0;

        if (!fd_is_valid_file(runtime, fd)) {
            set_trace(out_trace, "", 9, "linux-compat: write: bad fd");
            return -9;
        }
        if (!runtime->fds[fd].overlay_node ||
            !linux_compat_flags_writable(runtime->fds[fd].flags)) {
            set_trace(out_trace, fd_path(runtime, fd), 9,
                      "linux-compat: write: fd not writable");
            return -9;
        }
        node = (linux_compat_overlay_node_t*)runtime->fds[fd].node;
        if (node->directory) {
            set_trace(out_trace, node->path, 21,
                      "linux-compat: write: is directory");
            return -21;
        }
        if (runtime->fds[fd].offset >
                (size_t)LINUX_COMPAT_MAX_OVERLAY_FILE_SIZE ||
            length > (size_t)LINUX_COMPAT_MAX_OVERLAY_FILE_SIZE -
                         runtime->fds[fd].offset) {
            set_trace(out_trace, node->path, 28,
                      "linux-compat: write: overlay file full");
            return -28;
        }
        end = runtime->fds[fd].offset + length;
        while (i < length) {
            uint8_t chunk[64];
            const size_t chunk_size =
                (length - i) < sizeof(chunk) ? (length - i) : sizeof(chunk);

            if (!linux_compat_read_user_buffer(runtime,
                                               (const uint8_t*)buffer + i,
                                               chunk,
                                               chunk_size)) {
                set_trace(out_trace, node->path, 14,
                          "linux-compat: write: bad user buffer");
                return -14;
            }
            copy_bytes(node->data + runtime->fds[fd].offset + i,
                       chunk,
                       chunk_size);
            i += chunk_size;
        }
        runtime->fds[fd].offset = end;
        if (end > node->size) {
            node->size = end;
        }
        node->dirty = true;
        node->mtime = runtime->next_overlay_mtime;
        runtime->next_overlay_mtime += 1U;
        set_trace(out_trace, node->path, 0,
                  "linux-compat: write: overlay");
        return (int64_t)length;
    }
    if (fd != 1 && fd != 2) {
        set_trace(out_trace, "", 9, "linux-compat: write: bad fd");
        return -9;
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
    const char* path = fd_path(runtime, fd);
    size_t i = 0;
    size_t count = 0;

    if (!fd_is_valid_file(runtime, fd)) {
        set_trace(out_trace, "", 9, "linux-compat: getdents64: bad fd");
        return -9;
    }
    if (!fd_is_directory(runtime, fd)) {
        set_trace(out_trace, path, 20,
                  "linux-compat: getdents64: not directory");
        return -20;
    }
    if (dirents == 0 || capacity == 0U) {
        set_trace(out_trace, path, 22,
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
        if (child == 0 || find_overlay_node(runtime, child->path) != 0 ||
            !copy_basename_if_child(path,
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
                set_trace(out_trace, path, 14,
                          "linux-compat: getdents64: bad buffer");
                return -14;
            }
        } else {
            dirents[count] = record;
        }
        count += 1U;
    }
    for (i = 0; i < LINUX_COMPAT_MAX_OVERLAY_NODES; ++i) {
        linux_compat_overlay_node_t* child = &runtime->overlay_nodes[i];
        char name[LINUX_COMPAT_MAX_PATH];
        linux_compat_dirent_t record;

        if (count >= capacity) {
            break;
        }
        if (!child->used ||
            !copy_basename_if_child(path,
                                    child->path,
                                    name,
                                    sizeof(name))) {
            continue;
        }
        record.inode = child->inode;
        record.type =
            child->directory ? LINUX_COMPAT_DT_DIR : LINUX_COMPAT_DT_REG;
        copy_str(record.name, sizeof(record.name), name);
        if (runtime->vm != 0) {
            if (!linux_compat_write_result_buffer(runtime,
                                                  (uint8_t*)dirents +
                                                      (count * sizeof(record)),
                                                  &record,
                                                  sizeof(record))) {
                set_trace(out_trace, path, 14,
                          "linux-compat: getdents64: bad buffer");
                return -14;
            }
        } else {
            dirents[count] = record;
        }
        count += 1U;
    }
    set_trace(out_trace, path, 0, "linux-compat: getdents64: ok");
    return (int64_t)count;
}

static int64_t linux_compat_mprotect(uint64_t addr,
                                     size_t length,
                                     uint32_t prot,
                                     linux_compat_trace_t* out_trace) {
    (void)addr;
    (void)prot;
    if (length == 0U) {
        set_trace(out_trace, "", 22, "linux-compat: mprotect: empty range");
        return -22;
    }
    set_trace(out_trace, "", 0, "linux-compat: mprotect");
    return 0;
}

static int64_t linux_compat_uname(linux_compat_runtime_t* runtime,
                                  void* buffer,
                                  linux_compat_trace_t* out_trace) {
    linux_compat_utsname_t utsname;

    if (buffer == 0) {
        set_trace(out_trace, "", 14, "linux-compat: uname: bad buffer");
        return -14;
    }
    zero_bytes(&utsname, sizeof(utsname));
    copy_str(utsname.sysname, sizeof(utsname.sysname), "Linux");
    copy_str(utsname.nodename, sizeof(utsname.nodename), "mycpu");
    copy_str(utsname.release, sizeof(utsname.release), "6.0.0-myCPU");
    copy_str(utsname.version, sizeof(utsname.version), "#1 kernel_alpha");
    copy_str(utsname.machine, sizeof(utsname.machine), "riscv64");
    copy_str(utsname.domainname, sizeof(utsname.domainname), "localdomain");
    if (!linux_compat_write_result_buffer(runtime,
                                          buffer,
                                          &utsname,
                                          sizeof(utsname))) {
        set_trace(out_trace, "", 14, "linux-compat: uname: bad buffer");
        return -14;
    }
    set_trace(out_trace, "", 0, "linux-compat: uname");
    return 0;
}

static int64_t linux_compat_prlimit64(linux_compat_runtime_t* runtime,
                                      uint32_t resource,
                                      void* old_limit,
                                      linux_compat_trace_t* out_trace) {
    linux_compat_rlimit_t limit;

    (void)resource;
    if (old_limit == 0) {
        set_trace(out_trace, "", 0, "linux-compat: prlimit64");
        return 0;
    }
    limit.current = 8U * 1024U * 1024U;
    limit.maximum = limit.current;
    if (!linux_compat_write_result_buffer(runtime,
                                          old_limit,
                                          &limit,
                                          sizeof(limit))) {
        set_trace(out_trace, "", 14, "linux-compat: prlimit64: bad buffer");
        return -14;
    }
    set_trace(out_trace, "", 0, "linux-compat: prlimit64");
    return 0;
}

static int64_t linux_compat_readlinkat(linux_compat_runtime_t* runtime,
                                       const char* path,
                                       void* buffer,
                                       size_t length,
                                       linux_compat_trace_t* out_trace) {
    char resolved_path[LINUX_COMPAT_MAX_PATH];
    const char* target = "linux-compat:/proc/self/exe";
    const size_t target_len = str_len(target);
    const size_t copy_len = target_len < (length - 1U) ? target_len
                                                       : (length - 1U);

    if (buffer == 0 || length == 0U) {
        set_trace(out_trace, "", 14, "linux-compat: readlinkat: bad buffer");
        return -14;
    }
    if (!linux_compat_copy_in_path(runtime, path, resolved_path,
                                   sizeof(resolved_path))) {
        set_trace(out_trace, "", 14, "linux-compat: readlinkat: bad path");
        return -14;
    }
    if (!str_eq(resolved_path, "/proc/self/exe")) {
        set_trace(out_trace, resolved_path, 2,
                  "linux-compat: readlinkat: no such file");
        return -2;
    }
    if (!linux_compat_write_result_buffer(runtime, buffer, target, copy_len) ||
        !linux_compat_write_result_buffer(runtime,
                                          (uint8_t*)buffer + copy_len,
                                          "",
                                          1U)) {
        set_trace(out_trace, resolved_path, 14,
                  "linux-compat: readlinkat: bad buffer");
        return -14;
    }
    set_trace(out_trace, resolved_path, 0, "linux-compat: readlinkat");
    return (int64_t)copy_len;
}

static int64_t linux_compat_faccessat(linux_compat_runtime_t* runtime,
                                      const char* path,
                                      linux_compat_trace_t* out_trace) {
    char resolved_path[LINUX_COMPAT_MAX_PATH];
    linux_compat_stat_t stat;

    if (!linux_compat_copy_in_path(runtime, path, resolved_path,
                                   sizeof(resolved_path))) {
        set_trace(out_trace, "", 14, "linux-compat: faccessat: bad path");
        return -14;
    }
    if (linux_compat_stat_path_runtime(runtime, resolved_path, &stat, out_trace) !=
        LINUX_COMPAT_OK) {
        set_trace(out_trace, resolved_path, 2,
                  "linux-compat: faccessat: no such file");
        return -2;
    }
    set_trace(out_trace, resolved_path, 0, "linux-compat: faccessat");
    return 0;
}

static int64_t linux_compat_pread64(linux_compat_runtime_t* runtime,
                                    int32_t fd,
                                    void* buffer,
                                    size_t length,
                                    uint64_t offset,
                                    linux_compat_trace_t* out_trace) {
    size_t saved_offset = 0;
    int64_t result = 0;

    if (runtime == 0 || fd < 0 || fd >= (int32_t)LINUX_COMPAT_MAX_FDS ||
        (fd >= 3 && !runtime->fds[fd].open)) {
        set_trace(out_trace, "", 9, "linux-compat: pread64: bad fd");
        return -9;
    }
    saved_offset = runtime->fds[fd].offset;
    runtime->fds[fd].offset = (size_t)offset;
    result = linux_compat_read(runtime, fd, buffer, length, out_trace);
    runtime->fds[fd].offset = saved_offset;
    if (result >= 0) {
        set_trace(out_trace, "", 0, "linux-compat: pread64");
    }
    return result;
}

static int64_t linux_compat_pwrite64(linux_compat_runtime_t* runtime,
                                     int32_t fd,
                                     const void* buffer,
                                     size_t length,
                                     uint64_t offset,
                                     linux_compat_trace_t* out_trace) {
    size_t saved_offset = 0;
    int64_t result = 0;

    if (!fd_is_valid_file(runtime, fd)) {
        set_trace(out_trace, "", 9, "linux-compat: pwrite64: bad fd");
        return -9;
    }
    saved_offset = runtime->fds[fd].offset;
    runtime->fds[fd].offset = (size_t)offset;
    result = linux_compat_write(runtime, fd, buffer, length, out_trace);
    runtime->fds[fd].offset = saved_offset;
    if (result >= 0) {
        set_trace(out_trace, fd_path(runtime, fd), 0,
                  "linux-compat: pwrite64");
    }
    return result;
}

static int64_t linux_compat_fstat(linux_compat_runtime_t* runtime,
                                  int32_t fd,
                                  linux_compat_stat_t* out_stat,
                                  linux_compat_trace_t* out_trace) {
    linux_compat_stat_t stat;

    if (!fd_is_valid_file(runtime, fd)) {
        set_trace(out_trace, "", 9, "linux-compat: fstat: bad fd");
        return -9;
    }
    if (out_stat == 0) {
        set_trace(out_trace, fd_path(runtime, fd), 14,
                  "linux-compat: fstat: bad buffer");
        return -14;
    }
    if (runtime->fds[fd].overlay_node) {
        fill_stat_from_overlay(
            &stat,
            (const linux_compat_overlay_node_t*)runtime->fds[fd].node);
    } else {
        fill_stat_from_node(
            &stat,
            (const linux_compat_rootfs_node_t*)runtime->fds[fd].node);
    }
    if (!linux_compat_write_result_buffer(runtime,
                                          out_stat,
                                          &stat,
                                          sizeof(stat))) {
        set_trace(out_trace, fd_path(runtime, fd), 14,
                  "linux-compat: fstat: bad buffer");
        return -14;
    }
    set_trace(out_trace, fd_path(runtime, fd), 0, "linux-compat: fstat");
    return 0;
}

static int64_t linux_compat_writev(linux_compat_runtime_t* runtime,
                                   int32_t fd,
                                   const void* iovecs,
                                   size_t count,
                                   linux_compat_trace_t* out_trace) {
    size_t i = 0;
    int64_t total = 0;

    if (count > 16U || (iovecs == 0 && count != 0U)) {
        set_trace(out_trace, "", 22, "linux-compat: writev: bad iov");
        return -22;
    }
    for (i = 0; i < count; ++i) {
        linux_compat_iovec_t iov;
        int64_t wrote = 0;

        if (!linux_compat_read_user_buffer(
                runtime,
                (const uint8_t*)iovecs + (i * sizeof(iov)),
                &iov,
                sizeof(iov))) {
            set_trace(out_trace, "", 14, "linux-compat: writev: bad iov");
            return -14;
        }
        wrote = linux_compat_write(runtime, fd, iov.base, iov.length, out_trace);
        if (wrote < 0) {
            return wrote;
        }
        total += wrote;
    }
    set_trace(out_trace, "", 0, "linux-compat: writev");
    return total;
}

static int64_t linux_compat_statx(linux_compat_runtime_t* runtime,
                                  const char* path,
                                  linux_compat_statx_t* out_statx,
                                  linux_compat_trace_t* out_trace) {
    char resolved_path[LINUX_COMPAT_MAX_PATH];
    linux_compat_stat_t stat;
    linux_compat_statx_t statx;

    if (out_statx == 0) {
        set_trace(out_trace, "", 14, "linux-compat: statx: bad buffer");
        return -14;
    }
    if (!linux_compat_copy_in_path(runtime, path, resolved_path,
                                   sizeof(resolved_path))) {
        set_trace(out_trace, "", 14, "linux-compat: statx: bad path");
        return -14;
    }
    if (linux_compat_stat_path_runtime(runtime, resolved_path, &stat, out_trace) !=
        LINUX_COMPAT_OK) {
        set_trace(out_trace, resolved_path, 2,
                  "linux-compat: statx: no such file");
        return -2;
    }
    zero_bytes(&statx, sizeof(statx));
    statx.mask = 0x17ffU;
    statx.blksize = 4096U;
    statx.inode = stat.inode;
    statx.size = stat.size;
    statx.mode = stat.mode;
    if (!linux_compat_write_result_buffer(runtime,
                                          out_statx,
                                          &statx,
                                          sizeof(statx))) {
        set_trace(out_trace, resolved_path, 14,
                  "linux-compat: statx: bad buffer");
        return -14;
    }
    set_trace(out_trace, resolved_path, 0, "linux-compat: statx");
    return 0;
}

static int64_t linux_compat_mkdirat(linux_compat_runtime_t* runtime,
                                    int32_t dirfd,
                                    const char* path,
                                    uint32_t mode,
                                    linux_compat_trace_t* out_trace) {
    char resolved_path[LINUX_COMPAT_MAX_PATH];
    linux_compat_overlay_node_t* node = 0;

    (void)mode;
    if (dirfd != LINUX_COMPAT_AT_FDCWD) {
        set_trace(out_trace, path != 0 ? path : "", 22,
                  "linux-compat: mkdirat: unsupported path base");
        return -22;
    }
    if (!linux_compat_copy_in_path(runtime,
                                   path,
                                   resolved_path,
                                   sizeof(resolved_path))) {
        set_trace(out_trace, "", 14, "linux-compat: mkdirat: bad path");
        return -14;
    }
    if (resolved_path[0] != '/') {
        set_trace(out_trace, resolved_path, 22,
                  "linux-compat: mkdirat: unsupported path base");
        return -22;
    }
    if (path_exists(runtime, resolved_path)) {
        set_trace(out_trace, resolved_path, 17,
                  "linux-compat: mkdirat: exists");
        return -17;
    }
    if (!parent_directory_exists(runtime, resolved_path)) {
        set_trace(out_trace, resolved_path, 2,
                  "linux-compat: mkdirat: parent missing");
        return -2;
    }
    node = alloc_overlay_node(runtime,
                              resolved_path,
                              true,
                              linux_compat_dir_mode());
    if (node == 0) {
        set_trace(out_trace, resolved_path, 28,
                  "linux-compat: mkdirat: overlay full");
        return -28;
    }
    set_trace(out_trace, resolved_path, 0, "linux-compat: mkdirat");
    return 0;
}

static int64_t linux_compat_unlinkat(linux_compat_runtime_t* runtime,
                                     int32_t dirfd,
                                     const char* path,
                                     linux_compat_trace_t* out_trace) {
    char resolved_path[LINUX_COMPAT_MAX_PATH];
    linux_compat_overlay_node_t* overlay = 0;
    const linux_compat_rootfs_node_t* lower = 0;

    if (dirfd != LINUX_COMPAT_AT_FDCWD) {
        set_trace(out_trace, path != 0 ? path : "", 22,
                  "linux-compat: unlinkat: unsupported path base");
        return -22;
    }
    if (!linux_compat_copy_in_path(runtime,
                                   path,
                                   resolved_path,
                                   sizeof(resolved_path))) {
        set_trace(out_trace, "", 14, "linux-compat: unlinkat: bad path");
        return -14;
    }
    overlay = find_overlay_node(runtime, resolved_path);
    if (overlay != 0) {
        if (overlay->directory) {
            set_trace(out_trace, resolved_path, 21,
                      "linux-compat: unlinkat: is directory");
            return -21;
        }
        overlay->used = false;
        overlay->dirty = true;
        set_trace(out_trace, resolved_path, 0, "linux-compat: unlinkat");
        return 0;
    }
    lower = find_node(resolved_path);
    if (lower != 0) {
        set_trace(out_trace, resolved_path, 30,
                  "linux-compat: unlinkat: readonly lower");
        return -30;
    }
    set_trace(out_trace, resolved_path, 2,
              "linux-compat: unlinkat: no such file");
    return -2;
}

static int64_t linux_compat_renameat(linux_compat_runtime_t* runtime,
                                     int32_t dirfd,
                                     const char* old_path,
                                     const char* new_path,
                                     linux_compat_trace_t* out_trace) {
    char old_resolved[LINUX_COMPAT_MAX_PATH];
    char new_resolved[LINUX_COMPAT_MAX_PATH];
    linux_compat_overlay_node_t* source = 0;
    linux_compat_overlay_node_t* target = 0;

    if (dirfd != LINUX_COMPAT_AT_FDCWD) {
        set_trace(out_trace, old_path != 0 ? old_path : "", 22,
                  "linux-compat: renameat: unsupported path base");
        return -22;
    }
    if (!linux_compat_copy_in_path(runtime,
                                   old_path,
                                   old_resolved,
                                   sizeof(old_resolved)) ||
        !linux_compat_copy_in_path(runtime,
                                   new_path,
                                   new_resolved,
                                   sizeof(new_resolved))) {
        set_trace(out_trace, "", 14, "linux-compat: renameat: bad path");
        return -14;
    }
    if (old_resolved[0] != '/' || new_resolved[0] != '/') {
        set_trace(out_trace, old_resolved, 22,
                  "linux-compat: renameat: unsupported path base");
        return -22;
    }
    source = find_overlay_node(runtime, old_resolved);
    if (source == 0) {
        if (find_node(old_resolved) != 0) {
            set_trace(out_trace, old_resolved, 30,
                      "linux-compat: renameat: readonly lower");
            return -30;
        }
        set_trace(out_trace, old_resolved, 2,
                  "linux-compat: renameat: no such file");
        return -2;
    }
    if (!parent_directory_exists(runtime, new_resolved)) {
        set_trace(out_trace, new_resolved, 2,
                  "linux-compat: renameat: parent missing");
        return -2;
    }
    if (find_node(new_resolved) != 0 && find_overlay_node(runtime, new_resolved) == 0) {
        set_trace(out_trace, new_resolved, 30,
                  "linux-compat: renameat: readonly lower target");
        return -30;
    }
    target = find_overlay_node(runtime, new_resolved);
    if (target != 0 && target != source) {
        target->used = false;
    }
    copy_str(source->path, sizeof(source->path), new_resolved);
    source->dirty = true;
    source->mtime = runtime->next_overlay_mtime;
    runtime->next_overlay_mtime += 1U;
    set_trace(out_trace, new_resolved, 0, "linux-compat: renameat");
    return 0;
}

static int64_t linux_compat_ftruncate(linux_compat_runtime_t* runtime,
                                      int32_t fd,
                                      size_t length,
                                      linux_compat_trace_t* out_trace) {
    linux_compat_overlay_node_t* node = 0;

    if (!fd_is_valid_file(runtime, fd) || !runtime->fds[fd].overlay_node ||
        !linux_compat_flags_writable(runtime->fds[fd].flags)) {
        set_trace(out_trace, "", 9, "linux-compat: ftruncate: bad fd");
        return -9;
    }
    node = (linux_compat_overlay_node_t*)runtime->fds[fd].node;
    if (node->directory) {
        set_trace(out_trace, node->path, 22,
                  "linux-compat: ftruncate: is directory");
        return -22;
    }
    if (length > LINUX_COMPAT_MAX_OVERLAY_FILE_SIZE) {
        set_trace(out_trace, node->path, 27,
                  "linux-compat: ftruncate: too large");
        return -27;
    }
    if (length > node->size) {
        zero_bytes(node->data + node->size, length - node->size);
    }
    node->size = length;
    if (runtime->fds[fd].offset > length) {
        runtime->fds[fd].offset = length;
    }
    node->dirty = true;
    node->mtime = runtime->next_overlay_mtime;
    runtime->next_overlay_mtime += 1U;
    set_trace(out_trace, node->path, 0, "linux-compat: ftruncate");
    return 0;
}

static int64_t linux_compat_sync_fd(linux_compat_runtime_t* runtime,
                                    int32_t fd,
                                    linux_compat_trace_t* out_trace,
                                    const char* operation) {
    if (!fd_is_valid_file(runtime, fd)) {
        set_trace(out_trace, "", 9, operation);
        return -9;
    }
    set_trace(out_trace, fd_path(runtime, fd), 0, operation);
    return 0;
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
    } else if (request->number == LINUX_COMPAT_SYS_MPROTECT) {
        value = linux_compat_mprotect(request->addr,
                                      request->length,
                                      request->prot,
                                      out_trace);
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
        result = linux_compat_stat_path_runtime(runtime,
                                                resolved_path,
                                                &stat_value,
                                                out_trace);
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
    } else if (request->number == LINUX_COMPAT_SYS_MKDIRAT) {
        value = linux_compat_mkdirat(runtime,
                                     request->dirfd,
                                     request->path,
                                     request->flags,
                                     out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_UNLINKAT) {
        value = linux_compat_unlinkat(runtime,
                                      request->dirfd,
                                      request->path,
                                      out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_RENAMEAT ||
               request->number == LINUX_COMPAT_SYS_RENAMEAT2) {
        value = linux_compat_renameat(runtime,
                                      request->dirfd,
                                      request->path,
                                      request->new_path,
                                      out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_FTRUNCATE) {
        value = linux_compat_ftruncate(runtime,
                                       request->fd,
                                       request->length,
                                       out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_FACCESSAT) {
        value = linux_compat_faccessat(runtime, request->path, out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_OPENAT) {
        value = linux_compat_openat(runtime,
                                    request->dirfd,
                                    request->path,
                                    request->flags,
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
    } else if (request->number == LINUX_COMPAT_SYS_PREAD64) {
        value = linux_compat_pread64(runtime,
                                     request->fd,
                                     request->read_buffer,
                                     request->length,
                                     request->offset,
                                     out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_PWRITE64) {
        value = linux_compat_pwrite64(runtime,
                                      request->fd,
                                      request->write_buffer,
                                      request->length,
                                      request->offset,
                                      out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_FSTAT) {
        value = linux_compat_fstat(runtime,
                                   request->fd,
                                   request->stat,
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
    } else if (request->number == LINUX_COMPAT_SYS_WRITEV) {
        value = linux_compat_writev(runtime,
                                    request->fd,
                                    request->write_buffer,
                                    request->length,
                                    out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_SYNC) {
        value = 0;
        set_trace(out_trace, "", 0, "linux-compat: sync");
    } else if (request->number == LINUX_COMPAT_SYS_FSYNC) {
        value = linux_compat_sync_fd(runtime,
                                     request->fd,
                                     out_trace,
                                     "linux-compat: fsync");
    } else if (request->number == LINUX_COMPAT_SYS_FDATASYNC) {
        value = linux_compat_sync_fd(runtime,
                                     request->fd,
                                     out_trace,
                                     "linux-compat: fdatasync");
    } else if (request->number == LINUX_COMPAT_SYS_READLINKAT) {
        value = linux_compat_readlinkat(runtime,
                                        request->path,
                                        request->read_buffer,
                                        request->length,
                                        out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_CLOCK_GETTIME) {
        value = linux_compat_clock_gettime(request->fd,
                                           runtime,
                                           request->read_buffer,
                                           out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_UNAME) {
        value = linux_compat_uname(runtime, request->read_buffer, out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_PRLIMIT64) {
        value = linux_compat_prlimit64(runtime,
                                       request->command,
                                       request->read_buffer,
                                       out_trace);
    } else if (request->number == LINUX_COMPAT_SYS_SET_TID_ADDRESS) {
        value = 1;
        set_trace(out_trace, "", 0, "linux-compat: set_tid_address");
    } else if (request->number == LINUX_COMPAT_SYS_SET_ROBUST_LIST) {
        value = 0;
        set_trace(out_trace, "", 0, "linux-compat: set_robust_list");
    } else if (request->number == LINUX_COMPAT_SYS_RT_SIGACTION) {
        value = 0;
        set_trace(out_trace, "", 0, "linux-compat: rt_sigaction");
    } else if (request->number == LINUX_COMPAT_SYS_RT_SIGPROCMASK) {
        value = 0;
        set_trace(out_trace, "", 0, "linux-compat: rt_sigprocmask");
    } else if (request->number == LINUX_COMPAT_SYS_STATX) {
        value = linux_compat_statx(runtime,
                                   request->path,
                                   request->statx,
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
    size_t used = 0;

    if (out == 0 || out_size == 0) {
        return LINUX_COMPAT_ERR_NO_SUCH_FILE;
    }
    out[0] = '\0';
    zero_bytes(scratch, sizeof(*scratch));
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
        (void)append_str(out, out_size, &used, " message=path: no such file\n");
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
        result = linux_compat_build_load_plan(scratch->interp_entry.data,
                                              scratch->interp_entry.size,
                                              1U,
                                              0U,
                                              &scratch->interp_plan,
                                              &scratch->trace);
        if (result != LINUX_COMPAT_OK || scratch->interp_plan.requires_interp ||
            (scratch->interp_plan.elf_type == 3U &&
             !rebase_load_plan(&scratch->interp_plan,
                               LINUX_COMPAT_INTERP_LOAD_BIAS))) {
            set_trace(out_trace,
                      scratch->entry.path,
                      9,
                      "linux-compat: loader: interp unsupported");
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
                             " errno=9 loader reason=interp unsupported\n");
            return LINUX_COMPAT_ERR_UNSUPPORTED_ELF;
        }
        scratch->load_plan.interp_load_bias =
            scratch->interp_plan.load_bias != 0U
                ? scratch->interp_plan.load_bias
                : scratch->interp_plan.segments[0].vaddr;
        scratch->load_plan.interp_entry = scratch->interp_plan.entry;
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
        if (result == LINUX_COMPAT_OK && scratch->load_plan.requires_interp) {
            result = linux_compat_exec_load(&scratch->vm,
                                            scratch->interp_entry.data,
                                            scratch->interp_entry.size,
                                            &scratch->interp_plan,
                                            &scratch->interp_entry_pc,
                                            &scratch->trace);
        }
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
                                             scratch->load_plan.requires_interp
                                                 ? scratch->interp_entry_pc
                                                 : scratch->entry_pc,
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

    set_trace(out_trace,
              scratch->entry.path,
              38,
              "linux-compat: exec: real exec context missing");
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
                     " fail-closed real exec context missing errno=38\n");
    return LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL;
}
