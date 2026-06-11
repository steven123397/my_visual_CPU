#include <cstdio>
#include <string>

#include "../../src/debug/debug_protocol_command.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool expect_parse_error(const char* line) {
    try {
        (void)parse_debug_cli_command(line);
    } catch (...) {
        return true;
    }
    std::fprintf(stderr, "expected parse error for line: %s\n", line);
    return false;
}

}  // namespace

int main() {
    const DebugCliCommand load = parse_debug_cli_command(
        "{\"cmd\":\"load\",\"image\":\"guest\\/interactive_os.elf\",\"backend\":\"pipeline\","
        "\"block_transport\":\"virtio-blk\","
        "\"disk\":\"tests\\/data\\/storage_basic.txt\",\"disk_ready\":false,"
        "\"disk_magic_valid\":true,\"flat\":true,\"addr\":\"0x80000078\"}");
    if (!expect(load.kind == DebugCliCommandKind::Load, "load command kind mismatch")) {
        return 1;
    }
    if (!expect(load.image == "guest/interactive_os.elf", "load image should decode escaped slash")) {
        return 1;
    }
    if (!expect(load.backend == "pipeline", "load backend mismatch")) {
        return 1;
    }
    if (!expect(load.block_transport == "virtio-blk", "load block_transport mismatch")) {
        return 1;
    }
    if (!expect(load.disk == "tests/data/storage_basic.txt", "load disk should decode escaped slash")) {
        return 1;
    }
    if (!expect(load.disk_ready == false, "load disk_ready mismatch")) {
        return 1;
    }
    if (!expect(load.disk_magic_valid == true, "load disk_magic_valid mismatch")) {
        return 1;
    }
    if (!expect(load.flat == true, "load flat mismatch")) {
        return 1;
    }
    if (!expect(load.addr == 0x80000078ULL, "load addr mismatch")) {
        return 1;
    }

    const DebugCliCommand load_payload = parse_debug_cli_command(
        "{\"cmd\":\"load_payload\",\"image\":\"tests\\/data\\/board.dtb\",\"addr\":\"0x88000000\"}");
    if (!expect(load_payload.kind == DebugCliCommandKind::LoadPayload,
                "load_payload command kind mismatch")) {
        return 1;
    }
    if (!expect(load_payload.image == "tests/data/board.dtb",
                "load_payload image should decode escaped slash")) {
        return 1;
    }
    if (!expect(load_payload.addr == 0x88000000ULL, "load_payload addr mismatch")) {
        return 1;
    }

    const DebugCliCommand set_gpr = parse_debug_cli_command(
        "{\"cmd\":\"set_gpr\",\"reg\":\"a1\",\"value\":\"0x88000000\"}");
    if (!expect(set_gpr.kind == DebugCliCommandKind::SetGpr,
                "set_gpr command kind mismatch")) {
        return 1;
    }
    if (!expect(set_gpr.reg_name == "a1", "set_gpr reg name mismatch")) {
        return 1;
    }
    if (!expect(set_gpr.value == 0x88000000ULL, "set_gpr value mismatch")) {
        return 1;
    }

    const DebugCliCommand set_memory = parse_debug_cli_command(
        "{\"cmd\":\"set_memory\",\"addr\":\"0x80000100\",\"value\":\"0x1122334455667788\"}");
    if (!expect(set_memory.kind == DebugCliCommandKind::SetMemory,
                "set_memory command kind mismatch")) {
        return 1;
    }
    if (!expect(set_memory.addr == 0x80000100ULL, "set_memory addr mismatch")) {
        return 1;
    }
    if (!expect(set_memory.value == 0x1122334455667788ULL, "set_memory value mismatch")) {
        return 1;
    }
    if (!expect(set_memory.memory_size == 8, "set_memory default size mismatch")) {
        return 1;
    }
    if (!expect(!set_memory.virtual_address, "set_memory default virtual flag mismatch")) {
        return 1;
    }

    const DebugCliCommand set_memory_virtual = parse_debug_cli_command(
        "{\"cmd\":\"set_memory\",\"addr\":\"0x80000104\",\"value\":\"0xaa\","
        "\"size\":1,\"virtual\":true}");
    if (!expect(set_memory_virtual.kind == DebugCliCommandKind::SetMemory,
                "virtual set_memory command kind mismatch")) {
        return 1;
    }
    if (!expect(set_memory_virtual.addr == 0x80000104ULL, "virtual set_memory addr mismatch")) {
        return 1;
    }
    if (!expect(set_memory_virtual.value == 0xaaULL, "virtual set_memory value mismatch")) {
        return 1;
    }
    if (!expect(set_memory_virtual.memory_size == 1, "virtual set_memory size mismatch")) {
        return 1;
    }
    if (!expect(set_memory_virtual.virtual_address, "virtual set_memory flag mismatch")) {
        return 1;
    }

    const DebugCliCommand set_csr = parse_debug_cli_command(
        "{\"cmd\":\"set_csr\",\"csr\":\"mepc\",\"value\":\"0x80000090\"}");
    if (!expect(set_csr.kind == DebugCliCommandKind::SetCsr,
                "set_csr command kind mismatch")) {
        return 1;
    }
    if (!expect(set_csr.csr_name == "mepc", "set_csr csr name mismatch")) {
        return 1;
    }
    if (!expect(set_csr.value == 0x80000090ULL, "set_csr value mismatch")) {
        return 1;
    }

    const DebugCliCommand break_at =
        parse_debug_cli_command("{\"cmd\":\"break_at\",\"addr\":\"0x80000080\"}");
    if (!expect(break_at.kind == DebugCliCommandKind::BreakAt,
                "break_at command kind mismatch")) {
        return 1;
    }
    if (!expect(break_at.addr == 0x80000080ULL, "break_at addr mismatch")) {
        return 1;
    }

    const DebugCliCommand step_commit =
        parse_debug_cli_command("{\"cmd\":\"step_commit\",\"count\":\"7\"}");
    if (!expect(step_commit.kind == DebugCliCommandKind::StepCommit, "step_commit kind mismatch")) {
        return 1;
    }
    if (!expect(step_commit.count == 7, "step_commit count mismatch")) {
        return 1;
    }

    const DebugCliCommand uart_input =
        parse_debug_cli_command("{\"cmd\":\"uart_input\",\"text\":\"abc\\u0008\"}");
    if (!expect(uart_input.kind == DebugCliCommandKind::UartInput, "uart_input kind mismatch")) {
        return 1;
    }
    if (!expect(uart_input.text.size() == 4, "uart_input text size mismatch")) {
        return 1;
    }
    if (!expect(uart_input.text.substr(0, 3) == "abc", "uart_input text prefix mismatch")) {
        return 1;
    }
    if (!expect(uart_input.text[3] == '\b', "uart_input should decode unicode backspace")) {
        return 1;
    }

    const DebugCliCommand run_until_new =
        parse_debug_cli_command("{\"cmd\":\"run_until_new_uart_contains\","
                                "\"offset\":\"17\",\"text\":\"mycpu-linux# \","
                                "\"max_steps\":\"0x2faf080\"}");
    if (!expect(run_until_new.kind == DebugCliCommandKind::RunUntilNewUartContains,
                "run_until_new_uart_contains kind mismatch")) {
        return 1;
    }
    if (!expect(run_until_new.offset == 17, "run_until_new_uart_contains offset mismatch")) {
        return 1;
    }
    if (!expect(run_until_new.text == "mycpu-linux# ",
                "run_until_new_uart_contains text mismatch")) {
        return 1;
    }
    if (!expect(run_until_new.max_steps == 50000000ULL,
                "run_until_new_uart_contains max_steps mismatch")) {
        return 1;
    }

    const DebugCliCommand jit_dispatch =
        parse_debug_cli_command("{\"cmd\":\"jit_dispatch\"}");
    if (!expect(jit_dispatch.kind == DebugCliCommandKind::JitDispatch,
                "jit_dispatch command kind mismatch")) {
        return 1;
    }

    if (!expect_parse_error("{\"cmd\":\"bogus\"}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"snapshot\"} trailing")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"load\",\"image\":\"x.bin\",\"addr\":\"0x10junk\"}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"set_gpr\",\"reg\":\"a0\",\"value\":\"123abc\"}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"set_memory\",\"addr\":\"0x80000100\"}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"set_memory\",\"addr\":\"0x80000100\","
                            "\"value\":\"0x1\",\"size\":3}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"set_memory\",\"addr\":\"0x80000100\","
                            "\"value\":\"0x1\",\"virtual\":\"yes\"}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"set_csr\",\"csr\":768,\"value\":\"0x1\"}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"break_at\"}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"step_commit\",\"count\":\"-1\"}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"uart_output\",\"offset\":\"\"}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"run_until_halt\",\"max_steps\":\"18446744073709551616\"}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"run_until_new_uart_contains\","
                            "\"offset\":\" 17\",\"text\":\">\",\"max_steps\":\"1\"}")) {
        return 1;
    }
    if (!expect_parse_error("{\"cmd\":\"step_cycle\",\"count\":18446744073709551616}")) {
        return 1;
    }

    return 0;
}
