#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "spike_differential/final_state.h"
#include "spike_differential/mycpu_runner.h"
#include "spike_differential/spike_runner.h"
#include "spike_differential/state_compare.h"

namespace {

using namespace spike_differential;

enum class DifferentialErrorKind : uint8_t {
    None,
    MissingSpike,
    LaunchFailure,
    ParseFailure,
    Timeout,
    UnsupportedScenario,
    MycpuTimeout,
    Mismatch,
};

struct DifferentialRunResult {
    bool ok{false};
    DifferentialErrorKind error_kind{DifferentialErrorKind::None};
    DiffReport diff_report{};
    std::string message{};
};

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

std::array<uint64_t, 32> make_initial_gprs(
    std::initializer_list<std::pair<uint32_t, uint64_t>> values) {
    std::array<uint64_t, 32> gprs{};
    for (const auto& entry : values) {
        if (entry.first < gprs.size()) {
            gprs[entry.first] = entry.second;
        }
    }
    return gprs;
}

std::array<uint64_t, kTrackedCsrs.size()> make_initial_csrs(
    std::initializer_list<std::pair<uint32_t, uint64_t>> values) {
    std::array<uint64_t, kTrackedCsrs.size()> csrs{};
    for (const auto& entry : values) {
        const size_t index = tracked_csr_index(entry.first);
        if (index < csrs.size()) {
            csrs[index] = entry.second;
        }
    }
    return csrs;
}

std::vector<Scenario> build_smoke_scenarios() {
    return {
        {
            "alu_mem_csr",
            {
                kAddiX1FromX0Plus5,
                kAddiX2FromX1Plus7,
                kAddX3FromX1X2,
                kSwX3ToX10,
                kLwX4FromX10,
                kAddiX6FromX4Plus1,
                kCsrwMscratchX6,
                kCsrrX5Mscratch,
                kAddiA7Exit,
                kEcall,
            },
            {},
            {
                {kDataAddr, 4},
            },
            64,
            {},
            Scenario::PlatformFixture::None,
            make_initial_gprs({{10, kDataAddr}}),
            {},
            {
                {kDataAddr, 0, 4},
            },
            PrivilegeMode::Machine,
        },
        {
            "control_flow",
            {
                kJalX5Skip,
                kAddiX1WrongPath,
                kAddiX2FromX0Plus7,
                kBeqX0Taken,
                kAddiX3WrongPath,
                kAuipcX6,
                kAddiX6Target16,
                kJalrX7X6,
                kAddiX4WrongPath,
                kAddiX8FromX0Plus11,
                kAddiA7Exit,
                kEcall,
            },
            {},
            {},
            64,
            {},
        },
        {
            "predictable_branch_loop",
            {
                kAddiX2FromX0Plus5,
                kAddiX1Inc,
                kBltX1X2Loop,
                kAddiA7Exit,
                kEcall,
            },
            {},
            {},
            128,
            {},
        },
        {
            "trap_return",
            {
                kEcall,
                kAddiX1WrongPath,
                kAddiX2FromX0Plus7,
                kAddiA7Exit,
                kEcall,
            },
            {
                kCsrwMepcX6,
                kMret,
            },
            {},
            64,
            {},
            Scenario::PlatformFixture::None,
            make_initial_gprs({{6, kEntry + 8}}),
            make_initial_csrs({{CSR_MTVEC, kTrapVector}}),
        },
        {
            "illegal_trap",
            {
                kInvalidInsn,
                kAddiX1WrongPath,
            },
            {
                kAddiA7Exit,
                kEcall,
            },
            {},
            64,
            {},
            Scenario::PlatformFixture::None,
            {},
            make_initial_csrs({{CSR_MTVEC, kTrapVector}}),
        },
        {
            "delegated_user_ecall_to_supervisor",
            {
                kEcall,
                kAddiX1WrongPath,
                kAddiA7Exit,
                kEcall,
            },
            {
                kAddiX5FromX0Plus256,
                kCsrwSstatusX5,
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            {},
            Scenario::PlatformFixture::None,
            make_initial_gprs({{7, kEntry + 8}}),
            make_initial_csrs({{CSR_MEDELEG, 1ULL << 8}, {CSR_STVEC, kTrapVector}}),
            {},
            PrivilegeMode::User,
        },
    };
}

Scenario find_scenario(const char* name) {
    for (const Scenario& scenario : build_smoke_scenarios()) {
        if (std::string_view(scenario.name) == name) {
            return scenario;
        }
    }
    return Scenario{};
}

const char* privilege_name(PrivilegeMode privilege) {
    switch (privilege) {
    case PrivilegeMode::Machine:
        return "M";
    case PrivilegeMode::Supervisor:
        return "S";
    case PrivilegeMode::User:
        return "U";
    }
    return "M";
}

std::string hex_u64(uint64_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
    return buffer;
}

std::string render_fake_spike_output(const FinalState& state) {
    std::ostringstream output;
    output << hex_u64(state.pc) << '\n';
    output << privilege_name(state.privilege) << '\n';
    output << hex_u64(state.instret) << '\n';
    for (uint64_t value : state.gprs) {
        output << hex_u64(value) << '\n';
    }
    for (uint64_t value : state.csrs) {
        output << hex_u64(value) << '\n';
    }
    for (uint64_t value : state.watched_memory) {
        output << hex_u64(value) << '\n';
    }
    return output.str();
}

bool write_executable_text(const std::string& path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file << contents;
    file.close();
    if (!file) {
        return false;
    }
    return chmod(path.c_str(), 0755) == 0;
}

std::string make_temp_exec_path(const char* tag) {
    char path_template[64];
    std::snprintf(path_template, sizeof(path_template), "/tmp/%s_XXXXXX", tag);
    const int fd = mkstemp(path_template);
    if (fd < 0) {
        return {};
    }
    close(fd);
    unlink(path_template);
    return path_template;
}

DifferentialErrorKind map_spike_error_kind(SpikeErrorKind error_kind) {
    switch (error_kind) {
    case SpikeErrorKind::None:
        return DifferentialErrorKind::None;
    case SpikeErrorKind::MissingSpike:
        return DifferentialErrorKind::MissingSpike;
    case SpikeErrorKind::LaunchFailure:
        return DifferentialErrorKind::LaunchFailure;
    case SpikeErrorKind::ParseFailure:
        return DifferentialErrorKind::ParseFailure;
    case SpikeErrorKind::Timeout:
        return DifferentialErrorKind::Timeout;
    case SpikeErrorKind::UnsupportedScenario:
        return DifferentialErrorKind::UnsupportedScenario;
    }
    return DifferentialErrorKind::LaunchFailure;
}

DifferentialRunResult run_differential_scenario(const Scenario& scenario,
                                                const SpikeRunnerOptions& spike_options) {
    DifferentialRunResult result;
    FinalState mycpu = run_mycpu_final_state(scenario);
    if (mycpu.timed_out) {
        result.error_kind = DifferentialErrorKind::MycpuTimeout;
        result.message = "[" + std::string(scenario.name ? scenario.name : "scenario") +
                         "] myCPU exceeded step budget";
        return result;
    }

    const SpikeRunResult spike = run_spike_final_state(scenario, spike_options);
    if (!spike.ok) {
        result.error_kind = map_spike_error_kind(spike.error_kind);
        result.message = spike.message;
        return result;
    }

    FinalState spike_state = spike.final_state;
    if (scenario.initial_privilege != PrivilegeMode::Machine) {
        const size_t mstatus_index = tracked_csr_index(CSR_MSTATUS);
        if (mstatus_index < mycpu.csrs.size()) {
            mycpu.csrs[mstatus_index] &= ~MSTATUS_MPIE;
            spike_state.csrs[mstatus_index] &= ~MSTATUS_MPIE;
        }
        const size_t mepc_index = tracked_csr_index(CSR_MEPC);
        if (mepc_index < mycpu.csrs.size()) {
            mycpu.csrs[mepc_index] = 0;
            spike_state.csrs[mepc_index] = 0;
        }
    }

    CompareOptions compare_options;
    compare_options.include_instret = false;
    compare_options.include_trap_summary = true;
    for (uint32_t instruction : scenario.trap_program) {
        if (instruction == kMret || instruction == kSret) {
            compare_options.include_trap_summary = false;
            break;
        }
    }
    result.diff_report = compare_final_state(scenario.name, mycpu, spike_state, compare_options);
    if (!result.diff_report.matched) {
        result.error_kind = DifferentialErrorKind::Mismatch;
        result.message = result.diff_report.message;
        return result;
    }

    result.ok = true;
    result.error_kind = DifferentialErrorKind::None;
    return result;
}

bool test_match_report() {
    FinalState expected;
    expected.halted = true;
    expected.pc = kEntry + 0x10;
    expected.privilege = PrivilegeMode::Machine;
    expected.gprs[1] = 5;
    expected.gprs[2] = 12;
    expected.csrs[tracked_csr_index(CSR_SSTATUS)] = 1;
    expected.csrs[tracked_csr_index(CSR_MSCRATCH)] = 13;
    expected.watched_memory = {17};
    const FinalState actual = expected;
    const DiffReport report = compare_final_state("match", expected, actual);
    return expect(report.matched, "expected identical states to match") &&
           expect(report.first_mismatch_kind == MismatchKind::None,
                  "expected matched report kind to be none") &&
           expect(report.first_mismatch_field.empty(),
                  "expected matched report field to stay empty") &&
           expect(report.message.empty(), "expected matched report message to stay empty");
}

bool test_gpr_mismatch_report() {
    FinalState expected;
    expected.halted = true;
    expected.gprs[2] = 12;
    FinalState actual = expected;
    actual.gprs[2] = 99;
    const DiffReport report = compare_final_state("gpr_mismatch", expected, actual);
    return expect(!report.matched, "expected gpr mismatch to fail compare") &&
           expect(report.first_mismatch_kind == MismatchKind::Gpr,
                  "expected gpr mismatch kind") &&
           expect(report.first_mismatch_field == "gpr[x2]",
                  "expected gpr mismatch field");
}

bool test_instret_mismatch_report() {
    FinalState expected;
    expected.halted = true;
    expected.instret = 11;
    FinalState actual = expected;
    actual.instret = 99;
    const DiffReport report = compare_final_state("instret_mismatch", expected, actual);
    return expect(!report.matched, "expected instret mismatch to fail compare") &&
           expect(report.first_mismatch_kind == MismatchKind::Instret,
                  "expected instret mismatch kind") &&
           expect(report.first_mismatch_field == "instret",
                  "expected instret mismatch field");
}

bool test_csr_mismatch_report() {
    FinalState expected;
    expected.halted = true;
    expected.csrs[tracked_csr_index(CSR_MSCRATCH)] = 13;
    FinalState actual = expected;
    actual.csrs[tracked_csr_index(CSR_MSCRATCH)] = 99;
    const DiffReport report = compare_final_state("csr_mismatch", expected, actual);
    return expect(!report.matched, "expected csr mismatch to fail compare") &&
           expect(report.first_mismatch_kind == MismatchKind::Csr,
                  "expected csr mismatch kind") &&
           expect(report.first_mismatch_field == "csr[mscratch]",
                  "expected csr mismatch field");
}

bool test_watched_memory_mismatch_report() {
    FinalState expected;
    expected.halted = true;
    expected.watched_memory = {17};
    FinalState actual = expected;
    actual.watched_memory[0] = 33;
    const DiffReport report = compare_final_state("watch_mismatch", expected, actual);
    return expect(!report.matched, "expected watched memory mismatch to fail compare") &&
           expect(report.first_mismatch_kind == MismatchKind::WatchedMemory,
                  "expected watched memory mismatch kind") &&
           expect(report.first_mismatch_field == "watched_memory[0]",
                  "expected watched memory mismatch field");
}

bool test_trap_subfields_ignored_when_not_trapped() {
    FinalState expected;
    expected.halted = true;
    FinalState actual = expected;
    actual.trap_summary.cause = 5;
    actual.trap_summary.tval = 6;
    actual.trap_summary.epc = kEntry;
    actual.trap_summary.privilege_at_trap = PrivilegeMode::Supervisor;
    const DiffReport report = compare_final_state("trap_not_taken", expected, actual);
    return expect(report.matched,
                  "expected trap subfields to be ignored when neither side trapped");
}

bool test_trap_mismatch_report() {
    FinalState expected;
    expected.halted = true;
    FinalState actual = expected;
    actual.trap_summary.trapped = true;
    actual.trap_summary.cause = 2;
    actual.trap_summary.epc = kEntry;
    const DiffReport report = compare_final_state("trap_mismatch", expected, actual);
    return expect(!report.matched, "expected trap mismatch to fail compare") &&
           expect(report.first_mismatch_kind == MismatchKind::TrapTrapped,
                  "expected trap mismatch kind") &&
           expect(report.first_mismatch_field == "trap_summary.trapped",
                  "expected trap mismatch field");
}

bool test_mycpu_runner_controlled_exit() {
    const Scenario scenario = find_scenario("alu_mem_csr");
    const FinalState state = run_mycpu_final_state(scenario);
    return expect(state.halted, "expected alu_mem_csr to halt") &&
           expect(!state.timed_out, "expected alu_mem_csr not to time out") &&
           expect(!state.trap_summary.trapped, "expected alu_mem_csr not to trap") &&
           expect(state.pc == kEntry + 40, "expected alu_mem_csr final pc after controlled exit") &&
           expect(state.gprs[5] == 18, "expected alu_mem_csr to preserve csrr result in x5") &&
           expect(state.watched_memory.size() == 1 && state.watched_memory[0] == 17,
                  "expected alu_mem_csr watched memory value") &&
           expect(state.csrs[tracked_csr_index(CSR_MSCRATCH)] == 18,
                  "expected alu_mem_csr mscratch final state") &&
           expect(state.exit_reason == "controlled_exit",
                  "expected alu_mem_csr controlled exit reason");
}

bool test_mycpu_runner_trap_return_preserves_first_trap_summary() {
    const Scenario scenario = find_scenario("trap_return");
    const FinalState state = run_mycpu_final_state(scenario);
    return expect(state.halted, "expected trap_return to halt") &&
           expect(!state.timed_out, "expected trap_return not to time out") &&
           expect(state.trap_summary.trapped, "expected trap_return to record trap summary") &&
           expect(state.trap_summary.cause == 11, "expected trap_return machine ecall cause") &&
           expect(state.trap_summary.epc == kEntry, "expected trap_return original epc") &&
           expect(state.trap_summary.privilege_at_trap == PrivilegeMode::Machine,
                  "expected trap_return trap privilege") &&
           expect(state.gprs[2] == 7, "expected trap_return resumed path to execute") &&
           expect(state.exit_reason == "controlled_exit",
                  "expected trap_return controlled exit reason");
}

bool test_mycpu_runner_illegal_trap_controlled_exit() {
    const Scenario scenario = find_scenario("illegal_trap");
    const FinalState state = run_mycpu_final_state(scenario);
    return expect(state.halted, "expected illegal_trap to halt via trap handler exit") &&
           expect(!state.timed_out, "expected illegal_trap not to time out") &&
           expect(state.trap_summary.trapped, "expected illegal_trap trap summary") &&
           expect(state.trap_summary.cause == 2, "expected illegal_trap illegal instruction cause") &&
           expect(state.trap_summary.tval == kInvalidInsn, "expected illegal_trap tval") &&
           expect(state.trap_summary.epc == kEntry, "expected illegal_trap epc") &&
           expect(state.exit_reason == "controlled_exit",
                  "expected illegal_trap controlled exit reason");
}

bool test_mycpu_runner_step_budget_timeout() {
    Scenario scenario;
    scenario.name = "step_budget_timeout";
    scenario.program = {
        kAddiX1Inc,
        kBltX1X2Loop,
    };
    scenario.max_steps = 16;
    scenario.initial_gprs = make_initial_gprs({{2, UINT64_C(1000)}});
    const FinalState state = run_mycpu_final_state(scenario);
    return expect(!state.halted, "expected step_budget_timeout not to halt") &&
           expect(state.timed_out, "expected step_budget_timeout to time out") &&
           expect(!state.trap_summary.trapped, "expected step_budget_timeout not to trap") &&
           expect(state.exit_reason == "step_budget_exhausted",
                  "expected step_budget_timeout exit reason");
}

bool test_spike_runner_missing_spike() {
    const SpikeRunResult result = run_spike_final_state(find_scenario("control_flow"),
                                                        SpikeRunnerOptions{
                                                            "/definitely/missing/spike",
                                                            50,
                                                            {},
                                                        });
    return expect(!result.ok, "expected missing spike to fail") &&
           expect(result.error_kind == SpikeErrorKind::MissingSpike,
                  "expected missing spike error kind") &&
           expect(result.message.find("failed to launch spike executable") != std::string::npos,
                  "expected missing spike message");
}

bool test_spike_runner_non_zero_exit() {
    const std::string fake_spike = make_temp_exec_path("fake_spike_exit");
    if (!expect(!fake_spike.empty(), "expected temp fake spike path")) {
        return false;
    }
    if (!expect(write_executable_text(fake_spike, "#!/bin/sh\necho boom\nexit 7\n"),
                "expected fake spike script to be written")) {
        unlink(fake_spike.c_str());
        return false;
    }
    const SpikeRunResult result = run_spike_final_state(find_scenario("control_flow"),
                                                        SpikeRunnerOptions{
                                                            fake_spike,
                                                            100,
                                                            {},
                                                        });
    unlink(fake_spike.c_str());
    return expect(!result.ok, "expected non-zero spike exit to fail") &&
           expect(result.error_kind == SpikeErrorKind::LaunchFailure,
                  "expected non-zero spike exit error kind") &&
           expect(result.message.find("status 7") != std::string::npos,
                  "expected non-zero spike exit status in message") &&
           expect(result.raw_output.find("boom") != std::string::npos,
                  "expected non-zero spike output capture");
}

bool test_spike_runner_parse_failure_missing_privilege() {
    FinalState state;
    std::string error;
    SpikeScenarioPlan plan;
    if (!expect(build_spike_scenario_plan(find_scenario("control_flow"), plan, error),
                "expected control_flow spike plan to build")) {
        return false;
    }
    const bool parsed = parse_spike_final_state_output(find_scenario("control_flow"),
                                                       plan,
                                                       "0x8000002c\n0x10\n",
                                                       state,
                                                       error);
    return expect(!parsed, "expected missing privilege line to fail parse") &&
           expect(error.find("missing privilege") != std::string::npos,
                  "expected parse failure reason to mention missing privilege");
}

bool test_spike_runner_parse_failure_on_extra_numeric_output() {
    const Scenario scenario = find_scenario("control_flow");
    FinalState reference = run_mycpu_final_state(scenario);
    std::string output = render_fake_spike_output(reference);
    output += "0x999\n";

    FinalState parsed_state;
    std::string error;
    SpikeScenarioPlan plan;
    if (!expect(build_spike_scenario_plan(scenario, plan, error),
                "expected control_flow spike plan to build")) {
        return false;
    }
    const bool parsed =
        parse_spike_final_state_output(scenario, plan, output, parsed_state, error);
    return expect(!parsed, "expected extra numeric line to fail parse") &&
           expect(error.find("unexpected field count") != std::string::npos,
                  "expected parse failure reason to mention field count");
}

bool test_spike_runner_timeout() {
    const std::string fake_spike = make_temp_exec_path("fake_spike_timeout");
    if (!expect(!fake_spike.empty(), "expected temp timeout fake spike path")) {
        return false;
    }
    if (!expect(write_executable_text(fake_spike, "#!/bin/sh\nsleep 5\n"),
                "expected timeout fake spike script to be written")) {
        unlink(fake_spike.c_str());
        return false;
    }
    const SpikeRunResult result = run_spike_final_state(find_scenario("control_flow"),
                                                        SpikeRunnerOptions{
                                                            fake_spike,
                                                            20,
                                                            {},
                                                        });
    unlink(fake_spike.c_str());
    return expect(!result.ok, "expected spike timeout to fail") &&
           expect(result.error_kind == SpikeErrorKind::Timeout,
                  "expected spike timeout error kind") &&
           expect(result.message.find("timed out") != std::string::npos,
                  "expected spike timeout message");
}

bool test_spike_runner_supervisor_trap_inference_from_final_csrs() {
    const Scenario scenario = find_scenario("delegated_user_ecall_to_supervisor");
    SpikeScenarioPlan plan;
    std::string error;
    if (!expect(build_spike_scenario_plan(scenario, plan, error),
                "expected delegated scenario spike plan to build")) {
        return false;
    }

    std::ostringstream output;
    output << "0x" << std::hex << (kEntry + 8) << '\n';
    output << "S\n";
    output << "0x23\n";
    for (int i = 0; i < 32; ++i) {
        output << "0x" << std::hex << i << '\n';
    }
    for (uint32_t csr : kTrackedCsrs) {
        uint64_t value = 0;
        if (csr == CSR_STVEC) {
            value = kTrapVector;
        } else if (csr == CSR_SCAUSE) {
            value = 8;
        } else if (csr == CSR_SEPC) {
            value = kEntry;
        } else if (csr == CSR_STVAL) {
            value = 0x44;
        }
        output << "0x" << std::hex << value << '\n';
    }

    FinalState state;
    const bool parsed = parse_spike_final_state_output(scenario, plan, output.str(), state, error);
    return expect(parsed, "expected supervisor trap spike output to parse") &&
           expect(state.instret == 0x23, "expected supervisor trap instret") &&
           expect(state.privilege == PrivilegeMode::Supervisor,
                  "expected supervisor trap privilege") &&
           expect(state.trap_summary.trapped, "expected supervisor trap summary") &&
           expect(state.trap_summary.cause == 8, "expected supervisor trap cause") &&
           expect(state.trap_summary.epc == kEntry, "expected supervisor trap epc") &&
           expect(state.trap_summary.tval == 0x44, "expected supervisor trap tval") &&
           expect(state.trap_summary.privilege_at_trap == PrivilegeMode::Supervisor,
                  "expected supervisor trap privilege at trap");
}

bool test_spike_runner_machine_trap_inference_from_final_csrs() {
    const Scenario scenario = find_scenario("trap_return");
    SpikeScenarioPlan plan;
    std::string error;
    if (!expect(build_spike_scenario_plan(scenario, plan, error),
                "expected trap_return spike plan to build")) {
        return false;
    }

    std::ostringstream output;
    output << "0x" << std::hex << (kEntry + 8) << '\n';
    output << "M\n";
    output << "0x2a\n";
    for (int i = 0; i < 32; ++i) {
        output << "0x" << std::hex << (0x100 + i) << '\n';
    }
    for (uint32_t csr : kTrackedCsrs) {
        uint64_t value = 0;
        if (csr == CSR_MTVEC) {
            value = kTrapVector;
        } else if (csr == CSR_MCAUSE) {
            value = 11;
        } else if (csr == CSR_MEPC) {
            value = kEntry;
        } else if (csr == CSR_MTVAL) {
            value = 0;
        }
        output << "0x" << std::hex << value << '\n';
    }

    FinalState state;
    const bool parsed = parse_spike_final_state_output(scenario, plan, output.str(), state, error);
    return expect(parsed, "expected machine trap spike output to parse") &&
           expect(state.instret == 0x2a, "expected machine trap instret") &&
           expect(state.privilege == PrivilegeMode::Machine,
                  "expected machine trap privilege") &&
           expect(state.trap_summary.trapped, "expected machine trap summary") &&
           expect(state.trap_summary.cause == 11, "expected machine trap cause") &&
           expect(state.trap_summary.epc == kEntry, "expected machine trap epc") &&
           expect(state.trap_summary.privilege_at_trap == PrivilegeMode::Machine,
                  "expected machine trap privilege at trap");
}

bool test_spike_runner_ambiguous_trap_state_does_not_infer_trap() {
    const Scenario scenario = find_scenario("control_flow");
    SpikeScenarioPlan plan;
    std::string error;
    if (!expect(build_spike_scenario_plan(scenario, plan, error),
                "expected control_flow spike plan to build")) {
        return false;
    }

    std::ostringstream output;
    output << "0x" << std::hex << (kEntry + 8) << '\n';
    output << "M\n";
    output << "0x31\n";
    for (int i = 0; i < 32; ++i) {
        output << "0x" << std::hex << (0x200 + i) << '\n';
    }
    for (uint32_t csr : kTrackedCsrs) {
        uint64_t value = 0;
        if (csr == CSR_SCAUSE) {
            value = 8;
        } else if (csr == CSR_SEPC) {
            value = kEntry;
        } else if (csr == CSR_STVAL) {
            value = 0x44;
        } else if (csr == CSR_MCAUSE) {
            value = 2;
        } else if (csr == CSR_MEPC) {
            value = kEntry + 4;
        } else if (csr == CSR_MTVAL) {
            value = kInvalidInsn;
        }
        output << "0x" << std::hex << value << '\n';
    }

    FinalState state;
    const bool parsed = parse_spike_final_state_output(scenario, plan, output.str(), state, error);
    return expect(parsed, "expected ambiguous trap spike output to parse") &&
           expect(state.privilege == PrivilegeMode::Machine,
                  "expected ambiguous trap privilege to stay machine") &&
           expect(!state.trap_summary.trapped,
                  "expected ambiguous trap state not to infer trap summary");
}

bool test_mycpu_runner_supervisor_trap_summary() {
    const Scenario scenario = find_scenario("delegated_user_ecall_to_supervisor");
    const FinalState state = run_mycpu_final_state(scenario);
    return expect(state.halted, "expected delegated_user_ecall_to_supervisor to halt") &&
           expect(!state.timed_out, "expected delegated_user_ecall_to_supervisor not to time out") &&
           expect(state.trap_summary.trapped,
                  "expected delegated_user_ecall_to_supervisor trap summary") &&
           expect(state.trap_summary.cause == 8, "expected delegated user ecall cause") &&
           expect(state.trap_summary.epc == kEntry, "expected delegated user ecall epc") &&
           expect(state.trap_summary.privilege_at_trap == PrivilegeMode::Supervisor,
                  "expected delegated user ecall trap privilege") &&
           expect(state.privilege == PrivilegeMode::Supervisor,
                  "expected delegated scenario to remain in supervisor before exit") &&
           expect(state.exit_reason == "controlled_exit",
                  "expected delegated scenario controlled exit reason");
}

bool test_scripted_positive_differential_supports_privilege_scenario() {
    const Scenario scenario = find_scenario("delegated_user_ecall_to_supervisor");
    const FinalState mycpu = run_mycpu_final_state(scenario);
    const std::string fake_spike = make_temp_exec_path("fake_spike_positive");
    if (!expect(!fake_spike.empty(), "expected temp positive fake spike path")) {
        return false;
    }
    const std::string script = std::string("#!/bin/sh\ncat <<'EOF'\n") +
                               render_fake_spike_output(mycpu) + "EOF\n";
    if (!expect(write_executable_text(fake_spike, script),
                "expected positive fake spike script to be written")) {
        unlink(fake_spike.c_str());
        return false;
    }

    const DifferentialRunResult result =
        run_differential_scenario(scenario, SpikeRunnerOptions{fake_spike, 100, {}});
    unlink(fake_spike.c_str());
    if (!result.ok && !result.message.empty()) {
        std::fprintf(stderr, "%s\n", result.message.c_str());
    }
    return expect(result.ok,
                  "expected scripted positive differential to pass delegated privilege scenario");
}

bool test_differential_reports_mycpu_timeout() {
    Scenario scenario;
    scenario.name = "differential_timeout";
    scenario.program = {
        kAddiX1Inc,
        kBltX1X2Loop,
    };
    scenario.max_steps = 16;
    scenario.initial_gprs = make_initial_gprs({{2, UINT64_C(1000)}});

    const DifferentialRunResult result = run_differential_scenario(
        scenario,
        SpikeRunnerOptions{"/definitely/missing/spike", 50, {}});
    return expect(!result.ok, "expected timeout differential to fail") &&
           expect(result.error_kind == DifferentialErrorKind::MycpuTimeout,
                  "expected differential timeout error kind");
}

bool run_self_tests() {
    return test_match_report() && test_gpr_mismatch_report() && test_instret_mismatch_report() &&
           test_csr_mismatch_report() && test_watched_memory_mismatch_report() &&
           test_trap_subfields_ignored_when_not_trapped() && test_trap_mismatch_report() &&
           test_mycpu_runner_controlled_exit() &&
           test_mycpu_runner_trap_return_preserves_first_trap_summary() &&
           test_mycpu_runner_illegal_trap_controlled_exit() &&
           test_mycpu_runner_step_budget_timeout() && test_spike_runner_missing_spike() &&
           test_spike_runner_non_zero_exit() &&
           test_spike_runner_parse_failure_missing_privilege() &&
           test_spike_runner_parse_failure_on_extra_numeric_output() &&
           test_spike_runner_timeout() &&
           test_spike_runner_supervisor_trap_inference_from_final_csrs() &&
           test_spike_runner_machine_trap_inference_from_final_csrs() &&
           test_spike_runner_ambiguous_trap_state_does_not_infer_trap() &&
           test_mycpu_runner_supervisor_trap_summary() &&
           test_scripted_positive_differential_supports_privilege_scenario() &&
           test_differential_reports_mycpu_timeout();
}

std::string default_spike_path() {
    if (const char* env = std::getenv("SPIKE_PATH")) {
        if (*env != '\0') {
            return env;
        }
    }
    if (const char* env = std::getenv("SPIKE_BIN")) {
        if (*env != '\0') {
            return env;
        }
    }
    return "spike";
}

int run_real_differential_mode(const char* selected_scenario) {
    std::vector<const char*> scenario_names;
    if (selected_scenario != nullptr) {
        scenario_names.push_back(selected_scenario);
    } else {
        scenario_names = {
            "alu_mem_csr",
            "control_flow",
            "predictable_branch_loop",
            "trap_return",
            "illegal_trap",
            "delegated_user_ecall_to_supervisor",
        };
    }
    const SpikeRunnerOptions options{default_spike_path(), 5000, {}};
    for (const char* scenario_name : scenario_names) {
        const Scenario scenario = find_scenario(scenario_name);
        if (scenario.name == nullptr) {
            std::fprintf(stderr, "unknown spike differential scenario: %s\n", scenario_name);
            return 1;
        }
        const DifferentialRunResult result = run_differential_scenario(scenario, options);
        if (!result.ok) {
            std::fprintf(stderr, "%s\n", result.message.c_str());
            return 1;
        }
        std::printf("[%s] matched\n", scenario_name);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--run-differential") {
        const char* scenario_name = argc > 2 ? argv[2] : nullptr;
        return run_real_differential_mode(scenario_name);
    }
    return run_self_tests() ? 0 : 1;
}
