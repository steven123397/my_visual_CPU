#include "linux_compat.h"

static const uint8_t k_busybox_stub[128] = {
    [0] = 0x7f,
    [1] = 'E',
    [2] = 'L',
    [3] = 'F',
    [4] = 2,
    [5] = 1,
    [6] = 1,
    [16] = 2,
    [18] = 0xf3,
    [20] = 1,
    [24] = 0x00,
    [25] = 0x10,
    [26] = 0x40,
    [32] = 64,
    [52] = 64,
    [54] = 56,
    [56] = 1,
    [64] = 1,
};

static const uint8_t k_git_stub[128] = {
    [0] = 0x7f,
    [1] = 'E',
    [2] = 'L',
    [3] = 'F',
    [4] = 2,
    [5] = 1,
    [6] = 1,
    [16] = 2,
    [18] = 0xf3,
    [20] = 1,
    [24] = 0x00,
    [25] = 0x20,
    [26] = 0x40,
    [32] = 64,
    [52] = 64,
    [54] = 56,
    [56] = 1,
    [64] = 1,
};

static const linux_compat_rootfs_entry_t k_rootfs_catalog[] = {
    {"/bin/busybox", k_busybox_stub, sizeof(k_busybox_stub), true},
    {"/usr/bin/git", k_git_stub, sizeof(k_git_stub), true},
};

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
    size_t i = 0;

    clear_entry(out_entry);
    if (path == 0 || str_len(path) == 0U) {
        set_trace(out_trace, "", 2, "linux-compat: rootfs: no such file");
        return LINUX_COMPAT_ERR_NO_SUCH_FILE;
    }

    for (i = 0; i < sizeof(k_rootfs_catalog) / sizeof(k_rootfs_catalog[0]); ++i) {
        if (str_eq(path, k_rootfs_catalog[i].path)) {
            if (out_entry != 0) {
                *out_entry = k_rootfs_catalog[i];
            }
            set_trace(out_trace, path, 0, "linux-compat: rootfs: found");
            return LINUX_COMPAT_OK;
        }
    }

    set_trace(out_trace, path, 2, "linux-compat: rootfs: no such file");
    return LINUX_COMPAT_ERR_NO_SUCH_FILE;
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

linux_compat_result_t linux_compat_run(
    const linux_compat_exec_request_t* request,
    char* out,
    size_t out_size,
    linux_compat_trace_t* out_trace) {
    linux_compat_rootfs_entry_t entry;
    linux_compat_elf_info_t elf;
    linux_compat_trace_t trace;
    linux_compat_result_t result = LINUX_COMPAT_OK;
    size_t used = 0;

    if (out == 0 || out_size == 0) {
        return LINUX_COMPAT_ERR_NO_SUCH_FILE;
    }
    out[0] = '\0';
    if (request == 0 || request->path == 0 || str_len(request->path) == 0U) {
        set_trace(out_trace, "", 2, "linux-compat: usage: linux <path> [args...]");
        return append_str(out, out_size, &used, "linux-compat: usage: linux <path-or-command> [args...]\n")
                   ? LINUX_COMPAT_ERR_NO_SUCH_FILE
                   : LINUX_COMPAT_ERR_NO_SUCH_FILE;
    }

    result = linux_compat_lookup(request->path, &entry, &trace);
    if (result != LINUX_COMPAT_OK) {
        if (out_trace != 0) {
            *out_trace = trace;
        }
        (void)append_str(out, out_size, &used, "linux-compat: path=");
        (void)append_str(out, out_size, &used, trace.path);
        (void)append_str(out, out_size, &used, " errno=");
        (void)append_u64_dec(out, out_size, &used, (uint64_t)trace.errno_value);
        (void)append_str(out, out_size, &used, " message=rootfs: no such file\n");
        return result;
    }

    result = linux_compat_inspect_elf(entry.data, entry.size, &elf, &trace);
    if (result != LINUX_COMPAT_OK) {
        if (out_trace != 0) {
            *out_trace = trace;
        }
        (void)append_str(out, out_size, &used, "linux-compat: path=");
        (void)append_str(out, out_size, &used, entry.path);
        (void)append_str(out, out_size, &used, " errno=");
        (void)append_u64_dec(out, out_size, &used, (uint64_t)trace.errno_value);
        (void)append_char(out, out_size, &used, ' ');
        (void)append_str(out, out_size, &used, trace.message);
        (void)append_char(out, out_size, &used, '\n');
        return result;
    }

    set_trace(out_trace,
              entry.path,
              38,
              "linux-compat: syscall: unsupported syscall");
    if (out_trace != 0) {
        out_trace->syscall_number = 0;
        out_trace->pc = (uintptr_t)elf.entry;
    }

    (void)append_str(out, out_size, &used, "linux-compat: path=");
    (void)append_str(out, out_size, &used, entry.path);
    (void)append_str(out, out_size, &used, " argc=");
    (void)append_u64_dec(out, out_size, &used, (uint64_t)request->argc);
    (void)append_str(out, out_size, &used, " elf=rv64-little type=exec entry=");
    (void)append_u64_hex(out, out_size, &used, elf.entry);
    (void)append_str(out, out_size, &used, " phnum=");
    (void)append_u64_dec(out, out_size, &used, (uint64_t)elf.phnum);
    (void)append_str(out,
                     out_size,
                     &used,
                     " fail-closed unsupported syscall number=0 pc=");
    (void)append_u64_hex(out, out_size, &used, elf.entry);
    (void)append_str(out, out_size, &used, " errno=38\n");
    return LINUX_COMPAT_ERR_UNSUPPORTED_SYSCALL;
}
