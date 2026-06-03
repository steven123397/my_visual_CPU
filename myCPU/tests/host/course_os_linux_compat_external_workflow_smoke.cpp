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
                     "Stage 11 workflow smoke failed while waiting for: %s\n",
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
    session.load_elf("guest/generated/course_os_linux_compat_external_shell.elf",
                     parse_backend_kind(backend),
                     BlockTransport::SimpleStorage,
                     "tests/data/storage_basic.txt");
    session.run_until_uart_contains("course-os> ",
                                    DebugBudget::kCourseOsShellBootMaxSteps);

    size_t offset = session.uart_output(0).next_offset;

    constexpr ExpectedText kGitInit[] = {
        {"linux-compat: rootfs=external", "git init should use external rootfs"},
        {"linux-compat: path=/usr/bin/git", "git init should resolve through PATH"},
        {"Initialized", "git init should create a repository"},
        {"course-os> ", "git init should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "git init stage11repo\r",
                           kGitInit,
                           sizeof(kGitInit) / sizeof(kGitInit[0]))) {
        return 1;
    }

    constexpr ExpectedText kVim[] = {
        {"linux-compat: path=/usr/bin/vim", "vim should resolve through PATH"},
    };
    if (!run_shell_command(session,
                           offset,
                           "vim stage11repo/hello.c\r",
                           kVim,
                           sizeof(kVim) / sizeof(kVim[0]))) {
        return 1;
    }

    constexpr ExpectedText kVimSaved[] = {
        {"course-os> ", "vim save should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "i#include <stdio.h>\\nint main(){puts(\"stage11 hello\");return 0;}\\x1b:wq\r",
                           kVimSaved,
                           sizeof(kVimSaved) / sizeof(kVimSaved[0]))) {
        return 1;
    }

    constexpr ExpectedText kGitAdd[] = {
        {"linux-compat: path=/usr/bin/git", "git add should resolve through PATH"},
        {"course-os> ", "git add should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "git -C stage11repo add hello.c\r",
                           kGitAdd,
                           sizeof(kGitAdd) / sizeof(kGitAdd[0]))) {
        return 1;
    }

    constexpr ExpectedText kGitCommit[] = {
        {"linux-compat: path=/usr/bin/git", "git commit should resolve through PATH"},
        {"init", "git commit should report the fixed commit subject"},
        {"course-os> ", "git commit should return to prompt"},
    };
    if (!run_shell_command(
            session,
            offset,
            "GIT_AUTHOR_NAME=stage11 GIT_AUTHOR_EMAIL=stage11@example.invalid "
            "GIT_COMMITTER_NAME=stage11 GIT_COMMITTER_EMAIL=stage11@example.invalid "
            "git -C stage11repo commit -m init\r",
            kGitCommit,
            sizeof(kGitCommit) / sizeof(kGitCommit[0]))) {
        return 1;
    }

    constexpr ExpectedText kGitLog[] = {
        {"linux-compat: path=/usr/bin/git", "git log should resolve through PATH"},
        {"init", "git log should show the committed subject"},
        {"course-os> ", "git log should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "git -C stage11repo log --oneline\r",
                           kGitLog,
                           sizeof(kGitLog) / sizeof(kGitLog[0]))) {
        return 1;
    }

    constexpr ExpectedText kGcc[] = {
        {"linux-compat: path=/usr/bin/gcc", "gcc should resolve through PATH"},
        {"stage11 hello", "gcc hello.c && ./a.out should execute generated binary"},
        {"exec=real", "./a.out should run through Linux compat real exec"},
        {"course-os> ", "gcc workflow should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "cd stage11repo && gcc hello.c && ./a.out\r",
                           kGcc,
                           sizeof(kGcc) / sizeof(kGcc[0]))) {
        return 1;
    }

    return 0;
}
