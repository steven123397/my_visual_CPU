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
           << "{\"cmd\":\"uart_input\",\"text\":\"linux /bin/busybox --help\\r\"}\n"
           << run_until_uart_contains_command("linux-compat: path=/bin/busybox",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("BusyBox v",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("course-os> ",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"linux /usr/bin/git -h\\r\"}\n"
           << run_until_uart_contains_command("linux-compat: path=/usr/bin/git",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("usage: git",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("course-os> ",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"linux /nope\\r\"}\n"
           << run_until_uart_contains_command("path=/nope errno=2",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("course-os> ",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"help\\r\"}\n"
           << run_until_uart_contains_command("meminfo schedstat fsstat",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("course-os> ",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"git -h\\r\"}\n"
           << run_until_uart_contains_command("error\\r\\ncourse-os> ",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_output\",\"offset\":0}\n"
           << "{\"cmd\":\"quit\"}\n";

    const std::string output = run_cli_script(script.str());

    if (!expect_contains(output,
                         "course-os shell ready",
                         "course OS shell should emit a ready banner")) {
        return 1;
    }
    if (!expect_contains(output,
                         "linux-compat: path=/bin/busybox",
                         "busybox should use explicit linux compat launcher")) {
        return 1;
    }
    if (!expect_contains(output,
                         "BusyBox v",
                         "busybox help should complete through minimal Linux compat syscalls")) {
        return 1;
    }
    if (!expect_contains(output,
                         "Usage: busybox",
                         "busybox help should include a usage line")) {
        return 1;
    }
    if (!expect_contains(output,
                         "linux-compat: path=/usr/bin/git",
                         "git path should use explicit linux compat launcher")) {
        return 1;
    }
    if (!expect_contains(output,
                         "usage: git",
                         "git -h should emit a help usage line")) {
        return 1;
    }
    if (!expect_contains(output,
                         "path=/nope errno=2",
                         "bad linux compat path should report errno 2")) {
        return 1;
    }
    if (!expect_contains(output,
                         "meminfo schedstat fsstat",
                         "course help should remain available after linux compat")) {
        return 1;
    }
    if (!expect_contains(output,
                         "git -h",
                         "direct git fallback command should be visible") ||
        !expect_contains(output,
                         "error",
                         "direct git fallback should remain disabled in v0")) {
        return 1;
    }
    if (!expect_contains(output, "\"cmd\":\"quit\"", "quit response should be emitted")) {
        return 1;
    }

    return 0;
}
