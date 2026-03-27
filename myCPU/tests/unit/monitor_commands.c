#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/kernel_runtime.h"
#include "../../guest/include/monitor_commands.h"
#include "../../guest/include/storage.h"
#include "../../guest/kernel/vm_private.h"

void monitor_commands_reset(uint64_t boot_mtime);

static char g_output[8192];
static size_t g_output_len = 0;
static bool g_shutdown_called = false;
static uint64_t g_shutdown_code = 0;
static jmp_buf g_shutdown_env;
static bool g_expect_shutdown = false;
static uint64_t g_mtime = 0;
static bool g_storage_probe_result = true;
static storage_info_t g_storage_info = {
    .magic = UINT64_C(0x53544f52414745),
    .version = 1,
    .block_size = 512,
    .capacity_blocks = 16,
    .status = 0,
};
static uint64_t g_storage_status = 0;
static uint64_t g_storage_error = 0;
static uint64_t g_storage_read_result = 0;
static uint8_t g_storage_block[512];

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    fprintf(stderr, "output was:\n%s\n", g_output);
    return 1;
}

static void reset_output(void) {
    memset(g_output, 0, sizeof(g_output));
    g_output_len = 0;
}

static void append_output(const char* text) {
    const size_t remaining = sizeof(g_output) - g_output_len - 1U;
    size_t count = 0U;

    while (text != NULL && count < remaining && text[count] != '\0') {
        ++count;
    }
    if (count > 0U) {
        memcpy(g_output + g_output_len, text, count);
        g_output_len += count;
        g_output[g_output_len] = '\0';
    }
}

static void reset_stubs(void) {
    static const char kStorageText[] = "StorageImageData";

    reset_output();
    g_shutdown_called = false;
    g_shutdown_code = 0;
    g_expect_shutdown = false;
    g_mtime = 1234;
    g_storage_probe_result = true;
    g_storage_status = 0;
    g_storage_error = 0;
    g_storage_read_result = 0;
    memset(g_storage_block, 0, sizeof(g_storage_block));
    memcpy(g_storage_block, kStorageText, sizeof(kStorageText) - 1U);
}

void monitor_write_text(const char* text) {
    append_output(text);
}

void monitor_write_line(const char* text) {
    append_output(text != NULL ? text : "");
    append_output("\n");
}

uint64_t platform_clint_read_mtime(void) {
    return g_mtime;
}

bool storage_probe(storage_info_t* info) {
    if (g_storage_probe_result && info != NULL) {
        *info = g_storage_info;
    }
    return g_storage_probe_result;
}

bool storage_read_info(storage_info_t* info) {
    return storage_probe(info);
}

uint64_t storage_status(void) {
    return g_storage_status;
}

uint64_t storage_error(void) {
    return g_storage_error;
}

void storage_clear_error(void) {}

uint64_t storage_read_block(uint64_t lba, void* destination) {
    (void)lba;
    if (destination != NULL) {
        memcpy(destination, g_storage_block, sizeof(g_storage_block));
    }
    return g_storage_read_result;
}

uint64_t storage_read_block_with_count(uint64_t lba,
                                       uint64_t block_count,
                                       void* destination) {
    (void)block_count;
    return storage_read_block(lba, destination);
}

void platform_shutdown(uint64_t code) {
    g_shutdown_called = true;
    g_shutdown_code = code;
    if (g_expect_shutdown) {
        longjmp(g_shutdown_env, 1);
    }

    longjmp(g_shutdown_env, 2);
}

static int test_help_time_uptime_and_halt(void) {
    kernel_runtime_t runtime;

    reset_stubs();
    memset(&runtime, 0, sizeof(runtime));
    monitor_commands_reset(1000);

    if (!monitor_execute_line(&runtime, "help")) {
        return fail("help should continue running");
    }
    if (strstr(g_output, "help echo time uptime halt disk regs peek pagewalk pte") == NULL) {
        return fail("help should list the extended monitor commands");
    }

    reset_output();
    if (!monitor_execute_line(&runtime, "time")) {
        return fail("time should continue running");
    }
    if (strstr(g_output, "mtime=1234") == NULL) {
        return fail("time should report the current mtime");
    }

    reset_output();
    if (!monitor_execute_line(&runtime, "uptime")) {
        return fail("uptime should continue running");
    }
    if (strstr(g_output, "uptime=234") == NULL) {
        return fail("uptime should report ticks since boot");
    }

    reset_output();
    g_expect_shutdown = true;
    if (setjmp(g_shutdown_env) == 0) {
        (void)monitor_execute_line(&runtime, "halt");
        return fail("halt should terminate through platform_shutdown");
    }

    if (!g_shutdown_called || g_shutdown_code != 0) {
        return fail("halt should call platform_shutdown with code 0");
    }

    return 0;
}

static int test_disk_commands(void) {
    kernel_runtime_t runtime;

    reset_stubs();
    memset(&runtime, 0, sizeof(runtime));
    monitor_commands_reset(0);

    if (!monitor_execute_line(&runtime, "disk info")) {
        return fail("disk info should continue running");
    }
    if (strstr(g_output, "block_size=512") == NULL ||
        strstr(g_output, "capacity_blocks=16") == NULL) {
        return fail("disk info should report storage geometry");
    }

    reset_output();
    if (!monitor_execute_line(&runtime, "disk read 0")) {
        return fail("disk read should continue running");
    }
    if (strstr(g_output, "StorageImageData") == NULL) {
        return fail("disk read should print an ASCII preview");
    }

    return 0;
}

static int test_regs_and_peek(void) {
    kernel_runtime_t runtime;
    vm_address_space_t address_space;
    uint64_t sample = UINT64_C(0x1122334455667788);
    char command[64];

    reset_stubs();
    memset(&runtime, 0, sizeof(runtime));
    memset(&address_space, 0, sizeof(address_space));
    runtime.interrupts.timer_interrupts = 3U;
    runtime.interrupts.external_interrupts = 2U;
    runtime.interrupts.expected_external_source_id = 9U;
    address_space.satp_value = UINT64_C(0x8000000000000088);
    runtime.address_space = &address_space;
    monitor_commands_reset(0);

    if (!monitor_execute_line(&runtime, "regs")) {
        return fail("regs should continue running");
    }
    if (strstr(g_output, "timer_interrupts=3") == NULL ||
        strstr(g_output, "external_interrupts=2") == NULL) {
        return fail("regs should expose runtime interrupt counters");
    }

    reset_output();
    snprintf(command, sizeof(command), "peek 0x%llx 8",
             (unsigned long long)(uintptr_t)&sample);
    if (!monitor_execute_line(&runtime, command)) {
        return fail("peek should continue running");
    }
    if (strstr(g_output, "0x1122334455667788") == NULL) {
        return fail("peek should print the requested memory value");
    }

    return 0;
}

static int test_pagewalk_and_pte_dump(void) {
    kernel_runtime_t runtime;
    vm_address_space_t address_space;
    static uint64_t root_table[SV39_LEVEL_ENTRIES] __attribute__((aligned(MEMORY_PAGE_SIZE)));

    reset_stubs();
    memset(&runtime, 0, sizeof(runtime));
    memset(&address_space, 0, sizeof(address_space));
    memset(root_table, 0, sizeof(root_table));
    root_table[vpn_index(UINT64_C(0x80000000), 2U)] =
        pte_from_pa(UINT64_C(0x80000000),
                    SV39_PTE_VALID | VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXEC);
    address_space.root_table = root_table;
    runtime.address_space = &address_space;
    monitor_commands_reset(0);

    if (!monitor_execute_line(&runtime, "pagewalk 0x80000000")) {
        return fail("pagewalk should continue running");
    }
    if (strstr(g_output, "leaf=L2") == NULL ||
        strstr(g_output, "pa=0x80000000") == NULL) {
        return fail("pagewalk should resolve the mapped leaf");
    }

    reset_output();
    if (!monitor_execute_line(&runtime, "pte dump 0x80000000")) {
        return fail("pte dump should continue running");
    }
    if (strstr(g_output, "l2=") == NULL) {
        return fail("pte dump should print the raw level-2 entry");
    }

    return 0;
}

int main(void) {
    if (test_help_time_uptime_and_halt() != 0 ||
        test_disk_commands() != 0 ||
        test_regs_and_peek() != 0 ||
        test_pagewalk_and_pte_dump() != 0) {
        return 1;
    }

    return 0;
}
