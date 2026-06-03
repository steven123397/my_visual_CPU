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

struct ExpectedText {
    const char* needle;
    const char* message;
};

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
                     "linux compat terminal smoke failed while waiting for command UART text: %s\n",
                     needle);
        std::fprintf(stderr,
                     "command output since offset %zu was:\n%s\n",
                     offset,
                     chunk.text.c_str());
        return false;
    }
}

bool expect_chunk_contains(const DebugSession::UartOutputChunk& chunk,
                           const ExpectedText* expectations,
                           size_t expectation_count) {
    for (size_t i = 0; i < expectation_count; ++i) {
        if (!expect_contains(chunk.text,
                             expectations[i].needle,
                             expectations[i].message)) {
            return false;
        }
    }
    return true;
}

bool run_shell_command(DebugSession& session,
                       size_t& offset,
                       const char* command,
                       const ExpectedText* expectations,
                       size_t expectation_count) {
    DebugSession::UartOutputChunk chunk{};

    session.uart_input(command);
    for (size_t i = 0; i < expectation_count; ++i) {
        if (!run_until_new_uart_contains(session,
                                         offset,
                                         expectations[i].needle,
                                         DebugBudget::kCourseOsShellCommandMaxSteps,
                                         chunk)) {
            return false;
        }
    }
    if (!expect_chunk_contains(chunk, expectations, expectation_count)) {
        return false;
    }
    offset = chunk.next_offset;
    return true;
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

    size_t offset = boot_chunk.next_offset;

    constexpr ExpectedText kBusyboxHelpExpectations[] = {
        {"linux-compat: rootfs=builtin", "builtin rootfs source should be visible"},
        {"linux-compat: path=/bin/busybox", "busybox should use explicit linux compat launcher"},
        {"loader=static interp=none", "busybox should report static loader plan"},
        {"segments=1 stack=2/0/6", "busybox should report segment and stack summary"},
        {"exec=real", "busybox should run through the real Linux compat exec path"},
        {"trace=write", "busybox help should be produced by real write ecalls"},
        {"exit_group", "busybox help should exit through exit_group"},
        {"BusyBox v", "busybox help should complete through real Linux compat execution"},
        {"Usage: busybox", "busybox help should include a usage line"},
        {"course-os> ", "busybox help should return to the course OS prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "linux /bin/busybox --help\r",
                           kBusyboxHelpExpectations,
                           sizeof(kBusyboxHelpExpectations) /
                               sizeof(kBusyboxHelpExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kBusyboxEchoExpectations[] = {
        {"linux-compat: path=/bin/busybox", "busybox echo should use explicit linux compat launcher"},
        {"exec=real", "busybox echo should run through the real Linux compat exec path"},
        {"trace=write", "busybox echo should use real write ecalls"},
        {"exit_group", "busybox echo should exit through exit_group"},
        {"hello", "busybox echo should print hello from the user ELF payload"},
        {"course-os> ", "busybox echo should return to the course OS prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "linux /bin/busybox echo hello\r",
                           kBusyboxEchoExpectations,
                           sizeof(kBusyboxEchoExpectations) /
                               sizeof(kBusyboxEchoExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kGitExpectations[] = {
        {"linux-compat: rootfs=builtin", "git rootfs source should be visible"},
        {"linux-compat: path=/usr/bin/git", "git path should use explicit linux compat launcher"},
        {"exec=real", "git -h should run through the real Linux compat exec path"},
        {"trace=write", "git -h should use real write ecalls"},
        {"exit_group", "git -h should exit through exit_group"},
        {"usage: git", "git -h should emit a help usage line"},
        {"course-os> ", "git -h should return to the course OS prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "linux /usr/bin/git -h\r",
                           kGitExpectations,
                           sizeof(kGitExpectations) /
                               sizeof(kGitExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kBadPathExpectations[] = {
        {"path=/nope errno=2", "bad linux compat path should report errno 2"},
        {"course-os> ", "bad linux compat path should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "linux /nope\r",
                           kBadPathExpectations,
                           sizeof(kBadPathExpectations) /
                               sizeof(kBadPathExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kHelpExpectations[] = {
        {"meminfo schedstat fsstat", "course help should remain available after linux compat"},
        {"course-os> ", "course help should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "help\r",
                           kHelpExpectations,
                           sizeof(kHelpExpectations) /
                               sizeof(kHelpExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kDirectGitExpectations[] = {
        {"git -h", "direct git fallback command should be visible"},
        {"error\r\ncourse-os> ", "direct git fallback should remain disabled in v0"},
    };
    if (!run_shell_command(session,
                           offset,
                           "git -h\r",
                           kDirectGitExpectations,
                           sizeof(kDirectGitExpectations) /
                               sizeof(kDirectGitExpectations[0]))) {
        return 1;
    }

    return 0;
}
