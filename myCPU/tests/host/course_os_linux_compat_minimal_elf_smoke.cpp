#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "../../src/debug/debug_budget.h"
#include "../../src/debug/debug_session.h"

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

BackendKind parse_backend_kind(const char* backend) {
    if (std::strcmp(backend, "functional") == 0) {
        return BackendKind::Functional;
    }
    if (std::strcmp(backend, "pipeline") == 0) {
        return BackendKind::Pipeline;
    }
    std::fprintf(stderr, "unknown backend: %s\n", backend);
    std::exit(1);
}

bool run_until_new_uart_contains(DebugSession& session,
                                 size_t offset,
                                 const char* needle,
                                 std::uint64_t max_steps,
                                 DebugSession::UartOutputChunk& chunk) {
    try {
        chunk = session.run_until_new_uart_contains(offset, needle, max_steps);
        return true;
    } catch (const std::runtime_error& error) {
        chunk = session.uart_output(offset);
        std::fprintf(stderr, "%s\n", error.what());
        std::fprintf(stderr,
                     "minimal ELF smoke failed while waiting for command UART text: %s\n",
                     needle);
        std::fprintf(stderr,
                     "command output since offset %zu was:\n%s\n",
                     offset,
                     chunk.text.c_str());
        return false;
    }
}

}  // namespace

int main(int argc, char** argv) {
    const char* backend = argc > 1 ? argv[1] : "functional";

    DebugSession session;
    session.load_elf("guest/course_os_shell.elf",
                     parse_backend_kind(backend),
                     BlockTransport::SimpleStorage,
                     "tests/data/storage_basic.txt");
    session.run_until_uart_contains("course-os> ", DebugBudget::kCourseOsShellBootMaxSteps);

    const DebugSession::UartOutputChunk boot_chunk = session.uart_output(0);
    if (!expect_contains(boot_chunk.text,
                         "course-os shell ready",
                         "course OS shell should emit a ready banner") ||
        !expect_contains(boot_chunk.text,
                         "course-os> ",
                         "course OS shell prompt should settle")) {
        return 1;
    }

    const size_t command_offset = boot_chunk.next_offset;
    session.uart_input("linux /bin/minimal-elf\r");

    DebugSession::UartOutputChunk command_chunk{};
    if (!run_until_new_uart_contains(session,
                                     command_offset,
                                     "exec=real",
                                     DebugBudget::kCourseOsShellCommandMaxSteps,
                                     command_chunk) ||
        !run_until_new_uart_contains(session,
                                     command_offset,
                                     "trace=write/exit_group",
                                     DebugBudget::kCourseOsShellCommandMaxSteps,
                                     command_chunk) ||
        !run_until_new_uart_contains(session,
                                     command_offset,
                                     "hello",
                                     DebugBudget::kCourseOsShellCommandMaxSteps,
                                     command_chunk) ||
        !run_until_new_uart_contains(session,
                                     command_offset,
                                     "course-os> ",
                                     DebugBudget::kCourseOsShellCommandMaxSteps,
                                     command_chunk)) {
        return 1;
    }

    const std::string& output = command_chunk.text;

    if (!expect_contains(output,
                         "exec=real",
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
    return 0;
}
