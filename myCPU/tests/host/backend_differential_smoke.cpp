#include <array>
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>

#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/exec/backend.h"
#include "../../src/exec/functional_backend.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = 0x80000000ULL;
constexpr uint64_t kTrapVector = kEntry + 0x80;
constexpr uint64_t kDataAddr = kEntry + 0x100;

constexpr uint32_t kInvalidInsn = 0xffffffffU;
constexpr uint32_t kAddiX1FromX0Plus5 = 0x00500093U;      // addi x1, x0, 5
constexpr uint32_t kAddiX2FromX1Plus7 = 0x00708113U;      // addi x2, x1, 7
constexpr uint32_t kAddX3FromX1X2 = 0x002081b3U;          // add x3, x1, x2
constexpr uint32_t kSwX3ToX10 = 0x00352023U;              // sw x3, 0(x10)
constexpr uint32_t kLwX4FromX10 = 0x00052203U;            // lw x4, 0(x10)
constexpr uint32_t kAddiX6FromX4Plus1 = 0x00120313U;      // addi x6, x4, 1
constexpr uint32_t kCsrwMscratchX6 = 0x34031073U;         // csrw mscratch, x6
constexpr uint32_t kCsrrX5Mscratch = 0x340022f3U;         // csrr x5, mscratch
constexpr uint32_t kAddiX1WrongPath = 0x06300093U;        // addi x1, x0, 99
constexpr uint32_t kAddiX2FromX0Plus7 = 0x00700113U;      // addi x2, x0, 7
constexpr uint32_t kAddiX3WrongPath = 0x06300193U;        // addi x3, x0, 99
constexpr uint32_t kAddiX4WrongPath = 0x06300213U;        // addi x4, x0, 99
constexpr uint32_t kAddiX8FromX0Plus11 = 0x00b00413U;     // addi x8, x0, 11
constexpr uint32_t kJalX5Skip = 0x008002efU;              // jal x5, 8
constexpr uint32_t kBeqX0Taken = 0x00000463U;             // beq x0, x0, 8
constexpr uint32_t kAuipcX6 = 0x00000317U;                // auipc x6, 0
constexpr uint32_t kAddiX6Target16 = 0x01030313U;         // addi x6, x6, 16
constexpr uint32_t kJalrX7X6 = 0x000303e7U;               // jalr x7, x6, 0
constexpr uint32_t kAddiA7Exit = 0x05d00893U;             // addi a7, x0, 93
constexpr uint32_t kEcall = 0x00000073U;                  // ecall
constexpr uint32_t kCsrwMepcX6 = 0x34131073U;             // csrw mepc, x6
constexpr uint32_t kMret = 0x30200073U;                   // mret

constexpr std::array<uint32_t, 20> kTrackedCsrs{
    CSR_SSTATUS,
    CSR_SIE,
    CSR_STVEC,
    CSR_SCOUNTEREN,
    CSR_SSCRATCH,
    CSR_SEPC,
    CSR_SCAUSE,
    CSR_STVAL,
    CSR_SATP,
    CSR_MSTATUS,
    CSR_MISA,
    CSR_MEDELEG,
    CSR_MIDELEG,
    CSR_MIE,
    CSR_MTVEC,
    CSR_MCOUNTEREN,
    CSR_MSCRATCH,
    CSR_MEPC,
    CSR_MCAUSE,
    CSR_MTVAL,
};

struct MemoryWatch {
    uint64_t addr{0};
    int size{0};
};

struct Snapshot {
    uint64_t pc{0};
    uint64_t instret{0};
    bool halted{false};
    PrivilegeMode privilege{PrivilegeMode::Machine};
    std::array<uint64_t, 32> gprs{};
    std::array<uint64_t, kTrackedCsrs.size()> csrs{};
    std::vector<uint64_t> watched_memory{};
};

struct Scenario {
    const char* name{nullptr};
    std::vector<uint32_t> program{};
    std::vector<uint32_t> trap_program{};
    std::vector<MemoryWatch> watches{};
    int max_steps{64};
    std::function<void(CPU&, Ram&, Bus&)> configure{};
};

enum class BackendUnderTest : uint8_t {
    Functional,
    Pipeline,
};

struct TraceResult {
    bool halted{false};
    std::vector<Snapshot> events{};
    Snapshot final_state{};
};

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

std::unique_ptr<ExecutionBackend> make_backend(BackendUnderTest kind, CPU& cpu, Bus& bus) {
    switch (kind) {
    case BackendUnderTest::Functional:
        return std::make_unique<FunctionalBackend>(cpu, bus);
    case BackendUnderTest::Pipeline:
        return std::make_unique<PipelineBackend>(cpu, bus);
    }

    return nullptr;
}

Snapshot capture_snapshot(const CPU& cpu, Ram& ram, const std::vector<MemoryWatch>& watches) {
    Snapshot snapshot;
    snapshot.pc = cpu.core().pc();
    snapshot.instret = cpu.core().instret();
    snapshot.halted = cpu.core().halted();
    snapshot.privilege = cpu.core().privilege_mode();
    for (size_t i = 0; i < snapshot.gprs.size(); ++i) {
        snapshot.gprs[i] = cpu.core().read_gpr(static_cast<uint32_t>(i));
    }
    for (size_t i = 0; i < kTrackedCsrs.size(); ++i) {
        snapshot.csrs[i] = cpu.csr().read(kTrackedCsrs[i], cpu.core());
    }
    snapshot.watched_memory.reserve(watches.size());
    for (const MemoryWatch& watch : watches) {
        snapshot.watched_memory.push_back(ram.load(watch.addr, watch.size));
    }
    return snapshot;
}

bool is_commit_event(const Snapshot& previous, const Snapshot& current) {
    return current.instret != previous.instret ||
        current.pc != previous.pc ||
        current.halted != previous.halted ||
        current.privilege != previous.privilege ||
        current.csrs != previous.csrs;
}

const char* csr_name(uint32_t csr) {
    switch (csr) {
    case CSR_SSTATUS:
        return "sstatus";
    case CSR_SIE:
        return "sie";
    case CSR_STVEC:
        return "stvec";
    case CSR_SCOUNTEREN:
        return "scounteren";
    case CSR_SSCRATCH:
        return "sscratch";
    case CSR_SEPC:
        return "sepc";
    case CSR_SCAUSE:
        return "scause";
    case CSR_STVAL:
        return "stval";
    case CSR_SATP:
        return "satp";
    case CSR_MSTATUS:
        return "mstatus";
    case CSR_MISA:
        return "misa";
    case CSR_MEDELEG:
        return "medeleg";
    case CSR_MIDELEG:
        return "mideleg";
    case CSR_MIE:
        return "mie";
    case CSR_MTVEC:
        return "mtvec";
    case CSR_MCOUNTEREN:
        return "mcounteren";
    case CSR_MSCRATCH:
        return "mscratch";
    case CSR_MEPC:
        return "mepc";
    case CSR_MCAUSE:
        return "mcause";
    case CSR_MTVAL:
        return "mtval";
    default:
        return "unknown";
    }
}

bool report_snapshot_diff(const char* scenario_name, size_t index, const Snapshot& functional, const Snapshot& pipeline) {
    if (functional.pc != pipeline.pc) {
        std::fprintf(
            stderr,
            "[%s] event %zu pc mismatch: functional=0x%llx pipeline=0x%llx\n",
            scenario_name,
            index,
            static_cast<unsigned long long>(functional.pc),
            static_cast<unsigned long long>(pipeline.pc));
        return false;
    }
    if (functional.instret != pipeline.instret) {
        std::fprintf(
            stderr,
            "[%s] event %zu instret mismatch: functional=%llu pipeline=%llu\n",
            scenario_name,
            index,
            static_cast<unsigned long long>(functional.instret),
            static_cast<unsigned long long>(pipeline.instret));
        return false;
    }
    if (functional.halted != pipeline.halted) {
        std::fprintf(stderr, "[%s] event %zu halted mismatch\n", scenario_name, index);
        return false;
    }
    if (functional.privilege != pipeline.privilege) {
        std::fprintf(
            stderr,
            "[%s] event %zu privilege mismatch: functional=%u pipeline=%u\n",
            scenario_name,
            index,
            static_cast<unsigned>(functional.privilege),
            static_cast<unsigned>(pipeline.privilege));
        return false;
    }
    for (size_t i = 0; i < functional.gprs.size(); ++i) {
        if (functional.gprs[i] != pipeline.gprs[i]) {
            std::fprintf(
                stderr,
                "[%s] event %zu x%zu mismatch: functional=0x%llx pipeline=0x%llx\n",
                scenario_name,
                index,
                i,
                static_cast<unsigned long long>(functional.gprs[i]),
                static_cast<unsigned long long>(pipeline.gprs[i]));
            return false;
        }
    }
    for (size_t i = 0; i < functional.csrs.size(); ++i) {
        if (functional.csrs[i] != pipeline.csrs[i]) {
            std::fprintf(
                stderr,
                "[%s] event %zu %s mismatch: functional=0x%llx pipeline=0x%llx\n",
                scenario_name,
                index,
                csr_name(kTrackedCsrs[i]),
                static_cast<unsigned long long>(functional.csrs[i]),
                static_cast<unsigned long long>(pipeline.csrs[i]));
            return false;
        }
    }
    return true;
}

TraceResult collect_trace(const Scenario& scenario, BackendUnderTest kind) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    if (!scenario.trap_program.empty()) {
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
    }
    for (size_t i = 0; i < scenario.program.size(); ++i) {
        write32(ram, kEntry + static_cast<uint64_t>(i * 4), scenario.program[i]);
    }
    for (size_t i = 0; i < scenario.trap_program.size(); ++i) {
        write32(ram, kTrapVector + static_cast<uint64_t>(i * 4), scenario.trap_program[i]);
    }
    if (scenario.configure) {
        scenario.configure(cpu, ram, bus);
    }

    std::unique_ptr<ExecutionBackend> backend = make_backend(kind, cpu, bus);
    Snapshot previous = capture_snapshot(cpu, ram, scenario.watches);
    TraceResult result;

    for (int step = 0; step < scenario.max_steps; ++step) {
        backend->step();
        Snapshot current = capture_snapshot(cpu, ram, scenario.watches);
        if (is_commit_event(previous, current)) {
            result.events.push_back(current);
        }
        previous = current;
        if (current.halted) {
            result.halted = true;
            result.final_state = current;
            return result;
        }
    }

    result.final_state = previous;
    return result;
}

bool compare_trace(const Scenario& scenario) {
    const TraceResult functional = collect_trace(scenario, BackendUnderTest::Functional);
    if (!expect(functional.halted, "functional backend differential scenario did not halt")) {
        std::fprintf(stderr, "[%s] functional backend exceeded step budget\n", scenario.name);
        return false;
    }

    const TraceResult pipeline = collect_trace(scenario, BackendUnderTest::Pipeline);
    if (!expect(pipeline.halted, "pipeline backend differential scenario did not halt")) {
        std::fprintf(stderr, "[%s] pipeline backend exceeded step budget\n", scenario.name);
        return false;
    }

    if (functional.events.size() != pipeline.events.size()) {
        std::fprintf(
            stderr,
            "[%s] event count mismatch: functional=%zu pipeline=%zu\n",
            scenario.name,
            functional.events.size(),
            pipeline.events.size());
        return false;
    }

    for (size_t i = 0; i < functional.events.size(); ++i) {
        if (!report_snapshot_diff(scenario.name, i, functional.events[i], pipeline.events[i])) {
            return false;
        }
    }

    for (size_t i = 0; i < functional.final_state.watched_memory.size(); ++i) {
        if (functional.final_state.watched_memory[i] != pipeline.final_state.watched_memory[i]) {
            std::fprintf(
                stderr,
                "[%s] final watched memory %zu mismatch: functional=0x%llx pipeline=0x%llx\n",
                scenario.name,
                i,
                static_cast<unsigned long long>(functional.final_state.watched_memory[i]),
                static_cast<unsigned long long>(pipeline.final_state.watched_memory[i]));
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    const std::vector<Scenario> scenarios{
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
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(10, kDataAddr);
                ram.store(kDataAddr, 0, 4);
            },
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
            [](CPU& cpu, Ram&, Bus&) {
                cpu.core().write_gpr(6, kEntry + 8);
                cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
            },
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
            [](CPU& cpu, Ram&, Bus&) {
                cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
            },
        },
    };

    for (const Scenario& scenario : scenarios) {
        if (!compare_trace(scenario)) {
            return 1;
        }
    }

    return 0;
}
