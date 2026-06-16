#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "../../src/debug/debug_budget.h"
#include "../../src/debug/debug_session.h"

namespace {

constexpr const char* kOscompBasicShellElf =
    "guest/generated/course_os_oscomp_basic_shell.elf";
constexpr uint64_t kDefaultOscompBasicCommandMaxSteps = 1200000000ULL;

uint64_t oscomp_basic_command_max_steps() {
    const char* env = std::getenv("MYCPU_OSCOMP_BASIC_COMMAND_MAX_STEPS");
    if (env == nullptr || env[0] == '\0') {
        return kDefaultOscompBasicCommandMaxSteps;
    }

    char* end = nullptr;
    const unsigned long long value = std::strtoull(env, &end, 10);
    if (end == env || *end != '\0' || value == 0ULL) {
        std::fprintf(stderr,
                     "invalid MYCPU_OSCOMP_BASIC_COMMAND_MAX_STEPS: %s\n",
                     env);
        std::exit(1);
    }
    return static_cast<uint64_t>(value);
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

bool expect_real_exec_or_fail_closed(const std::string& output,
                                     const char* command) {
    const bool has_fail_closed_diagnostic =
        output.find("errno=") != std::string::npos ||
        output.find("last_error=") != std::string::npos ||
        output.find("unsupported") != std::string::npos ||
        output.find("loader reason=") != std::string::npos;

    if (output.find("exec=real") != std::string::npos) {
        if (!expect_contains(output,
                             "trace_count=",
                             "real exec should report trace_count") ||
            !expect_contains(output,
                             "exit=",
                             "real exec should report exit code")) {
            return false;
        }
        if (output.find("exit=0") != std::string::npos ||
            has_fail_closed_diagnostic) {
            return true;
        }
        std::fprintf(stderr,
                     "OSComp basic real exec reported non-zero exit without "
                     "diagnostics command=\"%s\"\n",
                     command);
        std::fprintf(stderr, "output was:\n%s\n", output.c_str());
        return false;
    }

    if (has_fail_closed_diagnostic) {
        std::fprintf(stderr,
                     "OSComp basic command fail-closed command=\"%s\"\n",
                     command);
        return true;
    }

    std::fprintf(stderr,
                 "OSComp basic command should real-exec or fail closed: %s\n",
                 command);
    std::fprintf(stderr, "output was:\n%s\n", output.c_str());
    return false;
}

void print_linux_compat_diagnostics(const std::string& output,
                                    const char* command) {
    std::fprintf(stderr, "OSComp basic diagnostics command=\"%s\"\n", command);
    size_t line_begin = 0;
    bool printed = false;
    while (line_begin <= output.size()) {
        const size_t line_end = output.find('\n', line_begin);
        const size_t count =
            line_end == std::string::npos ? output.size() - line_begin
                                          : line_end - line_begin;
        const std::string line = output.substr(line_begin, count);
        if (line.find("linux-compat: rootfs=") != std::string::npos ||
            line.find("linux-compat: path=") != std::string::npos) {
            std::fprintf(stderr, "%s\n", line.c_str());
            printed = true;
        }
        if (line_end == std::string::npos) {
            break;
        }
        line_begin = line_end + 1U;
    }
    if (!printed) {
        std::fprintf(stderr, "no linux-compat diagnostic lines captured\n");
    }
}

struct ExpectedText {
    const char* needle;
    const char* message;
};

bool run_until_new_uart_contains(DebugSession& session,
                                 size_t offset,
                                 const char* command,
                                 const char* needle,
                                 DebugSession::UartOutputChunk& chunk) {
    const uint64_t max_steps = oscomp_basic_command_max_steps();
    try {
        chunk = session.run_until_new_uart_contains(offset, needle, max_steps);
        return true;
    } catch (const std::runtime_error& error) {
        chunk = session.uart_output(offset);
        std::fprintf(stderr, "%s\n", error.what());
        std::fprintf(stderr,
                     "OSComp basic smoke failed while waiting for: %s\n",
                     needle);
        std::fprintf(stderr,
                     "OSComp basic command summary command=\"%s\" "
                     "step_budget=%llu stop=missing-uart output_bytes=%zu "
                     "next_offset=%zu offset=%zu\n",
                     command,
                     (unsigned long long)max_steps,
                     chunk.text.size(),
                     chunk.next_offset,
                     offset);
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
    const uint64_t max_steps = oscomp_basic_command_max_steps();
    std::fprintf(stderr,
                 "OSComp basic command begin command=\"%s\" "
                 "step_budget=%llu offset=%zu\n",
                 command,
                 (unsigned long long)max_steps,
                 offset);
    session.uart_input(command);
    for (size_t i = 0; i < expectation_count; ++i) {
        if (!run_until_new_uart_contains(session,
                                         offset,
                                         command,
                                         expectations[i].needle,
                                         chunk)) {
            return false;
        }
        std::fprintf(stderr,
                     "OSComp basic matched: %s\n",
                     expectations[i].needle);
    }
    for (size_t i = 0; i < expectation_count; ++i) {
        if (!expect_contains(chunk.text,
                             expectations[i].needle,
                             expectations[i].message)) {
            return false;
        }
    }
    print_linux_compat_diagnostics(chunk.text, command);
    offset = chunk.next_offset;
    std::fprintf(stderr,
                 "OSComp basic command summary command=\"%s\" "
                 "step_budget=%llu output_bytes=%zu next_offset=%zu\n",
                 command,
                 (unsigned long long)max_steps,
                 chunk.text.size(),
                 offset);
    return true;
}

void print_input_contract() {
    const char* rootfs = std::getenv("MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS");
    const char* testsuits = std::getenv("MYCPU_OSCOMP_TESTSUITS");

    if (rootfs == nullptr || rootfs[0] == '\0') {
        std::fprintf(stderr,
                     "MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS is required when "
                     "running this binary directly\n");
        std::exit(1);
    }

    const std::filesystem::path rootfs_path(rootfs);
    std::fprintf(stderr,
                 "OSComp basic resolved rootfs=%s\n",
                 std::filesystem::absolute(rootfs_path).lexically_normal()
                     .string()
                     .c_str());

    if (testsuits == nullptr || testsuits[0] == '\0') {
        std::fprintf(stderr,
                     "OSComp basic testsuits=not-set optional_env="
                     "MYCPU_OSCOMP_TESTSUITS\n");
        return;
    }

    const std::filesystem::path testsuits_path(testsuits);
    if (!std::filesystem::exists(testsuits_path)) {
        std::fprintf(stderr,
                     "MYCPU_OSCOMP_TESTSUITS does not exist: %s\n",
                     testsuits);
        std::exit(1);
    }
    std::fprintf(stderr,
                 "OSComp basic resolved testsuits=%s\n",
                 std::filesystem::absolute(testsuits_path)
                     .lexically_normal()
                     .string()
                     .c_str());
}

}  // namespace

int main(int argc, char** argv) {
    const char* backend = argc > 1 ? argv[1] : "functional";
    print_input_contract();

    DebugSession session;
    session.load_elf(kOscompBasicShellElf,
                     parse_backend_kind(backend),
                     BlockTransport::SimpleStorage,
                     "tests/data/storage_basic.txt");
    session.run_until_uart_contains("course-os> ",
                                    DebugBudget::kCourseOsShellBootMaxSteps);

    size_t offset = session.uart_output(0).next_offset;
    DebugSession::UartOutputChunk chunk{};

    constexpr ExpectedText kBusyboxHelp[] = {
        {"linux-compat: rootfs=external", "busybox should use external rootfs"},
        {"linux-compat: path=/bin/busybox", "busybox should resolve /bin/busybox"},
        {"course-os> ", "busybox help should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "linux /bin/busybox --help\r",
                           kBusyboxHelp,
                           sizeof(kBusyboxHelp) / sizeof(kBusyboxHelp[0]),
                           chunk) ||
        !expect_real_exec_or_fail_closed(chunk.text,
                                         "linux /bin/busybox --help")) {
        return 1;
    }

    if (chunk.text.find("exec=real") != std::string::npos &&
        !expect_any_contains(chunk.text,
                             "BusyBox",
                             "Usage:",
                             "busybox help should print help text on success")) {
        return 1;
    }

    constexpr ExpectedText kBusyboxEcho[] = {
        {"linux-compat: path=/bin/busybox", "busybox echo should resolve /bin/busybox"},
        {"course-os> ", "busybox echo should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "linux /bin/busybox echo oscomp-basic\r",
                           kBusyboxEcho,
                           sizeof(kBusyboxEcho) / sizeof(kBusyboxEcho[0]),
                           chunk) ||
        !expect_real_exec_or_fail_closed(chunk.text,
                                         "linux /bin/busybox echo oscomp-basic")) {
        return 1;
    }

    if (chunk.text.find("exec=real") != std::string::npos &&
        !expect_contains(chunk.text,
                         "oscomp-basic",
                         "busybox echo should print oscomp-basic on success")) {
        return 1;
    }

    constexpr ExpectedText kGitHelp[] = {
        {"linux-compat: rootfs=external", "git help should use external rootfs"},
        {"linux-compat: path=/usr/bin/git", "git help should resolve /usr/bin/git"},
        {"course-os> ", "git help should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "git -h\r",
                           kGitHelp,
                           sizeof(kGitHelp) / sizeof(kGitHelp[0]),
                           chunk) ||
        !expect_real_exec_or_fail_closed(chunk.text, "git -h")) {
        return 1;
    }

    if (chunk.text.find("exec=real") != std::string::npos &&
        !expect_contains(chunk.text,
                         "usage: git",
                         "git -h should print usage on success")) {
        return 1;
    }

    constexpr ExpectedText kMissingPath[] = {
        {"linux-compat: rootfs=external", "missing path should use external rootfs"},
        {"linux-compat: path=/oscomp-basic-missing", "missing path should echo resolved path"},
        {"errno=2", "missing path should report ENOENT"},
        {"message=path: no such file", "missing path should explain the missing file"},
        {"course-os> ", "missing path should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "linux /oscomp-basic-missing\r",
                           kMissingPath,
                           sizeof(kMissingPath) / sizeof(kMissingPath[0]),
                           chunk)) {
        return 1;
    }

    return 0;
}
