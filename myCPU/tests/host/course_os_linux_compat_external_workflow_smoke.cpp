#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "../../src/debug/debug_budget.h"
#include "../../src/debug/debug_session.h"
#include "course_os_linux_compat_trace_debug.h"

namespace {

constexpr uint64_t kDefaultStage11WorkflowCommandMaxSteps = 1200000000ULL;
constexpr const char* kExternalShellElf =
    "guest/generated/course_os_linux_compat_external_shell.elf";

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

uint64_t stage11_workflow_trace_sample_steps() {
    const char* env = std::getenv("MYCPU_STAGE11_WORKFLOW_TRACE_SAMPLE_STEPS");
    if (env == nullptr || env[0] == '\0') {
        return 0;
    }

    char* end = nullptr;
    const unsigned long long value = std::strtoull(env, &end, 10);
    if (end == env || *end != '\0') {
        std::fprintf(stderr,
                     "invalid MYCPU_STAGE11_WORKFLOW_TRACE_SAMPLE_STEPS: %s\n",
                     env);
        std::exit(1);
    }
    return static_cast<uint64_t>(value);
}

bool stage11_workflow_fast_commit_probe() {
    const char* env = std::getenv("MYCPU_STAGE11_WORKFLOW_FAST_COMMIT_PROBE");
    return env != nullptr && std::strcmp(env, "1") == 0;
}

bool stage11_workflow_direct_gcc_probe() {
    const char* env = std::getenv("MYCPU_STAGE11_WORKFLOW_DIRECT_GCC_PROBE");
    return env != nullptr && std::strcmp(env, "1") == 0;
}

const char* stage11_workflow_direct_gcc_command() {
    const char* env = std::getenv("MYCPU_STAGE11_WORKFLOW_DIRECT_GCC_COMMAND");
    return env != nullptr && env[0] != '\0' ? env : "gcc --h\r";
}

const char* stage11_workflow_final_command() {
    const char* env = std::getenv("MYCPU_STAGE11_WORKFLOW_FINAL_COMMAND");
    return env != nullptr && env[0] != '\0'
               ? env
               : "cd stage11repo && gcc hello.c && ./a.out\r";
}

bool stage11_workflow_final_probe() {
    const char* env = std::getenv("MYCPU_STAGE11_WORKFLOW_FINAL_PROBE");
    return env != nullptr && std::strcmp(env, "1") == 0;
}

bool stage11_workflow_print_command_output() {
    const char* env =
        std::getenv("MYCPU_STAGE11_WORKFLOW_PRINT_COMMAND_OUTPUT");
    return env != nullptr && std::strcmp(env, "1") == 0;
}

bool command_runs_aout(const char* command) {
    return command != nullptr && std::strstr(command, "./a.out") != nullptr;
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

enum class Stage11WorkflowStopMode {
    ExpectationSequence,
    StopNeedle,
    PromptThenValidate,
    ProbeStop,
};

constexpr uint32_t kStage11ProbeNone = 0U;
constexpr uint32_t kStage11ProbeFastCommit = 1U << 0U;
constexpr uint32_t kStage11ProbeDirectGcc = 1U << 1U;
constexpr uint32_t kStage11ProbeFinalCommand = 1U << 2U;

struct Stage11WorkflowCommand {
    const char* command;
    const char* description;
    Stage11WorkflowStopMode stop_mode;
    uint32_t probe_flags;
    const ExpectedText* expectations;
    size_t expectation_count;
    const char* stop_needle;
    const char* fallback_needle;
    bool expect_linux_summary;
};

const char* stop_mode_name(Stage11WorkflowStopMode mode) {
    switch (mode) {
        case Stage11WorkflowStopMode::ExpectationSequence:
            return "expectation-sequence";
        case Stage11WorkflowStopMode::StopNeedle:
            return "stop-needle";
        case Stage11WorkflowStopMode::PromptThenValidate:
            return "prompt-then-validate";
        case Stage11WorkflowStopMode::ProbeStop:
            return "probe-stop";
    }
    return "unknown";
}

bool expect_linux_run_summary(const std::string& output,
                              const char* command) {
    constexpr ExpectedText kRunSummary[] = {
        {"loader=", "Linux compat run should report loader kind"},
        {"interp=", "Linux compat run should report interpreter path"},
        {"trace_count=", "Linux compat run should report trace count"},
        {"last=", "Linux compat run should report latest syscall"},
        {"/errno=", "Linux compat run should report latest syscall errno"},
    };

    for (size_t i = 0; i < sizeof(kRunSummary) / sizeof(kRunSummary[0]);
         ++i) {
        if (!expect_contains(output,
                             kRunSummary[i].needle,
                             kRunSummary[i].message)) {
            std::fprintf(stderr,
                         "Linux compat summary assertion failed for command: %s\n",
                         command);
            return false;
        }
    }
    return true;
}

void print_guest_linux_trace(DebugSession& session) {
    static course_os_linux_compat_trace_debug::GuestLinuxTraceReader reader(
        kExternalShellElf);
    course_os_linux_compat_trace_debug::print_guest_linux_trace(session,
                                                               reader);
}

void print_debug_snapshot_brief(DebugSession& session, const char* label) {
    const DebugSnapshot snapshot = session.snapshot();
    std::fprintf(stderr,
                 "%s pc=0x%llx privilege=%u cycle=%llu instret=%llu "
                 "scause=0x%llx sepc=0x%llx a0=0x%llx a1=0x%llx "
                 "a2=0x%llx a7=0x%llx recent_uart=%s\n",
                 label,
                 (unsigned long long)snapshot.summary.pc,
                 (unsigned)snapshot.summary.privilege,
                 (unsigned long long)snapshot.summary.cycle,
                 (unsigned long long)snapshot.summary.instret,
                 (unsigned long long)snapshot.csrs.scause,
                 (unsigned long long)snapshot.csrs.sepc,
                 (unsigned long long)snapshot.gpr[10],
                 (unsigned long long)snapshot.gpr[11],
                 (unsigned long long)snapshot.gpr[12],
                 (unsigned long long)snapshot.gpr[17],
                 snapshot.devices.uart.recent_output.c_str());
}

void print_debug_snapshot_full(DebugSession& session) {
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
}

void print_trace_sample(DebugSession& session,
                        const char* command,
                        const char* needle,
                        uint64_t waited_steps,
                        size_t output_bytes) {
    std::fprintf(stderr,
                 "Stage 11 workflow trace sample "
                 "command=\"%s\" waited_steps=%llu "
                 "needle=\"%s\" output_bytes=%zu\n",
                 command,
                 (unsigned long long)waited_steps,
                 needle,
                 output_bytes);
    print_guest_linux_trace(session);
    print_debug_snapshot_brief(session, "sample snapshot");
}

void print_missing_uart_diagnostics(DebugSession& session,
                                    const char* command,
                                    const char* needle,
                                    uint64_t max_steps,
                                    size_t offset,
                                    const DebugSession::UartOutputChunk& chunk,
                                    const std::runtime_error& error) {
    std::fprintf(stderr, "%s\n", error.what());
    std::fprintf(stderr,
                 "Stage 11 workflow smoke failed while waiting for: %s\n",
                 needle);
    std::fprintf(stderr,
                 "Stage 11 workflow command summary command=\"%s\" "
                 "step_budget=%llu stop=missing-uart output_bytes=%zu "
                 "next_offset=%zu needle=\"%s\" offset=%zu\n",
                 command,
                 (unsigned long long)max_steps,
                 chunk.text.size(),
                 chunk.next_offset,
                 needle,
                 offset);
    std::fprintf(stderr,
                 "command output since offset %zu was:\n%s\n",
                 offset,
                 chunk.text.c_str());
    print_guest_linux_trace(session);
    print_debug_snapshot_full(session);
}

bool run_until_new_uart_contains(DebugSession& session,
                                 size_t offset,
                                 const char* command,
                                 const char* needle,
                                 DebugSession::UartOutputChunk& chunk) {
    const uint64_t max_steps = stage11_workflow_command_max_steps();
    const uint64_t sample_steps = stage11_workflow_trace_sample_steps();
    try {
        if (sample_steps == 0U) {
            chunk = session.run_until_new_uart_contains(
                offset, needle, max_steps);
        } else {
            for (uint64_t i = 0; i < max_steps; ++i) {
                session.debug_step_raw();
                chunk = session.uart_output(offset);
                if (chunk.text.find(needle) != std::string::npos) {
                    return true;
                }
                if (session.debug_halted()) {
                    throw std::runtime_error(
                        "guest halted before requested UART text appeared");
                }
                if ((i + 1U) % sample_steps == 0U) {
                    print_trace_sample(session,
                                       command,
                                       needle,
                                       i + 1U,
                                       chunk.text.size());
                }
            }
            throw std::runtime_error(
                "run_until_new_uart_contains exceeded step budget");
        }
        return true;
    } catch (const std::runtime_error& error) {
        chunk = session.uart_output(offset);
        print_missing_uart_diagnostics(session,
                                       command,
                                       needle,
                                       max_steps,
                                       offset,
                                       chunk,
                                       error);
        return false;
    }
}

size_t drain_uart_until_quiet(DebugSession& session) {
    constexpr uint64_t kDrainMaxSteps = 8192ULL;
    constexpr uint64_t kStableSteps = 512ULL;
    size_t last_size = session.uart_output(0).next_offset;
    uint64_t stable_steps = 0;

    for (uint64_t i = 0; i < kDrainMaxSteps; ++i) {
        session.debug_step_raw();
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
                       size_t expectation_count,
                       DebugSession::UartOutputChunk* out_chunk = nullptr) {
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
    if (stage11_workflow_print_command_output()) {
        std::fprintf(stderr,
                     "command output since offset %zu was:\n%s\n",
                     offset,
                     chunk.text.c_str());
        print_guest_linux_trace(session);
    }
    if (out_chunk != nullptr) {
        *out_chunk = chunk;
    }
    offset = drain_uart_until_quiet(session);
    std::fprintf(stderr,
                 "Stage 11 workflow command summary command=\"%s\" "
                 "step_budget=%llu stop=matched-prompt output_bytes=%zu "
                 "next_offset=%zu\n",
                 command,
                 (unsigned long long)max_steps,
                 chunk.text.size(),
                 offset);
    return true;
}

bool run_shell_command_waiting_for(DebugSession& session,
                                   size_t& offset,
                                   const char* command,
                                   const ExpectedText* expectations,
                                   size_t expectation_count,
                                   const char* stop_needle,
                                   DebugSession::UartOutputChunk* out_chunk =
                                       nullptr) {
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
    if (stage11_workflow_print_command_output()) {
        std::fprintf(stderr,
                     "command output since offset %zu was:\n%s\n",
                     offset,
                     chunk.text.c_str());
        print_guest_linux_trace(session);
    }
    if (out_chunk != nullptr) {
        *out_chunk = chunk;
    }
    offset = drain_uart_until_quiet(session);
    std::fprintf(stderr,
                 "Stage 11 workflow command summary command=\"%s\" "
                 "step_budget=%llu stop=matched-stop-needle "
                 "output_bytes=%zu next_offset=%zu\n",
                 command,
                 (unsigned long long)max_steps,
                 chunk.text.size(),
                 offset);
    return true;
}

bool run_shell_command_until_prompt_and_validate(DebugSession& session,
                                                 size_t& offset,
                                                 const char* command,
                                                 const ExpectedText* expectations,
                                                 size_t expectation_count,
                                                 DebugSession::UartOutputChunk*
                                                     out_chunk = nullptr) {
    DebugSession::UartOutputChunk chunk{};
    const uint64_t max_steps = stage11_workflow_command_max_steps();
    std::fprintf(stderr,
                 "Stage 11 workflow command begin command=\"%s\" "
                 "step_budget=%llu offset=%zu\n",
                 command,
                 (unsigned long long)max_steps,
                 offset);
    session.uart_input(command);
    if (!run_until_new_uart_contains(session,
                                     offset,
                                     command,
                                     "course-os> ",
                                     chunk)) {
        return false;
    }
    std::fprintf(stderr, "Stage 11 workflow matched: course-os> \n");
    for (size_t i = 0; i < expectation_count; ++i) {
        if (!expect_contains(chunk.text,
                             expectations[i].needle,
                             expectations[i].message)) {
            return false;
        }
    }
    if (stage11_workflow_print_command_output()) {
        std::fprintf(stderr,
                     "command output since offset %zu was:\n%s\n",
                     offset,
                     chunk.text.c_str());
        print_guest_linux_trace(session);
    }
    if (out_chunk != nullptr) {
        *out_chunk = chunk;
    }
    offset = drain_uart_until_quiet(session);
    std::fprintf(stderr,
                 "Stage 11 workflow command summary command=\"%s\" "
                 "step_budget=%llu stop=matched-prompt output_bytes=%zu "
                 "next_offset=%zu\n",
                 command,
                 (unsigned long long)max_steps,
                 chunk.text.size(),
                 offset);
    return true;
}

bool run_shell_command_probe_stop(DebugSession& session,
                                  size_t offset,
                                  const char* command,
                                  const char* stop_needle,
                                  const char* fallback_needle,
                                  DebugSession::UartOutputChunk* out_chunk =
                                      nullptr) {
    DebugSession::UartOutputChunk chunk{};
    const uint64_t max_steps = stage11_workflow_command_max_steps();
    std::fprintf(stderr,
                 "Stage 11 workflow probe begin command=\"%s\" "
                 "step_budget=%llu offset=%zu stop=\"%s\" fallback=\"%s\"\n",
                 command,
                 (unsigned long long)max_steps,
                 offset,
                 stop_needle,
                 fallback_needle);
    session.uart_input(command);
    for (uint64_t i = 0; i < max_steps; ++i) {
        session.debug_step_raw();
        chunk = session.uart_output(offset);
        if (chunk.text.find(stop_needle) != std::string::npos ||
            chunk.text.find(fallback_needle) != std::string::npos) {
            if (out_chunk != nullptr) {
                *out_chunk = chunk;
            }
            std::fprintf(stderr,
                         "Stage 11 workflow command summary command=\"%s\" "
                         "step_budget=%llu stop=matched-probe "
                         "output_bytes=%zu next_offset=%zu\n",
                         command,
                         (unsigned long long)max_steps,
                         chunk.text.size(),
                         chunk.next_offset);
            std::fprintf(stderr,
                         "probe output since offset %zu was:\n%s\n",
                         offset,
                         chunk.text.c_str());
            print_guest_linux_trace(session);
            print_debug_snapshot_full(session);
            return true;
        }
        if (session.debug_halted()) {
            std::fprintf(stderr, "guest halted during probe\n");
            return false;
        }
    }
    std::fprintf(stderr, "Stage 11 workflow probe exceeded step budget\n");
    chunk = session.uart_output(offset);
    if (out_chunk != nullptr) {
        *out_chunk = chunk;
    }
    std::fprintf(stderr,
                 "Stage 11 workflow command summary command=\"%s\" "
                 "step_budget=%llu stop=probe-step-budget output_bytes=%zu "
                 "next_offset=%zu\n",
                 command,
                 (unsigned long long)max_steps,
                 chunk.text.size(),
                 chunk.next_offset);
    std::fprintf(stderr,
                 "probe output since offset %zu was:\n%s\n",
                 offset,
                 chunk.text.c_str());
    print_guest_linux_trace(session);
    print_debug_snapshot_full(session);
    return false;
}

bool run_stage11_workflow_command(DebugSession& session,
                                  size_t& offset,
                                  const Stage11WorkflowCommand& spec) {
    bool ok = false;
    DebugSession::UartOutputChunk chunk{};

    std::fprintf(stderr,
                 "Stage 11 workflow command plan command=\"%s\" "
                 "description=\"%s\" stop_mode=%s probe_flags=0x%x\n",
                 spec.command,
                 spec.description,
                 stop_mode_name(spec.stop_mode),
                 spec.probe_flags);

    switch (spec.stop_mode) {
        case Stage11WorkflowStopMode::ExpectationSequence:
            ok = run_shell_command(session,
                                   offset,
                                   spec.command,
                                   spec.expectations,
                                   spec.expectation_count,
                                   &chunk);
            break;
        case Stage11WorkflowStopMode::StopNeedle:
            ok = run_shell_command_waiting_for(session,
                                               offset,
                                               spec.command,
                                               spec.expectations,
                                               spec.expectation_count,
                                               spec.stop_needle,
                                               &chunk);
            break;
        case Stage11WorkflowStopMode::PromptThenValidate:
            ok = run_shell_command_until_prompt_and_validate(
                session,
                offset,
                spec.command,
                spec.expectations,
                spec.expectation_count,
                &chunk);
            break;
        case Stage11WorkflowStopMode::ProbeStop:
            ok = run_shell_command_probe_stop(session,
                                              offset,
                                              spec.command,
                                              spec.stop_needle,
                                              spec.fallback_needle,
                                              &chunk);
            break;
    }

    if (!ok) {
        return false;
    }
    if (spec.expect_linux_summary &&
        !expect_linux_run_summary(chunk.text, spec.command)) {
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const char* backend = argc > 1 ? argv[1] : "functional";
    DebugSession session;
    session.load_elf(kExternalShellElf,
                     parse_backend_kind(backend),
                     BlockTransport::SimpleStorage,
                     "tests/data/storage_basic.txt");
    session.run_until_uart_contains("course-os> ",
                                    DebugBudget::kCourseOsShellBootMaxSteps);

    size_t offset = session.uart_output(0).next_offset;

    if (stage11_workflow_direct_gcc_probe()) {
        const char* command = stage11_workflow_direct_gcc_command();
        constexpr ExpectedText kDirectGcc[] = {
            {"linux-compat: path=/usr/bin/gcc", "gcc should resolve through PATH"},
            {"course-os> ", "gcc probe should return to prompt"},
        };
        constexpr ExpectedText kDirectGccAndAout[] = {
            {"linux-compat: path=/usr/bin/gcc", "gcc should resolve through PATH"},
            {"linux-compat: path=/a.out", "./a.out should run after gcc"},
            {"exec=real", "./a.out should use Linux compat real exec"},
            {"course-os> ", "gcc probe should return to prompt"},
        };
        const ExpectedText* expected =
            command_runs_aout(command) ? kDirectGccAndAout : kDirectGcc;
        const size_t expected_count =
            command_runs_aout(command)
                ? sizeof(kDirectGccAndAout) / sizeof(kDirectGccAndAout[0])
                : sizeof(kDirectGcc) / sizeof(kDirectGcc[0]);

        const Stage11WorkflowCommand direct_gcc_probe = {
            command,
            "direct gcc probe requested by environment",
            Stage11WorkflowStopMode::ExpectationSequence,
            kStage11ProbeDirectGcc,
            expected,
            expected_count,
            nullptr,
            nullptr,
            true,
        };
        if (!run_stage11_workflow_command(session, offset, direct_gcc_probe)) {
            return 1;
        }
        const DebugSession::UartOutputChunk chunk = session.uart_output(0);
        if (command_runs_aout(command) &&
            chunk.text.find("linux-compat: path=/a.out errno=2") !=
                std::string::npos) {
            std::fprintf(stderr,
                         "./a.out lookup failed after direct gcc probe\n");
            std::fprintf(stderr, "output was:\n%s\n", chunk.text.c_str());
            return 1;
        }
        return 0;
    }

    constexpr ExpectedText kGitInit[] = {
        {"linux-compat: rootfs=external", "git init should use external rootfs"},
        {"linux-compat: path=/usr/bin/git", "git init should resolve through PATH"},
        {"Initialized", "git init should create a repository"},
        {"course-os> ", "git init should return to prompt"},
    };
    constexpr ExpectedText kVimSave[] = {
        {"linux-compat: path=/usr/bin/vim", "vim should resolve through PATH"},
        {"course-os> ", "vim save should return to prompt"},
    };
    constexpr ExpectedText kGitAdd[] = {
        {"linux-compat: path=/usr/bin/git", "git add should resolve through PATH"},
        {"course-os> ", "git add should return to prompt"},
    };
    constexpr ExpectedText kGitCommit[] = {
        {"linux-compat: path=/usr/bin/git", "git commit should resolve through PATH"},
        {"file changed", "git commit should report the staged file delta"},
        {"create mode", "git commit should create hello.c in the first commit"},
    };
    constexpr ExpectedText kGitLog[] = {
        {"linux-compat: path=/usr/bin/git", "git log should resolve through PATH"},
        {"init", "git log should show the committed subject"},
        {"course-os> ", "git log should return to prompt"},
    };
    const Stage11WorkflowCommand kWorkflowCommands[] = {
        {"git init stage11repo\r",
         "create the writable workflow repository",
         Stage11WorkflowStopMode::ExpectationSequence,
         kStage11ProbeNone,
         kGitInit,
         sizeof(kGitInit) / sizeof(kGitInit[0]),
         nullptr,
         nullptr,
         true},
        {"vim stage11repo/hello.c\r",
         "open hello.c in vim",
         Stage11WorkflowStopMode::StopNeedle,
         kStage11ProbeNone,
         nullptr,
         0,
         "Press ENTER or type command to continue",
         nullptr,
         false},
        {"\r",
         "acknowledge vim new-file prompt",
         Stage11WorkflowStopMode::StopNeedle,
         kStage11ProbeNone,
         nullptr,
         0,
         "[New File]",
         nullptr,
         false},
        {"i#include <stdio.h>\nint main(){puts(\"stage11 hello\");return 0;}\x1b:wq\r",
         "write and save hello.c from vim",
         Stage11WorkflowStopMode::StopNeedle,
         kStage11ProbeNone,
         nullptr,
         0,
         "Press ENTER or type command to continue",
         nullptr,
         false},
        {"\r",
         "leave vim and return to shell",
         Stage11WorkflowStopMode::ExpectationSequence,
         kStage11ProbeNone,
         kVimSave,
         sizeof(kVimSave) / sizeof(kVimSave[0]),
         nullptr,
         nullptr,
         true},
        {"git -c safe.directory=/stage11repo -C stage11repo add hello.c\r",
         "stage hello.c in git",
         Stage11WorkflowStopMode::ExpectationSequence,
         kStage11ProbeNone,
         kGitAdd,
         sizeof(kGitAdd) / sizeof(kGitAdd[0]),
         nullptr,
         nullptr,
         true},
        {"git -C stage11repo -c safe.directory=/stage11repo "
         "-c user.name=stage11 "
         "-c user.email=stage11@example.invalid commit -m init\r",
         "commit the staged source file",
         Stage11WorkflowStopMode::PromptThenValidate,
         kStage11ProbeNone,
         kGitCommit,
         sizeof(kGitCommit) / sizeof(kGitCommit[0]),
         nullptr,
         nullptr,
         true},
        {"git -C stage11repo "
         "-c safe.directory=/stage11repo "
         "--no-pager log --oneline\r",
         "read back the git log",
         Stage11WorkflowStopMode::ExpectationSequence,
         kStage11ProbeNone,
         kGitLog,
         sizeof(kGitLog) / sizeof(kGitLog[0]),
         nullptr,
         nullptr,
         true},
    };

    if (!run_stage11_workflow_command(session, offset, kWorkflowCommands[0])) {
        return 1;
    }

    if (stage11_workflow_fast_commit_probe()) {
        const Stage11WorkflowCommand fast_commit_probe = {
            "git -c safe.directory=/stage11repo -C stage11repo "
            "-c user.name=s -c user.email=e@e "
            "commit --allow-empty -m i\r",
            "diagnostic fast commit probe requested by environment",
            Stage11WorkflowStopMode::ProbeStop,
            kStage11ProbeFastCommit,
            nullptr,
            0,
            "Bad address",
            "course-os> ",
            false,
        };
        (void)run_stage11_workflow_command(session, offset, fast_commit_probe);
        return 1;
    }

    for (size_t i = 1; i < sizeof(kWorkflowCommands) / sizeof(kWorkflowCommands[0]);
         ++i) {
        if (!run_stage11_workflow_command(session, offset, kWorkflowCommands[i])) {
            return 1;
        }
    }

    constexpr ExpectedText kFinalProbe[] = {
        {"linux-compat: path=/usr/bin/gcc", "gcc should resolve through PATH"},
        {"course-os> ", "gcc probe should return to prompt"},
    };
    constexpr ExpectedText kGcc[] = {
        {"linux-compat: path=/usr/bin/gcc", "gcc should resolve through PATH"},
        {"stage11 hello", "gcc hello.c && ./a.out should execute generated binary"},
        {"exec=real", "./a.out should run through Linux compat real exec"},
        {"course-os> ", "gcc workflow should return to prompt"},
    };
    const Stage11WorkflowCommand final_command = {
        stage11_workflow_final_command(),
        "compile hello.c and run the generated a.out",
        Stage11WorkflowStopMode::ExpectationSequence,
        kStage11ProbeFinalCommand,
        stage11_workflow_final_probe() ? kFinalProbe : kGcc,
        stage11_workflow_final_probe() ? sizeof(kFinalProbe) / sizeof(kFinalProbe[0])
                                       : sizeof(kGcc) / sizeof(kGcc[0]),
        nullptr,
        nullptr,
        true,
    };
    if (!run_stage11_workflow_command(session, offset, final_command)) {
        return 1;
    }

    return 0;
}
