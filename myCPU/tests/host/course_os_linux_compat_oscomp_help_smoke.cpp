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
                                 DebugSession::UartOutputChunk& chunk) {
    try {
        chunk = session.run_until_new_uart_contains(
            offset, needle, DebugBudget::kCourseOsShellCommandMaxSteps);
        return true;
    } catch (const std::runtime_error& error) {
        chunk = session.uart_output(offset);
        std::fprintf(stderr, "%s\n", error.what());
        std::fprintf(stderr,
                     "OSComp help smoke failed while waiting for: %s\n",
                     needle);
        std::fprintf(stderr,
                     "command output since offset %zu was:\n%s\n",
                     offset,
                     chunk.text.c_str());
        return false;
    }
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
                                         chunk)) {
            return false;
        }
    }
    for (size_t i = 0; i < expectation_count; ++i) {
        if (!expect_contains(chunk.text,
                             expectations[i].needle,
                             expectations[i].message)) {
            return false;
        }
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
    session.run_until_uart_contains("course-os> ",
                                    DebugBudget::kCourseOsShellBootMaxSteps);

    size_t offset = session.uart_output(0).next_offset;

    constexpr ExpectedText kGitDashH[] = {
        {"linux-compat: path=/usr/bin/git", "git -h should resolve through PATH fallback"},
        {"exec=real", "git -h should use real exec"},
        {"trace=write", "git -h should write from Linux compat user code"},
        {"usage: git", "git -h should emit usage"},
        {"course-os> ", "git -h should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "git -h\r",
                           kGitDashH,
                           sizeof(kGitDashH) / sizeof(kGitDashH[0]))) {
        return 1;
    }

    constexpr ExpectedText kGitHelp[] = {
        {"linux-compat: path=/usr/bin/git", "git help should resolve through PATH fallback"},
        {"exec=real", "git help should use real exec"},
        {"usage: git", "git help should emit usage"},
        {"course-os> ", "git help should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "git help\r",
                           kGitHelp,
                           sizeof(kGitHelp) / sizeof(kGitHelp[0]))) {
        return 1;
    }

    constexpr ExpectedText kMissingTool[] = {
        {"linux-compat: path=", "missing Stage 10 tool should use Linux compat diagnostics"},
        {"errno=2", "missing Stage 10 tool should fail closed with ENOENT"},
        {"path: no such file", "missing Stage 10 tool should report PATH miss"},
        {"course-os> ", "missing Stage 10 tool should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "vim -h\r",
                           kMissingTool,
                           sizeof(kMissingTool) / sizeof(kMissingTool[0])) ||
        !run_shell_command(session,
                           offset,
                           "gcc --h\r",
                           kMissingTool,
                           sizeof(kMissingTool) / sizeof(kMissingTool[0])) ||
        !run_shell_command(session,
                           offset,
                           "rustc -h\r",
                           kMissingTool,
                           sizeof(kMissingTool) / sizeof(kMissingTool[0]))) {
        return 1;
    }

    return 0;
}
