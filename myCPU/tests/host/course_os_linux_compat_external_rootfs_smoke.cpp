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

bool expect_any_contains(const std::string& haystack,
                         const char* first,
                         const char* second,
                         const char* message) {
    if (haystack.find(first) != std::string::npos ||
        haystack.find(second) != std::string::npos) {
        return true;
    }
    std::fprintf(stderr, "%s\n", message);
    std::fprintf(stderr, "output was:\n%s\n", haystack.c_str());
    return false;
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
                     "external rootfs smoke failed while waiting for: %s\n",
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
                       size_t expectation_count,
                       DebugSession::UartOutputChunk& chunk) {
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
    session.load_elf("guest/generated/course_os_linux_compat_external_shell.elf",
                     parse_backend_kind(backend),
                     BlockTransport::SimpleStorage,
                     "tests/data/storage_basic.txt");
    session.run_until_uart_contains("course-os> ",
                                    DebugBudget::kCourseOsShellBootMaxSteps);

    size_t offset = session.uart_output(0).next_offset;
    DebugSession::UartOutputChunk chunk{};

    constexpr ExpectedText kBusybox[] = {
        {"linux-compat: rootfs=external", "external rootfs should report generated provider"},
        {"linux-compat: path=/bin/busybox", "busybox should launch through explicit linux command"},
        {"loader=static interp=none", "busybox should report static loader plan"},
        {"BusyBox v", "busybox help should emit help"},
        {"course-os> ", "busybox help should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "linux /bin/busybox --help\r",
                           kBusybox,
                           sizeof(kBusybox) / sizeof(kBusybox[0]),
                           chunk)) {
        return 1;
    }

    constexpr ExpectedText kExplicitGit[] = {
        {"linux-compat: path=/usr/bin/git", "explicit git should use external /usr/bin/git"},
        {"usage: git", "explicit git help should emit usage"},
        {"course-os> ", "explicit git help should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "linux /usr/bin/git -h\r",
                           kExplicitGit,
                           sizeof(kExplicitGit) / sizeof(kExplicitGit[0]),
                           chunk)) {
        return 1;
    }

    constexpr ExpectedText kDirectGit[] = {
        {"linux-compat: rootfs=external", "direct git fallback should enter Linux compat"},
        {"linux-compat: path=/usr/bin/git", "direct git fallback should resolve through PATH"},
        {"course-os> ", "direct git fallback should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "git -h\r",
                           kDirectGit,
                           sizeof(kDirectGit) / sizeof(kDirectGit[0]),
                           chunk)) {
        return 1;
    }
    if (!expect_any_contains(
            chunk.text,
            "usage: git",
            "errno=",
            "direct git fallback should real-exec or emit Linux compat fail-closed diagnostics")) {
        return 1;
    }

    return 0;
}
