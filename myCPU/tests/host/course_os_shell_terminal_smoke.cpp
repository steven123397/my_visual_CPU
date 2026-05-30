#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

#include "../../src/debug/debug_budget.h"
#include "../../src/debug/debug_protocol.h"

namespace {

bool expect_contains(const std::string& haystack,
                     const char* needle,
                     const char* message) {
    if (haystack.find(needle) == std::string::npos) {
        std::fprintf(stderr, "%s\n", message);
        std::fprintf(stderr, "output was:\n%s\n", haystack.c_str());
        return false;
    }
    return true;
}

std::string run_until_uart_contains_command(const char* text,
                                            std::uint64_t max_steps) {
    std::ostringstream script;
    script << "{\"cmd\":\"run_until_uart_contains\",\"text\":\"" << text
           << "\",\"max_steps\":" << max_steps << "}\n";
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

    script << "{\"cmd\":\"load\",\"image\":\"guest/course_os_shell.elf\","
           << "\"backend\":\"" << backend
           << "\",\"disk\":\"tests/data/storage_basic.txt\"}\n"
           << run_until_uart_contains_command("course-os> ",
                                              DebugBudget::kCourseOsShellBootMaxSteps)
           << "{\"cmd\":\"uart_output\",\"offset\":0}\n"
           << "{\"cmd\":\"uart_input\",\"text\":\"help\\r\"}\n"
           << run_until_uart_contains_command("meminfo schedstat fsstat",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("course-os> ",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"echo file > /tmp/a\\r\"}\n"
           << run_until_uart_contains_command("course-os> ",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"cat /tmp/a\\r\"}\n"
           << run_until_uart_contains_command("file\\ncourse-os> ",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"echo pipe | cat\\r\"}\n"
           << run_until_uart_contains_command("pipe\\ncourse-os> ",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"ps\\r\"}\n"
           << run_until_uart_contains_command("pid=",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"meminfo\\r\"}\n"
           << run_until_uart_contains_command("total=",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"cpuinfo\\r\"}\n"
           << run_until_uart_contains_command("isa=rv64im",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"exec hello\\r\"}\n"
           << run_until_uart_contains_command("program=hello",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"exec crashdemo\\r\"}\n"
           << run_until_uart_contains_command("crash=isolated",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"crashlog\\r\"}\n"
           << run_until_uart_contains_command("reason=user-crash",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_output\",\"offset\":0}\n"
           << "{\"cmd\":\"quit\"}\n";

    const std::string output = run_cli_script(script.str());

    if (!expect_contains(output,
                         "course-os shell ready",
                         "course OS shell should emit a ready banner")) {
        return 1;
    }
    if (!expect_contains(output, "course-os> ", "course OS shell prompt should settle")) {
        return 1;
    }
    if (!expect_contains(output,
                         "meminfo schedstat fsstat",
                         "help should list proc shortcut commands")) {
        return 1;
    }
    if (!expect_contains(output, "file", "file redirection should roundtrip")) {
        return 1;
    }
    if (!expect_contains(output, "pipe", "pipe command should roundtrip")) {
        return 1;
    }
    if (!expect_contains(output, "pid=", "ps should expose process rows")) {
        return 1;
    }
    if (!expect_contains(output, "total=", "meminfo shortcut should read procfs")) {
        return 1;
    }
    if (!expect_contains(output, "isa=rv64im", "cpuinfo shortcut should read procfs")) {
        return 1;
    }
    if (!expect_contains(output, "program=hello", "exec hello should run")) {
        return 1;
    }
    if (!expect_contains(output,
                         "reason=user-crash",
                         "crashlog should remain readable after crashdemo")) {
        return 1;
    }
    if (!expect_contains(output, "\"cmd\":\"quit\"", "quit response should be emitted")) {
        return 1;
    }

    return 0;
}
