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
           << "{\"cmd\":\"uart_input\",\"text\":\"linux /bin/minimal-elf\\r\"}\n"
           << run_until_uart_contains_command("linux-compat: exec=real",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("trace=write/exit_group",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("hello",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("course-os> ",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_output\",\"offset\":0}\n"
           << "{\"cmd\":\"quit\"}\n";

    const std::string output = run_cli_script(script.str());

    if (!expect_contains(output,
                         "linux-compat: exec=real",
                         "minimal ELF smoke should use the real Linux compat exec path")) {
        return 1;
    }
    if (!expect_contains(output,
                         "trace=write/exit_group",
                         "minimal ELF smoke should trace real write and exit_group ecalls")) {
        return 1;
    }
    if (!expect_contains(output,
                         "hello",
                         "minimal ELF smoke should print from the user ELF payload")) {
        return 1;
    }
    if (!expect_contains(output,
                         "course-os> ",
                         "minimal ELF smoke should return to the course OS prompt")) {
        return 1;
    }
    if (!expect_contains(output, "\"cmd\":\"quit\"", "quit response should be emitted")) {
        return 1;
    }

    return 0;
}
