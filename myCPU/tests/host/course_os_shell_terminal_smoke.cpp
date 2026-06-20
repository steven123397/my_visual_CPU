#include <cstdint>
#include <sstream>
#include <string>

#include "../../src/debug/debug_budget.h"
#include "terminal_smoke_harness.h"

// 浏览器终端同源的 host smoke：通过 debug CLI 驱动 guest_course_os_shell_demo，
// 覆盖 prompt、procfs 快捷命令、FD/FS、pipe、ELF/libc 和 crash evidence。

namespace {

std::string wait_for_course_os_text(const char* text) {
    return terminal_smoke::debug_cli_run_until_uart_contains_command(
        text, DebugBudget::kCourseOsShellCommandMaxSteps);
}

}  // namespace

int main(int argc, char** argv) {
    const char* backend = argc > 1 ? argv[1] : "functional";
    std::ostringstream script;

    // 脚本按真实 UART 输入推进，避免只检查一次性 kernel_alpha marker。
    script << "{\"cmd\":\"load\",\"image\":\"guest/course_os_shell.elf\","
           << "\"backend\":\"" << backend
           << "\",\"disk\":\"tests/data/storage_basic.txt\"}\n"
           << terminal_smoke::debug_cli_run_until_uart_contains_command(
                  "course-os> ", DebugBudget::kCourseOsShellBootMaxSteps)
           << "{\"cmd\":\"uart_output\",\"offset\":0}\n"
           << "{\"cmd\":\"uart_input\",\"text\":\"help\\r\"}\n"
           << wait_for_course_os_text("meminfo schedstat fsstat")
           << wait_for_course_os_text("course-os> ")
           << "{\"cmd\":\"uart_input\",\"text\":\"echo file > /tmp/a\\r\"}\n"
           << wait_for_course_os_text("course-os> ")
           << "{\"cmd\":\"uart_input\",\"text\":\"cat /tmp/a\\r\"}\n"
           << wait_for_course_os_text("file\\ncourse-os> ")
           << "{\"cmd\":\"uart_input\",\"text\":\"echo pipe | cat\\r\"}\n"
           << wait_for_course_os_text("pipe\\ncourse-os> ")
           << "{\"cmd\":\"uart_input\",\"text\":\"ps\\r\"}\n"
           << wait_for_course_os_text("pid=")
           << "{\"cmd\":\"uart_input\",\"text\":\"meminfo\\r\"}\n"
           << wait_for_course_os_text("total=")
           << "{\"cmd\":\"uart_input\",\"text\":\"cpuinfo\\r\"}\n"
           << wait_for_course_os_text("isa=rv64im")
           << "{\"cmd\":\"uart_input\",\"text\":\"exec hello\\r\"}\n"
           << wait_for_course_os_text("program=hello")
           << "{\"cmd\":\"uart_input\",\"text\":\"exec crashdemo\\r\"}\n"
           << wait_for_course_os_text("crash=isolated")
           << "{\"cmd\":\"uart_input\",\"text\":\"crashlog\\r\"}\n"
           << wait_for_course_os_text("reason=user-crash")
           << "{\"cmd\":\"uart_output\",\"offset\":0}\n"
           << "{\"cmd\":\"quit\"}\n";

    const std::string output = terminal_smoke::run_debug_cli_script(script.str());

    if (!terminal_smoke::expect_contains(output,
                                         "course-os shell ready",
                                         "course OS shell should emit a ready banner")) {
        return 1;
    }
    if (!terminal_smoke::expect_contains(output,
                                         "course-os> ",
                                         "course OS shell prompt should settle")) {
        return 1;
    }
    if (!terminal_smoke::expect_contains(output,
                                         "meminfo schedstat fsstat",
                                         "help should list proc shortcut commands")) {
        return 1;
    }
    if (!terminal_smoke::expect_contains(output,
                                         "file",
                                         "file redirection should roundtrip")) {
        return 1;
    }
    if (!terminal_smoke::expect_contains(output, "pipe", "pipe command should roundtrip")) {
        return 1;
    }
    if (!terminal_smoke::expect_contains(output, "pid=", "ps should expose process rows")) {
        return 1;
    }
    if (!terminal_smoke::expect_contains(output,
                                         "total=",
                                         "meminfo shortcut should read procfs")) {
        return 1;
    }
    if (!terminal_smoke::expect_contains(output,
                                         "isa=rv64im",
                                         "cpuinfo shortcut should read procfs")) {
        return 1;
    }
    if (!terminal_smoke::expect_contains(output, "program=hello", "exec hello should run")) {
        return 1;
    }
    if (!terminal_smoke::expect_contains(output,
                                         "reason=user-crash",
                                         "crashlog should remain readable after crashdemo")) {
        return 1;
    }
    if (!terminal_smoke::expect_contains(output,
                                         "\"cmd\":\"quit\"",
                                         "quit response should be emitted")) {
        return 1;
    }

    return 0;
}
