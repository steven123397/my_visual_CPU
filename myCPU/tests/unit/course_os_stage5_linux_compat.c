/* Linux compat Stage5 单测：验证显式 linux launcher、ABI 标记和 fail-closed 诊断。 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_process.h"
#include "../../guest/include/linux_compat.h"

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

static void make_elf(uint8_t* image,
                     size_t size,
                     uint16_t type,
                     uint16_t machine,
                     uint32_t ph_type) {
    size_t i = 0;

    for (i = 0; i < size; ++i) {
        image[i] = 0U;
    }

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
    write_u64_le(image, 24U, 0x401000U);
    write_u64_le(image, 32U, 64U);
    write_u16_le(image, 52U, 64U);
    write_u16_le(image, 54U, 56U);
    write_u16_le(image, 56U, 1U);
    write_u32_le(image, 64U, ph_type);
}

static int test_rootfs_catalog_lookup_known_paths_and_bad_path(void) {
    linux_compat_rootfs_entry_t entry;
    linux_compat_trace_t trace;

    if (linux_compat_lookup("/bin/busybox", &entry, &trace) != LINUX_COMPAT_OK ||
        strcmp(entry.path, "/bin/busybox") != 0 ||
        entry.size == 0U ||
        !contains(trace.message, "rootfs: found")) {
        return fail("expected /bin/busybox to resolve from linux compat rootfs catalog");
    }

    if (linux_compat_lookup("/usr/bin/git", &entry, &trace) != LINUX_COMPAT_OK ||
        strcmp(entry.path, "/usr/bin/git") != 0 ||
        entry.size == 0U ||
        !contains(trace.message, "rootfs: found")) {
        return fail("expected /usr/bin/git to resolve from linux compat rootfs catalog");
    }

    if (linux_compat_lookup("/nope", &entry, &trace) !=
            LINUX_COMPAT_ERR_NO_SUCH_FILE ||
        trace.errno_value != 2 ||
        !contains(trace.message, "rootfs: no such file") ||
        !contains(trace.path, "/nope")) {
        return fail("expected bad linux compat path to fail closed");
    }

    return 0;
}

static int test_path_fallback_resolves_known_tools_only(void) {
    char resolved[LINUX_COMPAT_MAX_PATH];
    linux_compat_trace_t trace;

    if (linux_compat_resolve_path("git",
                                  resolved,
                                  sizeof(resolved),
                                  &trace) != LINUX_COMPAT_OK ||
        strcmp(resolved, "/usr/bin/git") != 0 ||
        !contains(trace.message, "path: fallback")) {
        return fail("expected git to resolve through Linux compat PATH fallback");
    }

    if (linux_compat_resolve_path("/usr/bin/git",
                                  resolved,
                                  sizeof(resolved),
                                  &trace) != LINUX_COMPAT_OK ||
        strcmp(resolved, "/usr/bin/git") != 0) {
        return fail("expected explicit Linux compat path to remain usable");
    }

    if (linux_compat_resolve_path("help",
                                  resolved,
                                  sizeof(resolved),
                                  &trace) != LINUX_COMPAT_ERR_NO_SUCH_FILE ||
        trace.errno_value != 2) {
        return fail("expected non-Linux fallback command to fail closed");
    }

    return 0;
}

static int test_stage10_missing_tools_resolve_for_diagnostics(void) {
    char resolved[LINUX_COMPAT_MAX_PATH];
    linux_compat_trace_t trace;

    if (linux_compat_resolve_path("vim",
                                  resolved,
                                  sizeof(resolved),
                                  &trace) != LINUX_COMPAT_OK ||
        strcmp(resolved, "/usr/bin/vim") != 0 ||
        !contains(trace.message, "path: fallback")) {
        return fail("expected missing vim asset to resolve for Linux compat diagnostics");
    }

    if (linux_compat_resolve_path("gcc",
                                  resolved,
                                  sizeof(resolved),
                                  &trace) != LINUX_COMPAT_OK ||
        strcmp(resolved, "/usr/bin/gcc") != 0 ||
        !contains(trace.message, "path: fallback")) {
        return fail("expected missing gcc asset to resolve for Linux compat diagnostics");
    }

    if (linux_compat_resolve_path("rustc",
                                  resolved,
                                  sizeof(resolved),
                                  &trace) != LINUX_COMPAT_OK ||
        strcmp(resolved, "/usr/bin/rustc") != 0 ||
        !contains(trace.message, "path: fallback")) {
        return fail("expected missing rustc asset to resolve for Linux compat diagnostics");
    }

    return 0;
}

static int test_missing_linux_compat_run_reports_path_diagnostic(void) {
    linux_compat_exec_request_t request;
    linux_compat_trace_t trace;
    char output[256];
    const char* argv[] = {"/usr/bin/vim", "-h"};

    memset(&request, 0, sizeof(request));
    request.path = "/usr/bin/vim";
    request.argc = 2U;
    request.argv = argv;

    if (linux_compat_run(&request, output, sizeof(output), &trace) !=
            LINUX_COMPAT_ERR_NO_SUCH_FILE ||
        !contains(output, "linux-compat: path=/usr/bin/vim") ||
        !contains(output, "errno=2") ||
        !contains(output, "path: no such file")) {
        return fail("expected missing Linux compat run to report PATH diagnostic");
    }

    return 0;
}

static int test_elf_inspection_accepts_rv64_exec_metadata(void) {
    uint8_t image[128];
    linux_compat_elf_info_t info;
    linux_compat_trace_t trace;

    make_elf(image, sizeof(image), 2U, 243U, 1U);

    if (linux_compat_inspect_elf(image, sizeof(image), &info, &trace) !=
            LINUX_COMPAT_OK ||
        info.elf_class != 2U ||
        info.endianness != 1U ||
        info.type != 2U ||
        info.machine != 243U ||
        info.entry != 0x401000U ||
        info.phoff != 64U ||
        info.phnum != 1U ||
        !contains(trace.message, "elf: ok")) {
        return fail("expected RV64 little-endian ET_EXEC ELF metadata");
    }

    return 0;
}

static int test_elf_inspection_fails_closed_for_bad_or_unsupported_elf(void) {
    uint8_t image[128];
    linux_compat_elf_info_t info;
    linux_compat_trace_t trace;

    make_elf(image, sizeof(image), 2U, 243U, 1U);
    image[0] = 0U;
    if (linux_compat_inspect_elf(image, sizeof(image), &info, &trace) !=
            LINUX_COMPAT_ERR_BAD_ELF ||
        trace.errno_value != 8 ||
        !contains(trace.message, "elf: bad magic")) {
        return fail("expected bad ELF magic to fail closed");
    }

    make_elf(image, sizeof(image), 2U, 62U, 1U);
    if (linux_compat_inspect_elf(image, sizeof(image), &info, &trace) !=
            LINUX_COMPAT_ERR_UNSUPPORTED_ELF ||
        !contains(trace.message, "elf: unsupported machine")) {
        return fail("expected non-RISC-V ELF machine to fail closed");
    }

    make_elf(image, sizeof(image), 3U, 243U, 1U);
    if (linux_compat_inspect_elf(image, sizeof(image), &info, &trace) !=
            LINUX_COMPAT_ERR_UNSUPPORTED_ELF ||
        !contains(trace.message, "elf: unsupported type")) {
        return fail("expected ET_DYN ELF to fail closed in v0");
    }

    return 0;
}

static int test_process_abi_defaults_course_and_marks_linux_compat(void) {
    course_process_table_t table;
    course_process_t* init = NULL;
    course_process_t* child = NULL;
    course_process_abi_t abi = COURSE_PROCESS_ABI_COURSE;

    course_process_table_init(&table);
    init = course_process_spawn(&table, 0U, "shell");
    if (init == NULL ||
        init->abi != COURSE_PROCESS_ABI_COURSE ||
        !course_process_get_abi(&table, init->pid, &abi) ||
        abi != COURSE_PROCESS_ABI_COURSE) {
        return fail("expected course processes to default to COURSE ABI");
    }

    if (!course_process_set_abi(&table,
                                init->pid,
                                COURSE_PROCESS_ABI_LINUX_COMPAT) ||
        !course_process_get_abi(&table, init->pid, &abi) ||
        abi != COURSE_PROCESS_ABI_LINUX_COMPAT) {
        return fail("expected Linux compat launcher to mark process ABI");
    }

    child = course_process_fork(&table, init->pid, "linux-child");
    if (child == NULL ||
        child->abi != COURSE_PROCESS_ABI_LINUX_COMPAT ||
        !course_process_get_abi(&table, child->pid, &abi) ||
        abi != COURSE_PROCESS_ABI_LINUX_COMPAT) {
        return fail("expected forked process to inherit Linux compat ABI");
    }

    return 0;
}

int main(void) {
    if (test_rootfs_catalog_lookup_known_paths_and_bad_path() != 0 ||
        test_path_fallback_resolves_known_tools_only() != 0 ||
        test_stage10_missing_tools_resolve_for_diagnostics() != 0 ||
        test_missing_linux_compat_run_reports_path_diagnostic() != 0 ||
        test_elf_inspection_accepts_rv64_exec_metadata() != 0 ||
        test_elf_inspection_fails_closed_for_bad_or_unsupported_elf() != 0 ||
        test_process_abi_defaults_course_and_marks_linux_compat() != 0) {
        return 1;
    }

    return 0;
}
