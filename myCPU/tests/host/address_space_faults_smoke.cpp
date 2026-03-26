#include <cstdio>

#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t SATP_MODE_SV39 = 8ULL << 60;
constexpr uint64_t ROOT_PAGE_TABLE = 0x80100000ULL;
constexpr uint64_t LEVEL1_PAGE_TABLE = 0x80101000ULL;
constexpr uint64_t LEVEL0_PAGE_TABLE = 0x80102000ULL;
constexpr uint64_t USER_EXEC_BACKING_PAGE = 0x80103000ULL;
constexpr uint64_t USER_EXEC_VADDR = 0x80001000ULL;

void write64(Ram& ram, uint64_t addr, uint64_t value) {
    ram.store(addr, value, 8);
}

void setup_sv39_user_exec_page(Ram& ram) {
    write64(ram, ROOT_PAGE_TABLE + 16, ((LEVEL1_PAGE_TABLE >> 12) << 10) | 0x1ULL);
    write64(ram, LEVEL1_PAGE_TABLE + 0, ((LEVEL0_PAGE_TABLE >> 12) << 10) | 0x1ULL);
    write64(ram, LEVEL0_PAGE_TABLE + 8, ((USER_EXEC_BACKING_PAGE >> 12) << 10) | 0x19ULL);
    ram.store(USER_EXEC_BACKING_PAGE, 0x00000013U, 4);
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, MEM_BASE);

    AddressSpace::AccessResult access_fault = cpu.address_space().load_result(bus, 0x1000, 4);
    if (!expect(!access_fault.ok, "unmapped bare-mode load should fail")) {
        return 1;
    }
    if (!expect(access_fault.fault.valid, "result API should surface load access fault")) {
        return 1;
    }
    if (!expect(access_fault.fault.cause == 5, "bare-mode load fault cause should be load access fault")) {
        return 1;
    }
    if (!expect(access_fault.fault.tval == 0x1000, "bare-mode load fault tval should be the faulting address")) {
        return 1;
    }
    if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 0, "result API should not write trap CSRs")) {
        return 1;
    }
    if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == 0, "result API should leave mtval untouched")) {
        return 1;
    }

    uint64_t value = 0;
    if (!expect(!cpu.address_space().load(bus, 0x1000, 4, value), "legacy load wrapper should still report failure")) {
        return 1;
    }
    if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 5, "legacy load wrapper should still enter a trap")) {
        return 1;
    }
    if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == 0x1000, "legacy load wrapper should still report mtval")) {
        return 1;
    }

    cpu_init(cpu, MEM_BASE);
    cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
    cpu.csr().write(CSR_SATP, SATP_MODE_SV39);

    AddressSpace::AccessResult page_fault = cpu.address_space().load_result(bus, 1ULL << 39, 4);
    if (!expect(!page_fault.ok, "non-canonical Sv39 load should fail")) {
        return 1;
    }
    if (!expect(page_fault.fault.valid, "Sv39 page fault should be returned as a fault result")) {
        return 1;
    }
    if (!expect(page_fault.fault.cause == 13, "Sv39 load fault cause should be load page fault")) {
        return 1;
    }
    if (!expect(page_fault.fault.tval == (1ULL << 39), "Sv39 page fault tval should preserve the virtual address")) {
        return 1;
    }
    if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 0, "result API should not trap on Sv39 page faults")) {
        return 1;
    }

    cpu_init(cpu, MEM_BASE);
    setup_sv39_user_exec_page(ram);
    cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
    cpu.csr().write(CSR_SATP, SATP_MODE_SV39 | (ROOT_PAGE_TABLE >> 12), cpu.core());
    cpu.address_space().flush_tlb();

    AddressSpace::AccessResult exec_page_fault =
        cpu.address_space().fetch32_result(bus, USER_EXEC_VADDR);
    if (!expect(!exec_page_fault.ok,
                "supervisor fetch from user executable page should fail")) {
        return 1;
    }
    if (!expect(exec_page_fault.fault.valid,
                "result API should surface instruction page faults")) {
        return 1;
    }
    if (!expect(exec_page_fault.fault.cause == 12,
                "Sv39 fetch fault cause should be instruction page fault")) {
        return 1;
    }
    if (!expect(exec_page_fault.fault.tval == USER_EXEC_VADDR,
                "Sv39 fetch fault tval should preserve the virtual address")) {
        return 1;
    }
    if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 0,
                "fetch result API should not trap on instruction page faults")) {
        return 1;
    }
    if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == 0,
                "fetch result API should leave mtval untouched")) {
        return 1;
    }

    cpu.core().set_pc(USER_EXEC_VADDR);
    uint32_t raw = 0;
    if (!expect(!cpu.address_space().fetch32(bus, raw),
                "legacy fetch wrapper should still report failure")) {
        return 1;
    }
    if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 12,
                "legacy fetch wrapper should still enter instruction page fault")) {
        return 1;
    }
    if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == USER_EXEC_VADDR,
                "legacy fetch wrapper should still report faulting virtual address")) {
        return 1;
    }

    return 0;
}
