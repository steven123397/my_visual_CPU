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

    script << "{\"cmd\":\"load\","
           << "\"image\":\"guest/generated/course_os_linux_compat_external_shell.elf\","
           << "\"backend\":\"" << backend
           << "\",\"disk\":\"tests/data/storage_basic.txt\"}\n"
           << run_until_uart_contains_command("course-os> ",
                                              DebugBudget::kCourseOsShellBootMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"linux /bin/busybox --help\\r\"}\n"
           << run_until_uart_contains_command("linux-compat: rootfs=external",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("linux-compat: path=/bin/busybox",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("BusyBox v",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << run_until_uart_contains_command("course-os> ",
                                              DebugBudget::kCourseOsShellCommandMaxSteps)
           << "{\"cmd\":\"uart_input\",\"text\":\"linux /usr/bin/git -h\\r\"}\n"
           << run_until_uart_contains_command("usage: git",
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
                         "linux-compat: rootfs=external",
                         "external rootfs smoke should report generated provider")) {
        return 1;
    }
    if (!expect_contains(output,
                         "linux-compat: path=/bin/busybox",
                         "external rootfs busybox should still launch through explicit linux command")) {
        return 1;
    }
    if (!expect_contains(output,
                         "BusyBox v",
                         "external rootfs busybox help path should still emit help")) {
        return 1;
    }
    if (!expect_contains(output,
                         "usage: git",
                         "external rootfs git help path should still emit help")) {
        return 1;
    }
    if (!expect_contains(output,
                         "error",
                         "direct git fallback should remain disabled with external rootfs")) {
        return 1;
    }
    if (!expect_contains(output, "\"cmd\":\"quit\"", "quit response should be emitted")) {
        return 1;
    }

    return 0;
}
