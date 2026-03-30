#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "../../src/debug/debug_protocol.h"

namespace {

bool expect_contains(const std::string& haystack, const char* needle, const char* message) {
    if (haystack.find(needle) == std::string::npos) {
        std::fprintf(stderr, "%s\n", message);
        std::fprintf(stderr, "output was:\n%s\n", haystack.c_str());
        return false;
    }
    return true;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

bool expect_line_with_fields(const std::vector<std::string>& lines,
                             const std::string& output,
                             std::initializer_list<const char*> needles,
                             const char* message) {
    for (const std::string& line : lines) {
        bool matched = true;
        for (const char* needle : needles) {
            if (line.find(needle) == std::string::npos) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return true;
        }
    }

    std::fprintf(stderr, "%s\n", message);
    std::fprintf(stderr, "output was:\n%s\n", output.c_str());
    return false;
}

std::string run_until_uart_contains_command(const char* text, int max_steps) {
    std::ostringstream script;
    script << "{\"cmd\":\"run_until_uart_contains\",\"text\":\"" << text
           << "\",\"max_steps\":" << max_steps << "}\n";
    return script.str();
}

std::string run_until_halt_command(int max_steps) {
    std::ostringstream script;
    script << "{\"cmd\":\"run_until_halt\",\"max_steps\":" << max_steps << "}\n";
    return script.str();
}

std::string run_cli_script(const std::string& script) {
    std::istringstream in(script);
    std::ostringstream out;
    std::ostringstream err;

    const int status = run_debug_cli(in, out, err);
    if (status != 0) {
        std::fprintf(stderr, "debug cli exited with status %d\n", status);
        std::fprintf(stderr, "stderr:\n%s\n", err.str().c_str());
        std::exit(1);
    }

    return out.str();
}

}  // namespace

int main(int argc, char** argv) {
    const char* backend = argc > 1 ? argv[1] : "functional";
    std::ostringstream script;

    script << "{\"cmd\":\"load\",\"image\":\"guest/interactive_os.elf\",\"backend\":\""
           << backend << "\",\"disk\":\"tests/data/storage_basic.txt\"}\n"
           << run_until_uart_contains_command("monitor> ", 5000000)
           << "{\"cmd\":\"uart_output\",\"offset\":0}\n"
           << "{\"cmd\":\"uart_input\",\"text\":\"help\\r\"}\n"
           << run_until_uart_contains_command("help echo time uptime halt disk regs peek pagewalk pte", 500000)
           << "{\"cmd\":\"uart_output\",\"offset\":0}\n"
           << "{\"cmd\":\"uart_input\",\"text\":\"echo hi\\r\"}\n"
           << run_until_uart_contains_command("hi\\r\\nmonitor> ", 500000)
           << "{\"cmd\":\"uart_output\",\"offset\":0}\n"
           << "{\"cmd\":\"uart_input\",\"text\":\"time\\r\"}\n"
           << run_until_uart_contains_command("mtime=", 500000)
           << "{\"cmd\":\"uart_input\",\"text\":\"uptime\\r\"}\n"
           << run_until_uart_contains_command("uptime=", 500000)
           << "{\"cmd\":\"uart_input\",\"text\":\"disk info\\r\"}\n"
           << run_until_uart_contains_command("block_size=", 500000)
           << "{\"cmd\":\"uart_input\",\"text\":\"disk read 0\\r\"}\n"
           << run_until_uart_contains_command("StorageImageData", 500000)
           << "{\"cmd\":\"uart_input\",\"text\":\"regs\\r\"}\n"
           << run_until_uart_contains_command("timer_interrupts=", 500000)
           << "{\"cmd\":\"uart_input\",\"text\":\"peek 0x80000000 8\\r\"}\n"
           << run_until_uart_contains_command("0x80000000:", 500000)
           << "{\"cmd\":\"uart_input\",\"text\":\"peek 0x40000000 4\\r\"}\n"
           << run_until_uart_contains_command("peek miss va=0x40000000 width=4", 500000)
           << "{\"cmd\":\"uart_input\",\"text\":\"pagewalk 0x80000000\\r\"}\n"
           << run_until_uart_contains_command("leaf=L2", 500000)
           << "{\"cmd\":\"uart_input\",\"text\":\"pte dump 0x80000000\\r\"}\n"
           << run_until_uart_contains_command("l2=", 500000)
           << "{\"cmd\":\"uart_input\",\"text\":\"halt\\r\"}\n"
           << run_until_halt_command(500000)
           << "{\"cmd\":\"snapshot\"}\n"
           << "{\"cmd\":\"uart_output\",\"offset\":0}\n"
           << "{\"cmd\":\"quit\"}\n";

    const std::string output =
        run_cli_script(script.str());
    const std::vector<std::string> lines = split_lines(output);

    if (!expect_contains(output, "\"cmd\":\"load\"", "load response should be emitted")) {
        return 1;
    }
    if (!expect_line_with_fields(lines,
                                 output,
                                 {
                                     "\"type\":\"uart_output\"",
                                     "\"text\":\"KMV",
                                     "monitor> ",
                                 },
                                 "interactive_os should emit a boot banner and prompt")) {
        return 1;
    }
    if (!expect_contains(output,
                         "help echo time uptime halt disk regs peek pagewalk pte",
                         "help command should list the extended command set")) {
        return 1;
    }
    if (!expect_contains(output, "mtime=", "time command should report the current mtime")) {
        return 1;
    }
    if (!expect_contains(output, "uptime=", "uptime command should report ticks since boot")) {
        return 1;
    }
    if (!expect_contains(output, "block_size=", "disk info should report storage geometry")) {
        return 1;
    }
    if (!expect_contains(output, "StorageImageData", "disk read should surface the storage payload preview")) {
        return 1;
    }
    if (!expect_contains(output, "timer_interrupts=", "regs should expose runtime counters")) {
        return 1;
    }
    if (!expect_contains(output,
                         "peek miss va=0x40000000 width=4",
                         "peek should reject unmapped virtual addresses without faulting")) {
        return 1;
    }
    if (!expect_contains(output, "leaf=L2", "pagewalk should resolve the kernel identity map")) {
        return 1;
    }
    if (!expect_contains(output, "l2=", "pte dump should print the raw level entries")) {
        return 1;
    }
    if (!expect_contains(output, "hi", "echo command should print its payload")) {
        return 1;
    }
    if (!expect_line_with_fields(lines,
                                 output,
                                 {
                                     "\"type\":\"snapshot\"",
                                     "\"halted\":true",
                                 },
                                 "halt command should eventually stop the guest")) {
        return 1;
    }
    if (!expect_contains(output, "\"cmd\":\"quit\"", "quit response should be emitted")) {
        return 1;
    }

    return 0;
}
