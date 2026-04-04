#include <cstdio>
#include <stdexcept>
#include <unordered_map>

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
constexpr uint64_t DATA_BACKING_PAGE_A = 0x80104000ULL;
constexpr uint64_t DATA_BACKING_PAGE_B = 0x80105000ULL;
constexpr uint64_t DATA_VADDR = 0x80002000ULL;
constexpr uint64_t UNMAPPED_PAGE_TABLE_ROOT = 0x90000000ULL;
constexpr uint64_t PAGE_WALK_BUS_FAULT_VADDR = 0x80006000ULL;
constexpr uint64_t READONLY_PAGE_TABLE_BASE = 0x90001000ULL;
constexpr uint64_t READONLY_LEVEL1_PAGE_TABLE = 0x90002000ULL;
constexpr uint64_t READONLY_LEVEL0_PAGE_TABLE = 0x90003000ULL;
constexpr uint64_t READONLY_LEAF_BACKING_PAGE = 0x80106000ULL;
constexpr uint64_t AD_UPDATE_BUS_FAULT_VADDR = 0x80007000ULL;

void write64(Ram& ram, uint64_t addr, uint64_t value) {
    ram.store(addr, value, 8);
}

void setup_sv39_user_exec_page(Ram& ram) {
    write64(ram, ROOT_PAGE_TABLE + 16, ((LEVEL1_PAGE_TABLE >> 12) << 10) | 0x1ULL);
    write64(ram, LEVEL1_PAGE_TABLE + 0, ((LEVEL0_PAGE_TABLE >> 12) << 10) | 0x1ULL);
    write64(ram, LEVEL0_PAGE_TABLE + 8, ((USER_EXEC_BACKING_PAGE >> 12) << 10) | 0x19ULL);
    ram.store(USER_EXEC_BACKING_PAGE, 0x00000013U, 4);
}

void map_sv39_data_page(Ram& ram, uint64_t backing_page) {
    write64(ram, ROOT_PAGE_TABLE + 16, ((LEVEL1_PAGE_TABLE >> 12) << 10) | 0x1ULL);
    write64(ram, LEVEL1_PAGE_TABLE + 0, ((LEVEL0_PAGE_TABLE >> 12) << 10) | 0x1ULL);
    write64(ram, LEVEL0_PAGE_TABLE + 16, ((backing_page >> 12) << 10) | 0x7ULL);
}

class ReadOnlyPageTableDevice : public Device {
public:
    ReadOnlyPageTableDevice(uint64_t base, uint64_t size)
        : Device(base, size) {}

    uint64_t load(uint64_t addr, int size) override {
        if (size != 8) {
            invalid_access(addr, size);
        }

        const auto it = ptes_.find(addr);
        if (it == ptes_.end()) {
            return 0;
        }
        return it->second;
    }

    void store(uint64_t, uint64_t, int) override {
        throw std::runtime_error("read-only page table");
    }

    const char* debug_name() const override {
        return "readonly_page_table";
    }

    bool is_mmio() const override {
        return false;
    }

    void set_pte(uint64_t addr, uint64_t pte) {
        ptes_[addr] = pte;
    }

private:
    std::unordered_map<uint64_t, uint64_t> ptes_;
};

uint64_t sv39_vpn(uint64_t vaddr, int level) {
    return (vaddr >> (12 + level * 9)) & 0x1FFULL;
}

void map_readonly_leaf(ReadOnlyPageTableDevice& device, uint64_t vaddr) {
    const uint64_t root_entry_addr = READONLY_PAGE_TABLE_BASE + sv39_vpn(vaddr, 2) * 8;
    const uint64_t level1_entry_addr = READONLY_LEVEL1_PAGE_TABLE + sv39_vpn(vaddr, 1) * 8;
    const uint64_t level0_entry_addr = READONLY_LEVEL0_PAGE_TABLE + sv39_vpn(vaddr, 0) * 8;

    const uint64_t root_pte = ((READONLY_LEVEL1_PAGE_TABLE >> 12) << 10) | 0x1ULL;
    const uint64_t level1_pte = ((READONLY_LEVEL0_PAGE_TABLE >> 12) << 10) | 0x1ULL;
    const uint64_t leaf_pte = ((READONLY_LEAF_BACKING_PAGE >> 12) << 10) | 0xFULL;

    device.set_pte(root_entry_addr, root_pte);
    device.set_pte(level1_entry_addr, level1_pte);
    device.set_pte(level0_entry_addr, leaf_pte);
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

    cpu_init(cpu, MEM_BASE);
    map_sv39_data_page(ram, DATA_BACKING_PAGE_A);
    ram.store(DATA_BACKING_PAGE_A, UINT32_C(0x11111111), 4);
    ram.store(DATA_BACKING_PAGE_B, UINT32_C(0x22222222), 4);
    cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
    const uint64_t satp_value = SATP_MODE_SV39 | (ROOT_PAGE_TABLE >> 12);
    cpu.csr().write(CSR_SATP, satp_value, cpu.core());

    AddressSpace::AccessResult first_data = cpu.address_space().load_result(bus, DATA_VADDR, 4);
    if (!expect(first_data.ok, "initial Sv39 data load should succeed")) {
        return 1;
    }
    if (!expect(first_data.value == UINT32_C(0x11111111),
                "initial Sv39 data load should observe the first mapping")) {
        return 1;
    }

    map_sv39_data_page(ram, DATA_BACKING_PAGE_B);
    AddressSpace::AccessResult stale_data = cpu.address_space().load_result(bus, DATA_VADDR, 4);
    if (!expect(stale_data.ok, "stale Sv39 TLB load should still succeed before refresh")) {
        return 1;
    }
    if (!expect(stale_data.value == UINT32_C(0x11111111),
                "rewriting the page table without a refresh should keep the stale translation")) {
        return 1;
    }

    cpu.csr().write(CSR_SATP, satp_value, cpu.core());
    AddressSpace::AccessResult refreshed_data = cpu.address_space().load_result(bus, DATA_VADDR, 4);
    if (!expect(refreshed_data.ok, "SATP rewrite should keep Sv39 translation valid")) {
        return 1;
    }
    if (!expect(refreshed_data.value == UINT32_C(0x22222222),
                "rewriting SATP should refresh the local TLB view of the same address space")) {
        return 1;
    }

    cpu_init(cpu, MEM_BASE);
    cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
    cpu.csr().write(CSR_SATP, SATP_MODE_SV39 | (UNMAPPED_PAGE_TABLE_ROOT >> 12), cpu.core());
    cpu.address_space().flush_tlb();

    AddressSpace::AccessResult walk_fetch_fault =
        cpu.address_space().fetch32_result(bus, PAGE_WALK_BUS_FAULT_VADDR);
    if (!expect(!walk_fetch_fault.ok, "page-walk fetch with unmapped PTE should fail")) {
        return 1;
    }
    if (!expect(walk_fetch_fault.fault.valid, "page-walk fetch fault should be surfaced")) {
        return 1;
    }
    if (!expect(walk_fetch_fault.fault.cause == 1,
                "page-walk PTE load failure should surface instruction access fault")) {
        return 1;
    }
    if (!expect(walk_fetch_fault.fault.tval == PAGE_WALK_BUS_FAULT_VADDR,
                "page-walk fetch bus fault should report faulting virtual address")) {
        return 1;
    }

    AddressSpace::AccessResult walk_load_fault =
        cpu.address_space().load_result(bus, PAGE_WALK_BUS_FAULT_VADDR, 4);
    if (!expect(!walk_load_fault.ok, "page-walk load with unmapped PTE should fail")) {
        return 1;
    }
    if (!expect(walk_load_fault.fault.valid, "page-walk load fault should be surfaced")) {
        return 1;
    }
    if (!expect(walk_load_fault.fault.cause == 5,
                "page-walk PTE load failure should surface load access fault")) {
        return 1;
    }
    if (!expect(walk_load_fault.fault.tval == PAGE_WALK_BUS_FAULT_VADDR,
                "page-walk load bus fault should report faulting virtual address")) {
        return 1;
    }

    AddressSpace::AccessResult walk_store_fault =
        cpu.address_space().store_result(bus, PAGE_WALK_BUS_FAULT_VADDR, UINT64_C(0x55), 1);
    if (!expect(!walk_store_fault.ok, "page-walk store with unmapped PTE should fail")) {
        return 1;
    }
    if (!expect(walk_store_fault.fault.valid, "page-walk store fault should be surfaced")) {
        return 1;
    }
    if (!expect(walk_store_fault.fault.cause == 7,
                "page-walk PTE load failure should surface store access fault")) {
        return 1;
    }
    if (!expect(walk_store_fault.fault.tval == PAGE_WALK_BUS_FAULT_VADDR,
                "page-walk store bus fault should report faulting virtual address")) {
        return 1;
    }

    ReadOnlyPageTableDevice readonly_page_table(READONLY_PAGE_TABLE_BASE, 0x3000);
    bus.attach(readonly_page_table);
    map_readonly_leaf(readonly_page_table, AD_UPDATE_BUS_FAULT_VADDR);

    cpu_init(cpu, MEM_BASE);
    cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
    cpu.csr().write(CSR_SATP, SATP_MODE_SV39 | (READONLY_PAGE_TABLE_BASE >> 12), cpu.core());
    cpu.address_space().flush_tlb();

    AddressSpace::AccessResult ad_fetch_fault =
        cpu.address_space().fetch32_result(bus, AD_UPDATE_BUS_FAULT_VADDR);
    if (!expect(!ad_fetch_fault.ok, "A-bit update failure for fetch should fail")) {
        return 1;
    }
    if (!expect(ad_fetch_fault.fault.valid, "A-bit update fetch fault should be surfaced")) {
        return 1;
    }
    if (!expect(ad_fetch_fault.fault.cause == 1,
                "A-bit update store failure should surface instruction access fault")) {
        return 1;
    }
    if (!expect(ad_fetch_fault.fault.tval == AD_UPDATE_BUS_FAULT_VADDR,
                "A-bit update fetch failure should report faulting virtual address")) {
        return 1;
    }

    AddressSpace::AccessResult ad_load_fault =
        cpu.address_space().load_result(bus, AD_UPDATE_BUS_FAULT_VADDR, 4);
    if (!expect(!ad_load_fault.ok, "A-bit update failure for load should fail")) {
        return 1;
    }
    if (!expect(ad_load_fault.fault.valid, "A-bit update load fault should be surfaced")) {
        return 1;
    }
    if (!expect(ad_load_fault.fault.cause == 5,
                "A-bit update store failure should surface load access fault")) {
        return 1;
    }
    if (!expect(ad_load_fault.fault.tval == AD_UPDATE_BUS_FAULT_VADDR,
                "A-bit update load failure should report faulting virtual address")) {
        return 1;
    }

    AddressSpace::AccessResult ad_store_fault =
        cpu.address_space().store_result(bus, AD_UPDATE_BUS_FAULT_VADDR, UINT64_C(0xAA), 1);
    if (!expect(!ad_store_fault.ok, "D-bit update failure for store should fail")) {
        return 1;
    }
    if (!expect(ad_store_fault.fault.valid, "D-bit update store fault should be surfaced")) {
        return 1;
    }
    if (!expect(ad_store_fault.fault.cause == 7,
                "D-bit update store failure should surface store access fault")) {
        return 1;
    }
    if (!expect(ad_store_fault.fault.tval == AD_UPDATE_BUS_FAULT_VADDR,
                "D-bit update failure should report faulting virtual address")) {
        return 1;
    }

    return 0;
}
