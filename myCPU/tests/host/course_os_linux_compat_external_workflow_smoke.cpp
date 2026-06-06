#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "../../src/debug/debug_budget.h"
#include "../../src/debug/debug_session.h"

namespace {

constexpr uint64_t kDefaultStage11WorkflowCommandMaxSteps = 1200000000ULL;

uint64_t stage11_workflow_command_max_steps() {
    const char* env = std::getenv("MYCPU_STAGE11_WORKFLOW_COMMAND_MAX_STEPS");
    if (env == nullptr || env[0] == '\0') {
        return kDefaultStage11WorkflowCommandMaxSteps;
    }

    char* end = nullptr;
    const unsigned long long value = std::strtoull(env, &end, 10);
    if (end == env || *end != '\0' || value == 0ULL) {
        std::fprintf(stderr,
                     "invalid MYCPU_STAGE11_WORKFLOW_COMMAND_MAX_STEPS: %s\n",
                     env);
        std::exit(1);
    }
    return static_cast<uint64_t>(value);
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
                                 const char* command,
                                 const char* needle,
                                 DebugSession::UartOutputChunk& chunk) {
    const uint64_t max_steps = stage11_workflow_command_max_steps();
    try {
        chunk = session.run_until_new_uart_contains(
            offset, needle, max_steps);
        return true;
    } catch (const std::runtime_error& error) {
        chunk = session.uart_output(offset);
        std::fprintf(stderr, "%s\n", error.what());
        std::fprintf(stderr,
                     "Stage 11 workflow smoke failed while waiting for: %s\n",
                     needle);
        std::fprintf(stderr,
                     "Stage 11 workflow command summary command=\"%s\" "
                     "step_budget=%llu stop=missing-uart needle=\"%s\" "
                     "offset=%zu\n",
                     command,
                     (unsigned long long)max_steps,
                     needle,
                     offset);
        std::fprintf(stderr,
                     "command output since offset %zu was:\n%s\n",
                     offset,
                     chunk.text.c_str());
        const DebugSnapshot snapshot = session.snapshot();
        std::fprintf(stderr,
                     "snapshot pc=0x%llx privilege=%u cycle=%llu instret=%llu "
                     "halted=%d mcause=0x%llx mtval=0x%llx mepc=0x%llx "
                     "mstatus=0x%llx scause=0x%llx stval=0x%llx sepc=0x%llx "
                     "sstatus=0x%llx satp=0x%llx "
                     "recent_uart=%s\n",
                     (unsigned long long)snapshot.summary.pc,
                     (unsigned)snapshot.summary.privilege,
                     (unsigned long long)snapshot.summary.cycle,
                     (unsigned long long)snapshot.summary.instret,
                     snapshot.summary.halted ? 1 : 0,
                     (unsigned long long)snapshot.csrs.mcause,
                     (unsigned long long)snapshot.csrs.mtval,
                     (unsigned long long)snapshot.csrs.mepc,
                     (unsigned long long)snapshot.csrs.mstatus,
                     (unsigned long long)snapshot.csrs.scause,
                     (unsigned long long)snapshot.csrs.stval,
                     (unsigned long long)snapshot.csrs.sepc,
                     (unsigned long long)snapshot.csrs.sstatus,
                     (unsigned long long)snapshot.csrs.satp,
                     snapshot.devices.uart.recent_output.c_str());
        std::fprintf(stderr,
                     "snapshot regs ra=0x%llx sp=0x%llx a0=0x%llx a1=0x%llx "
                     "a2=0x%llx a3=0x%llx a4=0x%llx a5=0x%llx a6=0x%llx "
                     "a7=0x%llx\n",
                     (unsigned long long)snapshot.gpr[1],
                     (unsigned long long)snapshot.gpr[2],
                     (unsigned long long)snapshot.gpr[10],
                     (unsigned long long)snapshot.gpr[11],
                     (unsigned long long)snapshot.gpr[12],
                     (unsigned long long)snapshot.gpr[13],
                     (unsigned long long)snapshot.gpr[14],
                     (unsigned long long)snapshot.gpr[15],
                     (unsigned long long)snapshot.gpr[16],
                     (unsigned long long)snapshot.gpr[17]);
        return false;
    }
}

size_t drain_uart_until_quiet(DebugSession& session) {
    constexpr uint64_t kDrainMaxSteps = 50000ULL;
    constexpr uint64_t kStableSteps = 2048ULL;
    size_t last_size = session.uart_output(0).next_offset;
    uint64_t stable_steps = 0;

    for (uint64_t i = 0; i < kDrainMaxSteps; ++i) {
        session.step_cycle();
        const size_t current_size = session.uart_output(0).next_offset;
        if (current_size == last_size) {
            stable_steps += 1U;
            if (stable_steps >= kStableSteps) {
                break;
            }
        } else {
            last_size = current_size;
            stable_steps = 0;
        }
    }
    return session.uart_output(0).next_offset;
}

bool run_shell_command(DebugSession& session,
                       size_t& offset,
                       const char* command,
                       const ExpectedText* expectations,
                       size_t expectation_count) {
    DebugSession::UartOutputChunk chunk{};
    const uint64_t max_steps = stage11_workflow_command_max_steps();
    std::fprintf(stderr,
                 "Stage 11 workflow command begin command=\"%s\" "
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
                     "Stage 11 workflow matched: %s\n",
                     expectations[i].needle);
    }
    for (size_t i = 0; i < expectation_count; ++i) {
        if (!expect_contains(chunk.text,
                             expectations[i].needle,
                             expectations[i].message)) {
            return false;
        }
    }
    offset = drain_uart_until_quiet(session);
    std::fprintf(stderr,
                 "Stage 11 workflow command summary command=\"%s\" "
                 "output_bytes=%zu next_offset=%zu stop=matched-prompt\n",
                 command,
                 chunk.text.size(),
                 offset);
    return true;
}

bool run_shell_command_waiting_for(DebugSession& session,
                                   size_t& offset,
                                   const char* command,
                                   const ExpectedText* expectations,
                                   size_t expectation_count,
                                   const char* stop_needle) {
    DebugSession::UartOutputChunk chunk{};
    const uint64_t max_steps = stage11_workflow_command_max_steps();
    std::fprintf(stderr,
                 "Stage 11 workflow command begin command=\"%s\" "
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
                     "Stage 11 workflow matched: %s\n",
                     expectations[i].needle);
    }
    if (!run_until_new_uart_contains(session,
                                     offset,
                                     command,
                                     stop_needle,
                                     chunk)) {
        return false;
    }
    std::fprintf(stderr, "Stage 11 workflow matched: %s\n", stop_needle);
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

    if (!run_shell_command_waiting_for(session,
                                       offset,
                                       "vim stage11repo/hello.c\r",
                                       nullptr,
                                       0,
                                       "Press ENTER or type command to continue")) {
        return 1;
    }
    constexpr ExpectedText kVimSave[] = {
        {"linux-compat: path=/usr/bin/vim", "vim should resolve through PATH"},
        {"course-os> ", "vim save should return to prompt"},
    };
    if (!run_shell_command(session,
                           offset,
                           "\ri#include <stdio.h>\nint main(){puts(\"stage11 hello\");return 0;}\x1b:wq\r",
                           kVimSave,
                           sizeof(kVimSave) / sizeof(kVimSave[0]))) {
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
        {"(root-commit)", "git commit should create the first commit"},
        {"course-os> ", "git commit should return to prompt"},
    };
    if (!run_shell_command(
            session,
            offset,
            "git -C stage11repo -c safe.directory=/stage11repo "
            "-c user.name=stage11 "
            "-c user.email=stage11@example.invalid commit -m init\r",
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
                           "git -C stage11repo "
                           "-c safe.directory=/stage11repo log --oneline\r",
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
