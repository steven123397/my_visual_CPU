#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../isa/effects.h"

class Bus;
class CoreState;
class CsrFile;
class TrapController;

enum class AccessType : uint8_t {
    Instruction,
    Load,
    Store,
};

class AddressSpace {
public:
    struct AccessResult {
        bool ok{false};
        uint64_t value{0};
        TrapRequest fault{};
    };

    AddressSpace(CoreState& core, CsrFile& csr, TrapController& trap);

    AccessResult fetch32_result(Bus& bus);
    AccessResult fetch32_result(Bus& bus, uint64_t pc);
    AccessResult load_result(Bus& bus, uint64_t addr, int size);
    AccessResult store_result(Bus& bus, uint64_t addr, uint64_t value, int size);
    bool fetch32(Bus& bus, uint32_t& raw);
    bool load(Bus& bus, uint64_t addr, int size, uint64_t& value);
    bool store(Bus& bus, uint64_t addr, uint64_t value, int size);
    void flush_tlb();

private:
    struct TlbEntry {
        bool valid{false};
        uint64_t satp{0};
        uint64_t vpage_base{0};
        uint64_t ppage_base{0};
        uint64_t pte{0};
        uint64_t pte_addr{0};
        uint8_t page_shift{0};
    };

    TlbEntry* lookup_tlb(uint64_t satp, uint64_t vaddr);
    void fill_tlb(uint64_t satp, uint64_t vaddr, uint64_t paddr, uint64_t pte, uint64_t pte_addr, uint8_t page_shift);
    bool check_leaf_permissions(uint64_t pte, AccessType type, uint64_t vaddr, TrapRequest& fault);
    bool update_pte_access_bits(Bus& bus, uint64_t pte_addr, uint64_t& pte, AccessType type, uint64_t vaddr, TrapRequest& fault);
    bool translate(Bus& bus, uint64_t vaddr, AccessType type, uint64_t& paddr, TrapRequest& fault);
    AccessResult access_result(Bus& bus, uint64_t vaddr, int size, AccessType type);
    void apply_fault(const TrapRequest& fault);
    bool walk_page_table(Bus& bus, uint64_t vaddr, AccessType type, uint64_t& paddr, TrapRequest& fault);

    CoreState& core_;
    CsrFile& csr_;
    TrapController& trap_;
    std::array<TlbEntry, 32> tlb_{};
    size_t next_tlb_victim_{0};
};
