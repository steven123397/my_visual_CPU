/* Linux compat Stage7 单测：验证外部 rootfs manifest 接入和缺资产行为。 */
#include <stdio.h>
#include <string.h>

#include "../../guest/include/linux_compat.h"
#include "../../guest/include/linux_compat_rootfs.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int test_provider_reports_external_source(void) {
    if (strcmp(linux_compat_rootfs_source_name(), "external") != 0) {
        return fail("expected generated external Linux compat rootfs provider");
    }
    return 0;
}

static int test_generated_busybox_can_be_read_and_inspected(void) {
    linux_compat_rootfs_entry_t entry;
    linux_compat_trace_t trace;
    linux_compat_elf_info_t elf;

    if (linux_compat_lookup("/bin/busybox", &entry, &trace) != LINUX_COMPAT_OK ||
        entry.size <= 128U ||
        linux_compat_inspect_elf(entry.data, entry.size, &elf, &trace) !=
            LINUX_COMPAT_OK ||
        elf.machine != 243U) {
        return fail("expected generated /bin/busybox to be real RV64 ELF bytes");
    }
    return 0;
}

static int test_optional_interpreter_asset_is_explicit_when_present(void) {
    linux_compat_rootfs_entry_t entry;
    linux_compat_trace_t trace;
    linux_compat_result_t result =
        linux_compat_lookup("/lib/ld-musl-riscv64.so.1", &entry, &trace);

    if (result == LINUX_COMPAT_OK) {
        if (entry.size == 0U || entry.data == NULL) {
            return fail("expected present optional interpreter to expose bytes");
        }
        return 0;
    }
    if (result != LINUX_COMPAT_ERR_NO_SUCH_FILE || trace.errno_value != 2) {
        return fail("expected optional interpreter to be present or explicitly absent");
    }
    return 0;
}

int main(void) {
    if (test_provider_reports_external_source() != 0 ||
        test_generated_busybox_can_be_read_and_inspected() != 0 ||
        test_optional_interpreter_asset_is_explicit_when_present() != 0) {
        return 1;
    }
    return 0;
}
