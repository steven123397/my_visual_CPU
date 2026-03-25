#include <cstdio>

#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t SATP_MODE_SV39 = 8ULL << 60;

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

    return 0;
}
