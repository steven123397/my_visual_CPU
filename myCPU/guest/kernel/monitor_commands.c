#include "monitor_commands.h"

#include <stddef.h>
#include <stdint.h>

#include "monitor.h"
#include "platform.h"
#include "storage.h"
#include "vm_debug.h"
#include "vm_private.h"

typedef struct MonitorToken {
    const char* start;
    size_t length;
} monitor_token_t;

static uint64_t g_monitor_boot_mtime = 0;

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

        value = value * base + digit;
        ++index;
    }

    *out_value = value;
    return true;
}

static void monitor_write_char(char ch) {
    char text[2];

    text[0] = ch;
    text[1] = '\0';
    monitor_write_text(text);
}

static void monitor_write_uint64(uint64_t value) {
    char buffer[32];
    size_t index = sizeof(buffer) - 1U;

    buffer[index] = '\0';
    do {
        buffer[--index] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U && index > 0U);

    monitor_write_text(&buffer[index]);
}

static void monitor_write_hex64(uint64_t value) {
    char buffer[19];
    static const char kHexDigits[] = "0123456789abcdef";
    size_t start = 0U;
    size_t index;

    buffer[0] = '0';
    buffer[1] = 'x';
    for (index = 0; index < 16U; ++index) {
        const unsigned shift = (unsigned)((15U - index) * 4U);
        buffer[2U + index] = kHexDigits[(value >> shift) & 0xfU];
    }
    buffer[18] = '\0';

    while (start < 15U && buffer[2U + start] == '0') {
        ++start;
    }

    monitor_write_text("0x");
    monitor_write_text(&buffer[2U + start]);
}

static void monitor_write_captured_token(monitor_token_t token) {
    size_t index;

    for (index = 0; index < token.length; ++index) {
        monitor_write_char(token.start[index]);
    }
}

static void monitor_write_preview_ascii(const uint8_t* data, size_t length) {
    size_t index;

    for (index = 0; index < length; ++index) {
        const uint8_t byte = data[index];
        if (byte == 0U) {
            break;
        }
        if (byte < 0x20U || byte > 0x7eU) {
            monitor_write_char('.');
            continue;
        }
        monitor_write_char((char)byte);
    }
}

static bool monitor_handle_help(void) {
    monitor_write_line("help echo time uptime halt disk regs peek pagewalk pte");
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
    if (!monitor_parse_u64(lba_token, &lba)) {
        monitor_write_line("usage: disk read <lba>");
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
        return monitor_handle_disk_info();
    }

    if (monitor_token_eq(subcommand, "read")) {
        return monitor_handle_disk_read(cursor);
    }

    monitor_write_line("usage: disk info | disk read <lba>");
    return true;
}

static bool monitor_handle_regs(const kernel_runtime_t* runtime) {
    const supervisor_runtime_interrupt_state_t* interrupts =
        runtime != NULL ? &runtime->interrupts : NULL;
    const vm_address_space_t* address_space =
        runtime != NULL ? runtime->address_space : NULL;
    const uint64_t satp_value =
        address_space != NULL ? address_space->satp_value : 0U;

    monitor_write_text("satp=");
    monitor_write_hex64(satp_value);
    monitor_write_text(" mtime=");
    monitor_write_uint64(platform_clint_read_mtime());
    monitor_write_text(" timer_interrupts=");
    monitor_write_uint64(interrupts != NULL ? interrupts->timer_interrupts : 0U);
    monitor_write_text(" external_interrupts=");
    monitor_write_uint64(interrupts != NULL ? interrupts->external_interrupts : 0U);
    monitor_write_text(" expected_external_source=");
    monitor_write_uint64(interrupts != NULL
                             ? interrupts->expected_external_source_id
                             : 0U);
    monitor_write_line("");
    return true;
}

static bool monitor_handle_peek(const char* cursor) {
    monitor_token_t address_token;
    monitor_token_t width_token;
    uint64_t address = 0;
    uint64_t width = 8;
    uint64_t value = 0;

    cursor = monitor_next_token(cursor, &address_token);
    if (!monitor_parse_u64(address_token, &address)) {
        monitor_write_line("usage: peek <addr> [1|2|4|8]");
        return true;
    }

    cursor = monitor_next_token(cursor, &width_token);
    if (width_token.length != 0U && !monitor_parse_u64(width_token, &width)) {
        monitor_write_line("usage: peek <addr> [1|2|4|8]");
        return true;
    }

    switch (width) {
    case 1:
        value = *(const volatile uint8_t*)(uintptr_t)address;
        break;
    case 2:
        value = *(const volatile uint16_t*)(uintptr_t)address;
        break;
    case 4:
        value = *(const volatile uint32_t*)(uintptr_t)address;
        break;
    case 8:
        value = *(const volatile uint64_t*)(uintptr_t)address;
        break;
    default:
        monitor_write_line("usage: peek <addr> [1|2|4|8]");
        return true;
    }

    monitor_write_hex64(address);
    monitor_write_text(": ");
    monitor_write_hex64(value);
    monitor_write_line("");
    return true;
}

static bool monitor_handle_pagewalk(const kernel_runtime_t* runtime,
                                    const char* cursor) {
    monitor_token_t address_token;
    uint64_t address = 0;
    vm_debug_walk_result_t result;

    cursor = monitor_next_token(cursor, &address_token);
    if (!monitor_parse_u64(address_token, &address)) {
        monitor_write_line("usage: pagewalk <addr>");
        return true;
    }

    if (!vm_debug_walk(runtime != NULL ? runtime->address_space : NULL,
                       (uintptr_t)address,
                       &result)) {
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
    uint64_t address = 0;
    vm_debug_walk_result_t result;
    unsigned level;

    cursor = monitor_next_token(cursor, &address_token);
    if (!monitor_parse_u64(address_token, &address)) {
        monitor_write_line("usage: pte dump <addr>");
        return true;
    }

    if (!vm_debug_walk(runtime != NULL ? runtime->address_space : NULL,
                       (uintptr_t)address,
                       &result)) {
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
        return monitor_handle_peek(cursor);
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
