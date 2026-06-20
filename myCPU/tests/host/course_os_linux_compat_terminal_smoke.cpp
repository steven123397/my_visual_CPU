// Linux compat 终端 smoke：通过 Course OS shell 驱动 linux 命令和 PATH fallback。
#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/debug/debug_budget.h"
#include "terminal_smoke_harness.h"

namespace {

bool run_course_os_command(DebugSession& session,
                           size_t& offset,
                           const char* command,
                           const terminal_smoke::ExpectedText* expectations,
                           size_t expectation_count) {
    return terminal_smoke::run_shell_command(session,
                                             offset,
                                             command,
                                             expectations,
                                             expectation_count,
                                             DebugBudget::kCourseOsShellCommandMaxSteps,
                                             "linux compat terminal smoke");
}

}  // namespace

int main(int argc, char** argv) {
    const char* backend = argc > 1 ? argv[1] : "functional";
    DebugSession session;

    const DebugSession::UartOutputChunk boot_chunk =
        terminal_smoke::load_and_wait_for_prompt(session,
                                                 "guest/course_os_shell.elf",
                                                 terminal_smoke::parse_backend_kind(backend),
                                                 "course-os> ",
                                                 DebugBudget::kCourseOsShellBootMaxSteps);
    if (!terminal_smoke::expect_contains(boot_chunk.text,
                                         "course-os shell ready",
                                         "course OS shell should emit a ready banner") ||
        !terminal_smoke::expect_contains(boot_chunk.text,
                                         "course-os> ",
                                         "course OS shell prompt should settle")) {
        return 1;
    }

    size_t offset = boot_chunk.next_offset;

    constexpr terminal_smoke::ExpectedText kBusyboxHelpExpectations[] = {
        {"linux-compat: rootfs=builtin", "builtin rootfs source should be visible"},
        {"linux-compat: path=/bin/busybox", "busybox should use explicit linux compat launcher"},
        {"loader=static interp=none", "busybox should report static loader plan"},
        {"segments=1 stack=2/0/12", "busybox should report segment and stack summary"},
        {"exec=real", "busybox should run through the real Linux compat exec path"},
        {"trace=write", "busybox help should be produced by real write ecalls"},
        {"exit_group", "busybox help should exit through exit_group"},
        {"BusyBox v", "busybox help should complete through real Linux compat execution"},
        {"Usage: busybox", "busybox help should include a usage line"},
        {"course-os> ", "busybox help should return to the course OS prompt"},
    };
    if (!run_course_os_command(session,
                               offset,
                               "linux /bin/busybox --help\r",
                               kBusyboxHelpExpectations,
                               sizeof(kBusyboxHelpExpectations) /
                                   sizeof(kBusyboxHelpExpectations[0]))) {
        return 1;
    }

    constexpr terminal_smoke::ExpectedText kBusyboxEchoExpectations[] = {
        {"linux-compat: path=/bin/busybox", "busybox echo should use explicit linux compat launcher"},
        {"exec=real", "busybox echo should run through the real Linux compat exec path"},
        {"trace=write", "busybox echo should use real write ecalls"},
        {"exit_group", "busybox echo should exit through exit_group"},
        {"hello", "busybox echo should print hello from the user ELF payload"},
        {"course-os> ", "busybox echo should return to the course OS prompt"},
    };
    if (!run_course_os_command(session,
                               offset,
                               "linux /bin/busybox echo hello\r",
                               kBusyboxEchoExpectations,
                               sizeof(kBusyboxEchoExpectations) /
                                   sizeof(kBusyboxEchoExpectations[0]))) {
        return 1;
    }

    constexpr terminal_smoke::ExpectedText kGitExpectations[] = {
        {"linux-compat: rootfs=builtin", "git rootfs source should be visible"},
        {"linux-compat: path=/usr/bin/git", "git path should use explicit linux compat launcher"},
        {"exec=real", "git -h should run through the real Linux compat exec path"},
        {"trace=write", "git -h should use real write ecalls"},
        {"exit_group", "git -h should exit through exit_group"},
        {"usage: git", "git -h should emit a help usage line"},
        {"course-os> ", "git -h should return to the course OS prompt"},
    };
    if (!run_course_os_command(session,
                               offset,
                               "linux /usr/bin/git -h\r",
                               kGitExpectations,
                               sizeof(kGitExpectations) /
                                   sizeof(kGitExpectations[0]))) {
        return 1;
    }

    constexpr terminal_smoke::ExpectedText kBadPathExpectations[] = {
        {"path=/nope errno=2", "bad linux compat path should report errno 2"},
        {"course-os> ", "bad linux compat path should return to prompt"},
    };
    if (!run_course_os_command(session,
                               offset,
                               "linux /nope\r",
                               kBadPathExpectations,
                               sizeof(kBadPathExpectations) /
                                   sizeof(kBadPathExpectations[0]))) {
        return 1;
    }

    constexpr terminal_smoke::ExpectedText kHelpExpectations[] = {
        {"meminfo schedstat fsstat", "course help should remain available after linux compat"},
        {"course-os> ", "course help should return to prompt"},
    };
    if (!run_course_os_command(session,
                               offset,
                               "help\r",
                               kHelpExpectations,
                               sizeof(kHelpExpectations) /
                                   sizeof(kHelpExpectations[0]))) {
        return 1;
    }

    constexpr terminal_smoke::ExpectedText kDirectGitExpectations[] = {
        {"linux-compat: rootfs=builtin", "direct git fallback should report builtin rootfs"},
        {"linux-compat: path=/usr/bin/git", "direct git fallback should resolve through Linux compat PATH"},
        {"exec=real", "direct git fallback should run through the real Linux compat exec path"},
        {"trace=write", "direct git fallback should use real write ecalls"},
        {"exit_group", "direct git fallback should exit through exit_group"},
        {"usage: git", "direct git -h should emit a help usage line"},
        {"course-os> ", "direct git fallback should return to prompt"},
    };
    if (!run_course_os_command(session,
                               offset,
                               "git -h\r",
                               kDirectGitExpectations,
                               sizeof(kDirectGitExpectations) /
                                   sizeof(kDirectGitExpectations[0]))) {
        return 1;
    }

    constexpr terminal_smoke::ExpectedText kDirectGitHelpExpectations[] = {
        {"linux-compat: path=/usr/bin/git", "direct git help should resolve through Linux compat PATH"},
        {"exec=real", "direct git help should run through the real Linux compat exec path"},
        {"trace=write", "direct git help should use real write ecalls"},
        {"usage: git", "direct git help should emit a usage line"},
        {"course-os> ", "direct git help should return to prompt"},
    };
    if (!run_course_os_command(session,
                               offset,
                               "git help\r",
                               kDirectGitHelpExpectations,
                               sizeof(kDirectGitHelpExpectations) /
                                   sizeof(kDirectGitHelpExpectations[0]))) {
        return 1;
    }

    constexpr terminal_smoke::ExpectedText kBuiltinGitInitExpectations[] = {
        {"linux-compat: rootfs=builtin", "default Course OS shell should keep git init on the builtin rootfs"},
        {"linux-compat: path=/usr/bin/git", "default git init should resolve through builtin Linux compat PATH"},
        {"usage: git", "default git init should remain a builtin help-run path"},
        {"course-os> ", "default git init should return to prompt"},
    };
    const size_t git_init_offset = offset;
    if (!run_course_os_command(session,
                               offset,
                               "git init\r",
                               kBuiltinGitInitExpectations,
                               sizeof(kBuiltinGitInitExpectations) /
                                   sizeof(kBuiltinGitInitExpectations[0]))) {
        return 1;
    }
    const DebugSession::UartOutputChunk after_git_init = session.uart_output(git_init_offset);
    if (after_git_init.text.find("Initialized") != std::string::npos) {
        std::fprintf(stderr,
                     "default Course OS shell git init should not initialize a writable repository\n");
        std::fprintf(stderr,
                     "command output since offset %zu was:\n%s\n",
                     git_init_offset,
                     after_git_init.text.c_str());
        return 1;
    }

    return 0;
}
