#include "monitor_commands.h"

#include <stddef.h>
#include <stdint.h>

#include "monitor.h"
#include "platform.h"
#include "storage.h"
#include "vm_debug.h"

typedef struct MonitorToken {
    const char* start;
    size_t length;
} monitor_token_t;

static uint64_t g_monitor_boot_mtime = 0;
static const char kMonitorHelpText[] =
    "help echo time uptime halt disk regs peek pagewalk pte";
static const char kDiskUsageText[] = "usage: disk info | disk read <lba>";
static const char kPeekUsageText[] = "usage: peek <addr> [1|2|4|8]";
static const char kPagewalkUsageText[] = "usage: pagewalk <addr>";
static const char kPteDumpUsageText[] = "usage: pte dump <addr>";

static const char* monitor_skip_spaces(const char* text) {
    while (text != NULL && *text == ' ') {
        ++text;
    }
    return text;
}

static bool monitor_token_eq(monitor_token_t token, const char* word) {
    size_t index = 0;

    if (token.start == NULL || word == NULL) {
        return false;
    }

    while (index < token.length && word[index] != '\0') {
        if (token.start[index] != word[index]) {
            return false;
        }
        ++index;
    }

    return index == token.length && word[index] == '\0';
}

static const char* monitor_next_token(const char* text, monitor_token_t* token) {
    const char* cursor = monitor_skip_spaces(text);

    if (token == NULL) {
        return cursor;
    }

    token->start = cursor;
    token->length = 0;
    if (cursor == NULL) {
        return NULL;
    }

    while (cursor[token->length] != '\0' && cursor[token->length] != ' ') {
        ++token->length;
    }

    return cursor + token->length;
}

static bool monitor_cursor_at_end(const char* cursor) {
    cursor = monitor_skip_spaces(cursor);
    return cursor == NULL || *cursor == '\0';
}

static bool monitor_parse_u64(monitor_token_t token, uint64_t* out_value) {
    uint64_t value = 0;
    uint64_t base = 10;
    size_t index = 0;

    if (token.start == NULL || token.length == 0 || out_value == NULL) {
        return false;
    }

    if (token.length > 2 && token.start[0] == '0' &&
        (token.start[1] == 'x' || token.start[1] == 'X')) {
        base = 16;
        index = 2;
    }

    if (index >= token.length) {
        return false;
    }

    while (index < token.length) {
        const char ch = token.start[index];
        uint64_t digit = 0;

        if (ch >= '0' && ch <= '9') {
            digit = (uint64_t)(ch - '0');
        } else if (base == 16 && ch >= 'a' && ch <= 'f') {
            digit = (uint64_t)(10 + ch - 'a');
        } else if (base == 16 && ch >= 'A' && ch <= 'F') {
            digit = (uint64_t)(10 + ch - 'A');
        } else {
            return false;
        }

        if (digit >= base) {
            return false;
        }

        if (value > (UINT64_MAX - digit) / base) {
            return false;
        }

        value = value * base + digit;
        ++index;
    }

    *out_value = value;
    return true;
}

static void monitor_write_captured_token(monitor_token_t token) {
    size_t index;

    for (index = 0; index < token.length; ++index) {
        monitor_write_char(token.start[index]);
    }
}

static bool monitor_handle_help(void) {
    monitor_write_line(kMonitorHelpText);
    return true;
}

static bool monitor_handle_time(void) {
    monitor_write_text("mtime=");
    monitor_write_uint64(platform_clint_read_mtime());
    monitor_write_line("");
    return true;
}

static bool monitor_handle_uptime(void) {
    const uint64_t now = platform_clint_read_mtime();

    monitor_write_text("uptime=");
    monitor_write_uint64(now - g_monitor_boot_mtime);
    monitor_write_line("");
    return true;
}

static bool monitor_handle_halt(void) {
    monitor_write_line("halting");
    platform_shutdown(0);
}

static bool monitor_handle_echo(const char* command) {
    const char* payload = monitor_skip_spaces(command + 4);

    monitor_write_line(payload != NULL ? payload : "");
    return true;
}

static bool monitor_handle_disk_info(void) {
    storage_info_t info;

    if (!storage_probe(&info)) {
        monitor_write_text("disk probe failed status=");
        monitor_write_hex64(storage_status());
        monitor_write_text(" error=");
        monitor_write_hex64(storage_error());
        monitor_write_line("");
        return true;
    }

    monitor_write_text("disk magic=");
    monitor_write_hex64(info.magic);
    monitor_write_text(" version=");
    monitor_write_uint64(info.version);
    monitor_write_text(" block_size=");
    monitor_write_uint64(info.block_size);
    monitor_write_text(" capacity_blocks=");
    monitor_write_uint64(info.capacity_blocks);
    monitor_write_text(" status=");
    monitor_write_hex64(info.status);
    monitor_write_line("");
    return true;
}

static bool monitor_handle_disk_read(const char* cursor) {
    monitor_token_t lba_token;
    uint64_t lba = 0;
    uint8_t block[512];
    uint64_t status;

    cursor = monitor_next_token(cursor, &lba_token);
    if (!monitor_parse_u64(lba_token, &lba) || !monitor_cursor_at_end(cursor)) {
        monitor_write_line(kDiskUsageText);
        return true;
    }

    status = storage_read_block(lba, block);
    if (status != 0U) {
        monitor_write_text("disk read failed status=");
        monitor_write_hex64(status);
        monitor_write_text(" error=");
        monitor_write_hex64(storage_error());
        monitor_write_line("");
        return true;
    }

    monitor_write_text("disk lba=");
    monitor_write_uint64(lba);
    monitor_write_text(" ascii=");
    monitor_write_preview_ascii(block, sizeof(block));
    monitor_write_line("");
    return true;
}

static bool monitor_handle_disk(const char* cursor) {
    monitor_token_t subcommand;

    cursor = monitor_next_token(cursor, &subcommand);
    if (monitor_token_eq(subcommand, "info")) {
        if (!monitor_cursor_at_end(cursor)) {
            monitor_write_line(kDiskUsageText);
            return true;
        }
        return monitor_handle_disk_info();
    }

    if (monitor_token_eq(subcommand, "read")) {
        return monitor_handle_disk_read(cursor);
    }

    monitor_write_line(kDiskUsageText);
    return true;
}

static bool monitor_handle_regs(const kernel_runtime_t* runtime) {
    const supervisor_runtime_interrupt_state_t* interrupts =
        kernel_runtime_interrupt_state_const(runtime);
    const vm_address_space_t* address_space = kernel_runtime_address_space(runtime);
    const uint64_t satp_value =
        address_space != NULL ? vm_address_space_satp_value(address_space) : 0U;

    monitor_write_text("satp=");
    monitor_write_hex64(satp_value);
    monitor_write_text(" mtime=");
    monitor_write_uint64(platform_clint_read_mtime());
    monitor_write_text(" timer_interrupts=");
    monitor_write_uint64(
        supervisor_runtime_interrupt_state_timer_interrupts(interrupts));
    monitor_write_text(" external_interrupts=");
    monitor_write_uint64(
        supervisor_runtime_interrupt_state_external_interrupts(interrupts));
    monitor_write_text(" expected_external_source=");
    monitor_write_uint64(
        supervisor_runtime_interrupt_state_expected_external_source_id(
            interrupts));
    monitor_write_line("");
    return true;
}

static bool monitor_handle_peek(const kernel_runtime_t* runtime,
                                const char* cursor) {
    monitor_token_t address_token;
    monitor_token_t width_token;
    const vm_address_space_t* address_space = kernel_runtime_address_space(runtime);
    uint64_t address = 0;
    uint64_t width = 8;
    vm_debug_read_result_t result;

    cursor = monitor_next_token(cursor, &address_token);
    if (!monitor_parse_u64(address_token, &address)) {
        monitor_write_line(kPeekUsageText);
        return true;
    }

    cursor = monitor_next_token(cursor, &width_token);
    if (width_token.length != 0U && !monitor_parse_u64(width_token, &width)) {
        monitor_write_line(kPeekUsageText);
        return true;
    }

    if (!monitor_cursor_at_end(cursor)) {
        monitor_write_line(kPeekUsageText);
        return true;
    }

    if (width != 1U && width != 2U && width != 4U && width != 8U) {
        monitor_write_line(kPeekUsageText);
        return true;
    }

    if (!vm_debug_read(address_space, (uintptr_t)address, (size_t)width, &result)) {
        monitor_write_text("peek miss va=");
        monitor_write_hex64(address);
        monitor_write_text(" width=");
        monitor_write_uint64(width);
        monitor_write_line("");
        return true;
    }

    monitor_write_hex64(address);
    monitor_write_text(": ");
    monitor_write_hex64(result.value);
    monitor_write_line("");
    return true;
}

static bool monitor_handle_pagewalk(const kernel_runtime_t* runtime,
                                    const char* cursor) {
    monitor_token_t address_token;
    const vm_address_space_t* address_space = kernel_runtime_address_space(runtime);
    uint64_t address = 0;
    vm_debug_walk_result_t result;

    cursor = monitor_next_token(cursor, &address_token);
    if (!monitor_parse_u64(address_token, &address) ||
        !monitor_cursor_at_end(cursor)) {
        monitor_write_line(kPagewalkUsageText);
        return true;
    }

    if (!vm_debug_walk(address_space, (uintptr_t)address, &result)) {
        monitor_write_text("pagewalk miss va=");
        monitor_write_hex64(address);
        monitor_write_line("");
        return true;
    }

    monitor_write_text("pagewalk va=");
    monitor_write_hex64(address);
    monitor_write_text(" leaf=L");
    monitor_write_uint64(result.leaf_level);
    monitor_write_text(" pa=");
    monitor_write_hex64((uint64_t)result.resolved_pa);
    monitor_write_text(" pte=");
    monitor_write_hex64(result.leaf_pte);
    monitor_write_line("");
    return true;
}

static bool monitor_handle_pte_dump(const kernel_runtime_t* runtime,
                                    const char* cursor) {
    monitor_token_t address_token;
    const vm_address_space_t* address_space = kernel_runtime_address_space(runtime);
    uint64_t address = 0;
    vm_debug_walk_result_t result;
    unsigned level;

    cursor = monitor_next_token(cursor, &address_token);
    if (!monitor_parse_u64(address_token, &address) ||
        !monitor_cursor_at_end(cursor)) {
        monitor_write_line(kPteDumpUsageText);
        return true;
    }

    if (!vm_debug_walk(address_space, (uintptr_t)address, &result)) {
        monitor_write_text("pte miss va=");
        monitor_write_hex64(address);
        monitor_write_line("");
        return true;
    }

    for (level = 2U;; --level) {
        monitor_write_text("l");
        monitor_write_uint64(level);
        monitor_write_text("=");
        if (result.entry_valid[level]) {
            monitor_write_hex64(result.entries[level]);
        } else {
            monitor_write_text("invalid");
        }
        if (level == 0U) {
            break;
        }
        monitor_write_text(" ");
    }
    monitor_write_line("");
    return true;
}

void monitor_commands_reset(uint64_t boot_mtime) {
    g_monitor_boot_mtime = boot_mtime;
}

bool monitor_execute_line(kernel_runtime_t* runtime, const char* line) {
    const char* command = monitor_skip_spaces(line);
    monitor_token_t primary;
    const char* cursor;

    if (command == NULL || *command == '\0') {
        return true;
    }

    cursor = monitor_next_token(command, &primary);
    if (monitor_token_eq(primary, "help")) {
        return monitor_handle_help();
    }

    if (monitor_token_eq(primary, "time")) {
        return monitor_handle_time();
    }

    if (monitor_token_eq(primary, "uptime")) {
        return monitor_handle_uptime();
    }

    if (monitor_token_eq(primary, "halt")) {
        return monitor_handle_halt();
    }

    if (monitor_token_eq(primary, "echo")) {
        return monitor_handle_echo(command);
    }

    if (monitor_token_eq(primary, "disk")) {
        return monitor_handle_disk(cursor);
    }

    if (monitor_token_eq(primary, "regs")) {
        return monitor_handle_regs(runtime);
    }

    if (monitor_token_eq(primary, "peek")) {
        return monitor_handle_peek(runtime, cursor);
    }

    if (monitor_token_eq(primary, "pagewalk")) {
        return monitor_handle_pagewalk(runtime, cursor);
    }

    if (monitor_token_eq(primary, "pte")) {
        monitor_token_t subcommand;

        cursor = monitor_next_token(cursor, &subcommand);
        if (monitor_token_eq(subcommand, "dump")) {
            return monitor_handle_pte_dump(runtime, cursor);
        }
        monitor_write_text("unknown pte subcommand: ");
        monitor_write_captured_token(subcommand);
        monitor_write_line("");
        return true;
    }

    monitor_write_text("unknown command: ");
    monitor_write_captured_token(primary);
    monitor_write_line("");
    return true;
}
