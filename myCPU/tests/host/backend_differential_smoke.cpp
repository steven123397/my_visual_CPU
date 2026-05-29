#include <array>
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>

#include "spike_differential/shared_spec.h"

#include "../../include/platform_mmio.h"
#include "../../src/devices/plic.h"
#include "../../src/devices/uart16550.h"
#include "../../src/exec/backend.h"
#include "../../src/exec/functional_backend.h"
#include "../../src/exec/pipeline_backend.h"

namespace {

using namespace spike_differential;

constexpr uint64_t kMachineExitVector = kEntry + 0xc0;
constexpr uint32_t kEbreak = 0x00100073U;

struct Snapshot {
    uint64_t pc{0};
    uint64_t instret{0};
    bool halted{false};
    PrivilegeMode privilege{PrivilegeMode::Machine};
    std::array<uint64_t, 32> gprs{};
    std::array<uint64_t, kTrackedCsrs.size()> csrs{};
    std::vector<uint64_t> watched_memory{};
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

void write16(Ram& ram, uint64_t addr, uint16_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

void install_machine_exit_handler(CPU& cpu, Ram& ram) {
    cpu.csr().write(CSR_MTVEC, kMachineExitVector, cpu.core());
    write32(ram, kMachineExitVector + 0, kAddiA7Exit);
    write32(ram, kMachineExitVector + 4, kEcall);
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

bool is_interrupt_entry_event(const Snapshot& previous, const Snapshot& current) {
    constexpr uint64_t kInterruptCauseBit = 1ULL << 63;
    const uint64_t supervisorTrapBase = current.csrs[2] & ~0x3ULL;
    const uint64_t machineTrapBase = current.csrs[14] & ~0x3ULL;
    const bool machine_interrupt_entered =
        current.csrs[18] != previous.csrs[18] &&
        (current.csrs[18] & kInterruptCauseBit) != 0 &&
        current.pc == machineTrapBase;
    const bool supervisor_interrupt_entered =
        current.csrs[6] != previous.csrs[6] &&
        (current.csrs[6] & kInterruptCauseBit) != 0 &&
        current.pc == supervisorTrapBase;
    return machine_interrupt_entered || supervisor_interrupt_entered;
}

void dump_trace_summary(const char* label, const std::vector<Snapshot>& events) {
    std::fprintf(stderr, "%s trace (%zu events):\n", label, events.size());
    for (size_t i = 0; i < events.size(); ++i) {
        const Snapshot& event = events[i];
        std::fprintf(
            stderr,
            "  [%zu] pc=0x%llx instret=%llu halted=%d priv=%u mepc=0x%llx mcause=0x%llx mtval=0x%llx sepc=0x%llx scause=0x%llx stval=0x%llx\n",
            i,
            static_cast<unsigned long long>(event.pc),
            static_cast<unsigned long long>(event.instret),
            event.halted ? 1 : 0,
            static_cast<unsigned>(event.privilege),
            static_cast<unsigned long long>(event.csrs[17]),
            static_cast<unsigned long long>(event.csrs[18]),
            static_cast<unsigned long long>(event.csrs[19]),
            static_cast<unsigned long long>(event.csrs[5]),
            static_cast<unsigned long long>(event.csrs[6]),
            static_cast<unsigned long long>(event.csrs[7]));
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
        std::fprintf(stderr, "[%s] event %zu privilege mismatch\n", scenario_name, index);
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
    std::unique_ptr<Plic> plic;
    std::unique_ptr<Uart16550> uart;
    if (scenario.fixture == Scenario::PlatformFixture::UartPlic) {
        plic = std::make_unique<Plic>();
        uart = std::make_unique<Uart16550>(*plic);
        uart->set_mirror_stdout(false);
        bus.attach(*plic);
        bus.attach(*uart);
    }
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
            // The functional backend services interrupts and then executes the
            // first trap-handler instruction in the same step. The pipeline
            // backend exposes a transient interrupt-entry state before that
            // retirement, so normalize it out before comparing traces.
            if (!is_interrupt_entry_event(previous, current)) {
                result.events.push_back(current);
            }
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
        dump_trace_summary("functional", functional.events);
        dump_trace_summary("pipeline", pipeline.events);
        return false;
    }

    for (size_t i = 0; i < functional.events.size(); ++i) {
        if (!report_snapshot_diff(scenario.name, i, functional.events[i], pipeline.events[i])) {
            dump_trace_summary("functional", functional.events);
            dump_trace_summary("pipeline", pipeline.events);
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
            "rvc_halfword_control_flow",
            {},
            {},
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                constexpr uint16_t kCAddiA0Minus1 = 0x157DU;  // c.addi a0, -1
                constexpr uint16_t kCBeqzA0Plus4 = 0xC111U;   // c.beqz a0, +4
                constexpr uint16_t kCAddiwA1Plus1 = 0x2585U;  // c.addiw a1, 1

                cpu.core().write_gpr(10, 1);
                write16(ram, kEntry + 0, kCAddiA0Minus1);
                write16(ram, kEntry + 2, kCBeqzA0Plus4);
                write16(ram, kEntry + 4, kCAddiwA1Plus1);
                write16(ram, kEntry + 6, kCAddiwA1Plus1);
                write32(ram, kEntry + 8, kAddiA7Exit);
                write32(ram, kEntry + 12, kEcall);
            },
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
        {
            "machine_timer_interrupt_at_cycle_start",
            {
                kAddiX1WrongPath,
                kAddiA7Exit,
                kEcall,
            },
            {
                kCsrwMepcX6,
                kMret,
            },
            {},
            128,
            [](CPU& cpu, Ram&, Bus&) {
                cpu.core().write_gpr(6, kEntry + 4);

                cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
                cpu.csr().write(CSR_MIE, MIE_MTIE, cpu.core());
                cpu.csr().write(CSR_MSTATUS, MSTATUS_MIE, cpu.core());
                cpu.csr().write(CSR_MIP, MIE_MTIE, cpu.core());
            },
        },
        {
            "delegated_user_ecall_to_supervisor",
            {
                kEcall,
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                kAddiX5FromX0Plus256,
                kCsrwSstatusX5,
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::User);

                cpu.csr().write(CSR_MEDELEG, 1ULL << 8, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                install_machine_exit_handler(cpu, ram);
            },
        },
        {
            "sret_to_user_halt",
            {
                kSret,
                kAddiX1WrongPath,
                kEbreak,
            },
            {},
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_SEPC, kEntry + 8, cpu.core());
                cpu.csr().write(CSR_MSTATUS, 0, cpu.core());  // SPP=U, SPIE=0
                install_machine_exit_handler(cpu, ram);
            },
        },
        {
            "sv39_mprv_fault",
            {
                0x00052083U,  // lw x1, 0(x10)
                0x00b52023U,  // sw x11, 0(x10)
                0x00052603U,  // lw x12, 0(x10)
                0x30079073U,  // csrw mstatus, x15
                0x00072683U,  // lw x13, 0(x14)
                kAddiA7Exit,
                kEcall,
            },
            {
                0x34131073U,  // csrw mepc, x6
                kMret,
            },
            {
                {0x80104000ULL, 4},
            },
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                constexpr uint64_t kSatpSv39 = 8ULL << 60;
                constexpr uint64_t kRootPageTable = 0x80100000ULL;
                constexpr uint64_t kLevel1PageTable = 0x80101000ULL;
                constexpr uint64_t kLevel0PageTable = 0x80102000ULL;
                constexpr uint64_t kUserBackingPage = 0x80103000ULL;
                constexpr uint64_t kSupervisorBackingPage = 0x80104000ULL;
                constexpr uint64_t kUserVirtualPage = 0x80002000ULL;
                constexpr uint64_t kSupervisorVirtualPage = 0x80003000ULL;

                // Root[2] -> level-1 table
                write64(ram, kRootPageTable + 16, ((kLevel1PageTable >> 12) << 10) | 0x1ULL);
                // Level-1[0] -> level-0 table
                write64(ram, kLevel1PageTable + 0, ((kLevel0PageTable >> 12) << 10) | 0x1ULL);
                // Level-0[2]: user data page
                write64(ram, kLevel0PageTable + 16, ((kUserBackingPage >> 12) << 10) | 0x17ULL);
                // Level-0[3]: supervisor-only data page
                write64(ram, kLevel0PageTable + 24, ((kSupervisorBackingPage >> 12) << 10) | 0x7ULL);

                ram.store(kUserBackingPage, 0x11223344U, 4);
                ram.store(kSupervisorBackingPage, 0x55667788U, 4);

                cpu.core().write_gpr(10, kUserVirtualPage);
                cpu.core().write_gpr(11, 0x01020304U);
                cpu.core().write_gpr(14, kSupervisorVirtualPage);
                cpu.core().write_gpr(15, MSTATUS_MPRV);  // MPP=U, MPRV=1, SUM=0
                cpu.core().write_gpr(6, kEntry + 20);    // resume at addi a7, x0, 93

                cpu.csr().write(CSR_SATP, kSatpSv39 | (kRootPageTable >> 12), cpu.core());
                cpu.csr().write(
                    CSR_MSTATUS,
                    MSTATUS_MPRV | (1ULL << MSTATUS_MPP_SHIFT) | MSTATUS_SUM,
                    cpu.core());
                cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
                cpu.address_space().flush_tlb();
            },
        },
        {
            "sv39_instruction_page_fault",
            {
                0x00030067U,  // jalr x0, x6, 0
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                0x14139073U,  // csrw sepc, x7
                0x10200073U,  // sret
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                constexpr uint64_t kSatpSv39 = 8ULL << 60;
                constexpr uint64_t kRootPageTable = 0x80100000ULL;
                constexpr uint64_t kLevel1PageTable = 0x80101000ULL;
                constexpr uint64_t kLevel0PageTable = 0x80102000ULL;
                constexpr uint64_t kUserExecBackingPage = 0x80103000ULL;
                constexpr uint64_t kUserExecVirtualPage = 0x80001000ULL;

                // Root[2] -> level-1 table
                write64(ram, kRootPageTable + 16, ((kLevel1PageTable >> 12) << 10) | 0x1ULL);
                // Level-1[0] -> level-0 table
                write64(ram, kLevel1PageTable + 0, ((kLevel0PageTable >> 12) << 10) | 0x1ULL);
                // Level-0[0]: code page at kEntry, executable in S-mode
                write64(ram, kLevel0PageTable + 0, ((kEntry >> 12) << 10) | 0xFULL);
                // Level-0[1]: user executable page, must fault when fetched in S-mode
                write64(ram, kLevel0PageTable + 8, ((kUserExecBackingPage >> 12) << 10) | 0x19ULL);

                // Populate the user-exec backing page with a harmless instruction
                // so a buggy implementation would continue past the jump.
                write32(ram, kUserExecBackingPage, kAddiX1WrongPath);

                cpu.core().write_gpr(6, kUserExecVirtualPage);
                cpu.core().write_gpr(7, kEntry + 8);  // resume at addi a7, x0, 93
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_SATP, kSatpSv39 | (kRootPageTable >> 12), cpu.core());
                cpu.csr().write(CSR_MEDELEG, 1ULL << 12, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                install_machine_exit_handler(cpu, ram);
                cpu.address_space().flush_tlb();
            },
        },
        {
            "supervisor_mmio_instruction_access_fault",
            {
                0x00030067U,  // jalr x0, x6, 0
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(6, UART_BASE);
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_MEDELEG, 1ULL << 1, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                install_machine_exit_handler(cpu, ram);
            },
            Scenario::PlatformFixture::UartPlic,
        },
        {
            "sv39_load_page_fault",
            {
                kLwX1FromX10,
                kAddiX2WrongPath,
                kEbreak,
            },
            {
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                constexpr uint64_t kSatpSv39 = 8ULL << 60;
                constexpr uint64_t kRootPageTable = 0x80100000ULL;
                constexpr uint64_t kFaultVirtualAddr = 0xC0000000ULL;

                // Root[2]: 1GiB leaf covering the executable image at 0x80000000.
                write64(ram, kRootPageTable + 16, ((kEntry >> 12) << 10) | 0xFULL);

                cpu.core().write_gpr(10, kFaultVirtualAddr);
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_SATP, kSatpSv39 | (kRootPageTable >> 12), cpu.core());
                cpu.csr().write(CSR_MEDELEG, 1ULL << 13, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                install_machine_exit_handler(cpu, ram);
                cpu.address_space().flush_tlb();
            },
        },
        {
            "sv39_store_page_fault",
            {
                kSwX11ToX10,
                kAddiX3WrongPath,
                kEbreak,
            },
            {
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                constexpr uint64_t kSatpSv39 = 8ULL << 60;
                constexpr uint64_t kRootPageTable = 0x80100000ULL;
                constexpr uint64_t kFaultVirtualAddr = 0xC0001000ULL;

                // Root[2]: 1GiB leaf covering the executable image at 0x80000000.
                write64(ram, kRootPageTable + 16, ((kEntry >> 12) << 10) | 0xFULL);

                cpu.core().write_gpr(10, kFaultVirtualAddr);
                cpu.core().write_gpr(11, 0x12345678U);
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_SATP, kSatpSv39 | (kRootPageTable >> 12), cpu.core());
                cpu.csr().write(CSR_MEDELEG, 1ULL << 15, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                install_machine_exit_handler(cpu, ram);
                cpu.address_space().flush_tlb();
            },
        },
        {
            "sv39_reserved_non_leaf_fault",
            {
                kLwX1FromX10,
                kAddiX4WrongPath,
                kEbreak,
            },
            {
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                constexpr uint64_t kSatpSv39 = 8ULL << 60;
                constexpr uint64_t kRootPageTable = 0x80100000ULL;
                constexpr uint64_t kLevel1PageTable = 0x80101000ULL;
                constexpr uint64_t kLevel0CodePageTable = 0x80102000ULL;
                constexpr uint64_t kLevel0ReservedPageTable = 0x80103000ULL;
                constexpr uint64_t kBackingPage = 0x80104000ULL;
                constexpr uint64_t kFaultVirtualAddr = 0x80400000ULL;

                // Root[2] -> level-1 table
                write64(ram, kRootPageTable + 16, ((kLevel1PageTable >> 12) << 10) | 0x1ULL);
                // Level-1[0] -> level-0 code page table
                write64(ram, kLevel1PageTable + 0, ((kLevel0CodePageTable >> 12) << 10) | 0x1ULL);
                // Level-0[0]: executable code page at kEntry
                write64(ram, kLevel0CodePageTable + 0, ((kEntry >> 12) << 10) | 0xFULL);
                // Level-1[2]: reserved non-leaf pointer with U bit set
                write64(ram, kLevel1PageTable + 16, ((kLevel0ReservedPageTable >> 12) << 10) | 0x11ULL);
                // Leaf that must never become reachable if reserved-bit check works
                write64(ram, kLevel0ReservedPageTable + 0, ((kBackingPage >> 12) << 10) | 0x7ULL);
                ram.store(kBackingPage, 0xCAFEBABEU, 4);

                cpu.core().write_gpr(10, kFaultVirtualAddr);
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_SATP, kSatpSv39 | (kRootPageTable >> 12), cpu.core());
                cpu.csr().write(CSR_MEDELEG, 1ULL << 13, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                install_machine_exit_handler(cpu, ram);
                cpu.address_space().flush_tlb();
            },
        },
        {
            "supervisor_mmio_load_access_fault",
            {
                kLwX1FromX10,
                kAddiX2WrongPath,
                kEbreak,
            },
            {
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(10, UART_BASE);
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_MEDELEG, 1ULL << 5, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                install_machine_exit_handler(cpu, ram);
            },
            Scenario::PlatformFixture::UartPlic,
        },
        {
            "supervisor_mmio_store_access_fault",
            {
                kSwX11ToX10,
                kAddiX3WrongPath,
                kEbreak,
            },
            {
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(10, UART_BASE);
                cpu.core().write_gpr(11, 0x41);
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_MEDELEG, 1ULL << 7, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                install_machine_exit_handler(cpu, ram);
            },
            Scenario::PlatformFixture::UartPlic,
        },
        {
            "supervisor_timer_interrupt_after_mret",
            {
                kMret,
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                kCsrrcSipX5,
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(5, MIE_STIE);
                cpu.core().write_gpr(7, kEntry + 8);

                cpu.csr().write(CSR_MEPC, kEntry + 8, cpu.core());
                cpu.csr().write(CSR_MIDELEG, MIE_STIE, cpu.core());
                cpu.csr().write(CSR_MIE, MIE_STIE, cpu.core());
                cpu.csr().write(
                    CSR_MSTATUS,
                    MSTATUS_SIE | (1ULL << MSTATUS_MPP_SHIFT),
                    cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                cpu.csr().write(CSR_MIP, MIE_STIE, cpu.core());
                install_machine_exit_handler(cpu, ram);
            },
        },
        {
            "supervisor_timer_interrupt_after_sip_write",
            {
                kCsrwSipX5,
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                kCsrrcSipX5,
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(5, MIE_STIE);
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_MIDELEG, MIE_STIE, cpu.core());
                cpu.csr().write(CSR_SIE, MIE_STIE, cpu.core());
                cpu.csr().write(CSR_MSTATUS, MSTATUS_SIE, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                install_machine_exit_handler(cpu, ram);
            },
        },
        {
            "supervisor_timer_interrupt_after_sie_write",
            {
                kCsrwSieX5,
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                kCsrrcSipX5,
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(5, MIE_STIE);
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_MIDELEG, MIE_STIE, cpu.core());
                cpu.csr().write(CSR_MSTATUS, MSTATUS_SIE, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                cpu.csr().write(CSR_SIP, MIE_STIE, cpu.core());
                install_machine_exit_handler(cpu, ram);
            },
        },
        {
            "supervisor_timer_interrupt_after_sstatus_write",
            {
                kCsrwSstatusX5,
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                kCsrrcSipX5,
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(5, MSTATUS_SIE);
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_MIDELEG, MIE_STIE, cpu.core());
                cpu.csr().write(CSR_SIE, MIE_STIE, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                cpu.csr().write(CSR_SIP, MIE_STIE, cpu.core());
                install_machine_exit_handler(cpu, ram);
            },
        },
        {
            "supervisor_timer_interrupt_at_cycle_start",
            {
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                kCsrrcSipX5,
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(5, MIE_STIE);
                cpu.core().write_gpr(7, kEntry + 4);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_MIDELEG, MIE_STIE, cpu.core());
                cpu.csr().write(CSR_SIE, MIE_STIE, cpu.core());
                cpu.csr().write(CSR_MSTATUS, MSTATUS_SIE, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                cpu.csr().write(CSR_SIP, MIE_STIE, cpu.core());
                install_machine_exit_handler(cpu, ram);
            },
        },
        {
            "user_mode_supervisor_timer_interrupt_at_cycle_start",
            {
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                kCsrrcSipX5,
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(5, MIE_STIE);
                cpu.core().write_gpr(7, kEntry + 4);
                cpu.core().set_privilege_mode(PrivilegeMode::User);

                cpu.csr().write(CSR_MIDELEG, MIE_STIE, cpu.core());
                cpu.csr().write(CSR_SIE, MIE_STIE, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                cpu.csr().write(CSR_SIP, MIE_STIE, cpu.core());
                install_machine_exit_handler(cpu, ram);
            },
        },
        {
            "user_mode_supervisor_external_interrupt_after_sret",
            {
                kSret,
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                kCsrrcSipX5,
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(5, MIE_SEIE);
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_SEPC, kEntry + 8, cpu.core());
                cpu.csr().write(CSR_MIDELEG, MIE_SEIE, cpu.core());
                cpu.csr().write(CSR_SIE, MIE_SEIE, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                cpu.csr().write(CSR_MIP, MIE_SEIE, cpu.core());
                cpu.csr().write(CSR_MSTATUS, 0, cpu.core());  // SPP=U, SIE=0
                install_machine_exit_handler(cpu, ram);
            },
        },
        {
            "supervisor_external_interrupt_after_sip_write",
            {
                kCsrwSipX5,
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                kCsrrcSipX5,
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(5, MIE_SEIE);
                cpu.core().write_gpr(7, kEntry + 8);
                cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);

                cpu.csr().write(CSR_MIDELEG, MIE_SEIE, cpu.core());
                cpu.csr().write(CSR_SIE, MIE_SEIE, cpu.core());
                cpu.csr().write(CSR_MSTATUS, MSTATUS_SIE, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                install_machine_exit_handler(cpu, ram);
            },
        },
        {
            "user_mode_supervisor_external_interrupt_at_cycle_start",
            {
                kAddiX1WrongPath,
                kEbreak,
            },
            {
                kCsrrcSipX5,
                kCsrwSepcX7,
                kSret,
            },
            {},
            128,
            [](CPU& cpu, Ram& ram, Bus&) {
                cpu.core().write_gpr(5, MIE_SEIE);
                cpu.core().write_gpr(7, kEntry + 4);
                cpu.core().set_privilege_mode(PrivilegeMode::User);

                cpu.csr().write(CSR_MIDELEG, MIE_SEIE, cpu.core());
                cpu.csr().write(CSR_SIE, MIE_SEIE, cpu.core());
                cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
                cpu.csr().write(CSR_SIP, MIE_SEIE, cpu.core());
                install_machine_exit_handler(cpu, ram);
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
