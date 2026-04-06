#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "../../../src/arch/csr_file.h"
#include "../../../src/cpu.h"
#include "../../../src/mem/bus.h"
#include "../../../src/mem/ram.h"

namespace spike_differential {

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
constexpr uint32_t kAddiX1Inc = 0x00108093U;              // addi x1, x1, 1
constexpr uint32_t kAddiX2WrongPath = 0x06300113U;        // addi x2, x0, 99
constexpr uint32_t kAddiX2FromX0Plus5 = 0x00500113U;      // addi x2, x0, 5
constexpr uint32_t kAddiX2FromX0Plus7 = 0x00700113U;      // addi x2, x0, 7
constexpr uint32_t kAddiX3WrongPath = 0x06300193U;        // addi x3, x0, 99
constexpr uint32_t kAddiX4WrongPath = 0x06300213U;        // addi x4, x0, 99
constexpr uint32_t kAddiX8FromX0Plus11 = 0x00b00413U;     // addi x8, x0, 11
constexpr uint32_t kJalX5Skip = 0x008002efU;              // jal x5, 8
constexpr uint32_t kBeqX0Taken = 0x00000463U;             // beq x0, x0, 8
constexpr uint32_t kAuipcX6 = 0x00000317U;                // auipc x6, 0
constexpr uint32_t kAddiX6Target16 = 0x01030313U;         // addi x6, x6, 16
constexpr uint32_t kJalrX7X6 = 0x000303e7U;               // jalr x7, x6, 0
constexpr uint32_t kLwX1FromX10 = 0x00052083U;            // lw x1, 0(x10)
constexpr uint32_t kSwX11ToX10 = 0x00b52023U;             // sw x11, 0(x10)
constexpr uint32_t kAddiA7Exit = 0x05d00893U;             // addi a7, x0, 93
constexpr uint32_t kEcall = 0x00000073U;                  // ecall
constexpr uint32_t kCsrwMepcX6 = 0x34131073U;             // csrw mepc, x6
constexpr uint32_t kMret = 0x30200073U;                   // mret
constexpr uint32_t kCsrwSipX5 = 0x14429073U;              // csrw sip, x5
constexpr uint32_t kCsrrcSipX5 = 0x1442b073U;             // csrrc x0, sip, x5
constexpr uint32_t kCsrwSepcX7 = 0x14139073U;             // csrw sepc, x7
constexpr uint32_t kAddiX5FromX0Plus256 = 0x10000293U;    // addi x5, x0, 256
constexpr uint32_t kCsrwSieX5 = 0x10429073U;              // csrw sie, x5
constexpr uint32_t kCsrwSstatusX5 = 0x10029073U;          // csrw sstatus, x5
constexpr uint32_t kSret = 0x10200073U;                   // sret
constexpr uint32_t kBltX1X2Loop = 0xfe20cee3U;            // blt x1, x2, -4

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

struct MemoryInit {
    uint64_t addr{0};
    uint64_t value{0};
    int size{0};
};

struct Scenario {
    enum class PlatformFixture : uint8_t {
        None,
        UartPlic,
    };

    const char* name{nullptr};
    std::vector<uint32_t> program{};
    std::vector<uint32_t> trap_program{};
    std::vector<MemoryWatch> watches{};
    int max_steps{64};
    std::function<void(CPU&, Ram&, Bus&)> configure{};
    PlatformFixture fixture{PlatformFixture::None};
    std::array<uint64_t, 32> initial_gprs{};
    std::array<uint64_t, kTrackedCsrs.size()> initial_csrs{};
    std::vector<MemoryInit> initial_memory{};
    PrivilegeMode initial_privilege{PrivilegeMode::Machine};
};

inline size_t tracked_csr_index(uint32_t csr) {
    for (size_t i = 0; i < kTrackedCsrs.size(); ++i) {
        if (kTrackedCsrs[i] == csr) {
            return i;
        }
    }
    return kTrackedCsrs.size();
}

inline void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

inline void write64(Ram& ram, uint64_t addr, uint64_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

inline void apply_initial_state(CPU& cpu, Ram& ram, const Scenario& scenario) {
    for (size_t i = 0; i < scenario.initial_gprs.size(); ++i) {
        if (scenario.initial_gprs[i] != 0) {
            cpu.core().write_gpr(static_cast<uint32_t>(i), scenario.initial_gprs[i]);
        }
    }
    for (size_t i = 0; i < scenario.initial_csrs.size(); ++i) {
        if (scenario.initial_csrs[i] != 0) {
            cpu.csr().write(kTrackedCsrs[i], scenario.initial_csrs[i], cpu.core());
        }
    }
    cpu.core().set_privilege_mode(scenario.initial_privilege);
    for (const MemoryInit& init : scenario.initial_memory) {
        ram.store(init.addr, init.value, init.size);
    }
}

inline const char* csr_name(uint32_t csr) {
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
        return "csr";
    }
}

}  // namespace spike_differential
