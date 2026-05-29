#include "address_space.h"

#include "../arch/core_state.h"
#include "../arch/csr_file.h"
#include "../trap.h"
#include "bus.h"
#include "simple_l1_cache.h"

namespace {

constexpr uint64_t CAUSE_INSN_ACCESS_FAULT = 1;
constexpr uint64_t CAUSE_LOAD_ACCESS_FAULT = 5;
constexpr uint64_t CAUSE_STORE_ACCESS_FAULT = 7;
constexpr uint64_t CAUSE_INSN_PAGE_FAULT = 12;
constexpr uint64_t CAUSE_LOAD_PAGE_FAULT = 13;
constexpr uint64_t CAUSE_STORE_PAGE_FAULT = 15;

constexpr uint64_t SATP_MODE_SHIFT = 60;
constexpr uint64_t SATP_MODE_MASK = 0xFULL << SATP_MODE_SHIFT;
constexpr uint64_t SATP_MODE_BARE = 0ULL;
constexpr uint64_t SATP_MODE_SV39 = 8ULL;
constexpr uint64_t SATP_PPN_MASK = (1ULL << 44) - 1;
constexpr uint64_t SV39_VADDR_BITS = 39;
constexpr uint64_t SV39_PAGE_SIZE = 4096;
constexpr uint64_t SV39_PAGE_OFFSET_MASK = SV39_PAGE_SIZE - 1;
constexpr uint8_t SV39_PAGE_SHIFT = 12;

constexpr uint64_t PTE_V = 1ULL << 0;
constexpr uint64_t PTE_R = 1ULL << 1;
constexpr uint64_t PTE_W = 1ULL << 2;
constexpr uint64_t PTE_X = 1ULL << 3;
constexpr uint64_t PTE_U = 1ULL << 4;
constexpr uint64_t PTE_A = 1ULL << 6;
constexpr uint64_t PTE_D = 1ULL << 7;
constexpr uint64_t PTE_SV39_RESERVED_MASK = 0xFFC0000000000000ULL;

constexpr int SV39_LEVELS = 3;
constexpr int SV39_PTESIZE = 8;
constexpr uint64_t SV39_VPN_MASK = 0x1FF;
constexpr uint64_t PTE_PPN_MASK = (1ULL << 44) - 1;

uint64_t access_fault_cause(AccessType type) {
    switch (type) {
    case AccessType::Instruction:
        return CAUSE_INSN_ACCESS_FAULT;
    case AccessType::Load:
        return CAUSE_LOAD_ACCESS_FAULT;
    case AccessType::Store:
        return CAUSE_STORE_ACCESS_FAULT;
    }
    return CAUSE_LOAD_ACCESS_FAULT;
}

uint64_t page_fault_cause(AccessType type) {
    switch (type) {
    case AccessType::Instruction:
        return CAUSE_INSN_PAGE_FAULT;
    case AccessType::Load:
        return CAUSE_LOAD_PAGE_FAULT;
    case AccessType::Store:
        return CAUSE_STORE_PAGE_FAULT;
    }
    return CAUSE_LOAD_PAGE_FAULT;
}

TrapRequest make_fault(uint64_t cause, uint64_t tval) {
    TrapRequest fault;
    fault.valid = true;
    fault.cause = cause;
    fault.tval = tval;
    return fault;
}

bool is_sv39_canonical(uint64_t vaddr) {
    const uint64_t sign = (vaddr >> 38) & 1ULL;
    const uint64_t upper = vaddr >> SV39_VADDR_BITS;
    const uint64_t expected_upper = sign ? ((1ULL << (64 - SV39_VADDR_BITS)) - 1ULL) : 0ULL;
    return upper == expected_upper;
}

uint64_t size_mask(int size) {
    if (size >= 8) {
        return ~0ULL;
    }
    return (1ULL << (size * 8)) - 1ULL;
}

const char* access_source(AccessType type) {
    return type == AccessType::Instruction ? "fetch" : "guest-data";
}

const char* access_kind(AccessType type) {
    switch (type) {
    case AccessType::Instruction:
        return "instruction-fetch";
    case AccessType::Load:
        return "data-load";
    case AccessType::Store:
        return "data-store";
    }
    return "unknown";
}

int page_chunk_size(uint64_t addr, int remaining) {
    const int until_boundary = static_cast<int>(SV39_PAGE_SIZE - (addr & SV39_PAGE_OFFSET_MASK));
    return remaining < until_boundary ? remaining : until_boundary;
}

bool can_access_user_page(AccessType type, PrivilegeMode mode, uint64_t mstatus) {
    if (mode == PrivilegeMode::User) {
        return true;
    }
    if (type == AccessType::Instruction) {
        return false;
    }
    return (mstatus & MSTATUS_SUM) != 0;
}

PrivilegeMode decode_mpp(uint64_t mstatus) {
    switch ((mstatus & MSTATUS_MPP_MASK) >> MSTATUS_MPP_SHIFT) {
    case 0:
        return PrivilegeMode::User;
    case 1:
        return PrivilegeMode::Supervisor;
    case 3:
        return PrivilegeMode::Machine;
    default:
        return PrivilegeMode::User;
    }
}

PrivilegeMode effective_privilege_mode(AccessType type, const CoreState& core, uint64_t mstatus) {
    if (type == AccessType::Instruction) {
        return core.privilege_mode();
    }
    if (core.privilege_mode() == PrivilegeMode::Machine && (mstatus & MSTATUS_MPRV)) {
        return decode_mpp(mstatus);
    }
    return core.privilege_mode();
}

}  // namespace

AddressSpace::AddressSpace(CoreState& core, CsrFile& csr, TrapController& trap)
    : core_(core), csr_(csr), trap_(trap) {}

AddressSpace::AccessResult AddressSpace::fetch16_result(Bus& bus) {
    return fetch16_result(bus, core_.pc());
}

AddressSpace::AccessResult AddressSpace::fetch16_result(Bus& bus, uint64_t pc) {
    return access_result(bus, pc, 2, AccessType::Instruction);
}

void AddressSpace::flush_tlb() {
    for (TlbEntry& entry : tlb_) {
        entry.valid = false;
    }
    next_tlb_victim_ = 0;
}

void AddressSpace::bind_l1_data_cache(SimpleL1DataCache* cache) {
    l1_data_cache_ = cache;
}

AddressSpace::AccessResult AddressSpace::fetch32_result(Bus& bus) {
    return fetch32_result(bus, core_.pc());
}

AddressSpace::AccessResult AddressSpace::fetch32_result(Bus& bus, uint64_t pc) {
    return access_result(bus, pc, 4, AccessType::Instruction);
}

AddressSpace::AccessResult AddressSpace::load_result(Bus& bus, uint64_t addr, int size) {
    return access_result(bus, addr, size, AccessType::Load);
}

AddressSpace::TranslateResult AddressSpace::translate_result(Bus& bus,
                                                             uint64_t addr,
                                                             AccessType type,
                                                             bool update_access_bits) {
    TranslateResult result;
    result.ok = translate(bus, addr, type, result.paddr, result.fault, update_access_bits);
    return result;
}

AddressSpace::AccessResult AddressSpace::store_result(Bus& bus, uint64_t addr, uint64_t value, int size) {
    AccessResult result;
    uint64_t current_addr = addr;
    uint64_t remaining_value = value;
    int remaining = size;

    while (remaining > 0) {
        const int chunk = page_chunk_size(current_addr, remaining);
        uint64_t paddr = 0;
        if (!translate(bus, current_addr, AccessType::Store, paddr, result.fault, true)) {
            return result;
        }

        const uint64_t chunk_value = remaining_value & size_mask(chunk);
        if (!try_data_store(bus, paddr, chunk_value, chunk)) {
            result.fault = make_fault(access_fault_cause(AccessType::Store), current_addr);
            return result;
        }

        current_addr += static_cast<uint64_t>(chunk);
        remaining_value >>= chunk * 8;
        remaining -= chunk;
    }

    result.ok = true;
    return result;
}

bool AddressSpace::fetch32(Bus& bus, uint32_t& raw) {
    const AccessResult result = fetch32_result(bus);
    if (!result.ok) {
        apply_fault(result.fault);
        return false;
    }
    raw = static_cast<uint32_t>(result.value);
    return true;
}

bool AddressSpace::fetch16(Bus& bus, uint16_t& raw) {
    const AccessResult result = fetch16_result(bus);
    if (!result.ok) {
        apply_fault(result.fault);
        return false;
    }
    raw = static_cast<uint16_t>(result.value);
    return true;
}

bool AddressSpace::load(Bus& bus, uint64_t addr, int size, uint64_t& value) {
    const AccessResult result = load_result(bus, addr, size);
    if (!result.ok) {
        apply_fault(result.fault);
        return false;
    }
    value = result.value;
    return true;
}

bool AddressSpace::store(Bus& bus, uint64_t addr, uint64_t value, int size) {
    const AccessResult result = store_result(bus, addr, value, size);
    if (!result.ok) {
        apply_fault(result.fault);
        return false;
    }
    return true;
}

AddressSpace::TlbEntry* AddressSpace::lookup_tlb(uint64_t satp, uint64_t vaddr) {
    for (TlbEntry& entry : tlb_) {
        if (!entry.valid || entry.satp != satp) {
            continue;
        }

        const uint64_t page_mask = (1ULL << entry.page_shift) - 1ULL;
        const uint64_t vpage_base = vaddr & ~page_mask;
        if (vpage_base != entry.vpage_base) {
            continue;
        }

        return &entry;
    }

    return nullptr;
}

void AddressSpace::fill_tlb(
    uint64_t satp, uint64_t vaddr, uint64_t paddr, uint64_t pte, uint64_t pte_addr, uint8_t page_shift) {
    const uint64_t page_mask = (1ULL << page_shift) - 1ULL;
    TlbEntry& entry = tlb_[next_tlb_victim_];
    entry.valid = true;
    entry.satp = satp;
    entry.vpage_base = vaddr & ~page_mask;
    entry.ppage_base = paddr & ~page_mask;
    entry.pte = pte;
    entry.pte_addr = pte_addr;
    entry.page_shift = page_shift;
    next_tlb_victim_ = (next_tlb_victim_ + 1) % tlb_.size();
}

bool AddressSpace::check_leaf_permissions(
    uint64_t pte,
    AccessType type,
    PrivilegeMode effective_mode,
    uint64_t vaddr,
    TrapRequest& fault) {
    const bool readable = pte & PTE_R;
    const bool writable = pte & PTE_W;
    const bool executable = pte & PTE_X;
    const bool user_accessible = pte & PTE_U;
    const uint64_t mstatus = csr_.read(CSR_MSTATUS, core_);

    if (writable && !readable) {
        fault = make_fault(page_fault_cause(type), vaddr);
        return false;
    }

    const bool is_user_mode = effective_mode == PrivilegeMode::User;
    if (is_user_mode && !user_accessible) {
        fault = make_fault(page_fault_cause(type), vaddr);
        return false;
    }
    if (!is_user_mode && user_accessible && !can_access_user_page(type, effective_mode, mstatus)) {
        fault = make_fault(page_fault_cause(type), vaddr);
        return false;
    }

    switch (type) {
    case AccessType::Instruction:
        if (!executable) {
            fault = make_fault(page_fault_cause(type), vaddr);
            return false;
        }
        break;
    case AccessType::Load:
        if (!readable && !((mstatus & MSTATUS_MXR) && executable)) {
            fault = make_fault(page_fault_cause(type), vaddr);
            return false;
        }
        break;
    case AccessType::Store:
        if (!writable) {
            fault = make_fault(page_fault_cause(type), vaddr);
            return false;
        }
        break;
    }

    return true;
}

bool AddressSpace::update_pte_access_bits(
    Bus& bus, uint64_t pte_addr, uint64_t& pte, AccessType type, uint64_t vaddr, TrapRequest& fault) {
    uint64_t updated_pte = pte | PTE_A;
    if (type == AccessType::Store) {
        updated_pte |= PTE_D;
    }

    if (updated_pte == pte) {
        return true;
    }

    if (!bus.try_store_observed(pte_addr, updated_pte, SV39_PTESIZE, "page-walk", "pte-ad-update")) {
        fault = make_fault(access_fault_cause(type), vaddr);
        return false;
    }

    pte = updated_pte;
    return true;
}

bool AddressSpace::translate(Bus& bus,
                             uint64_t vaddr,
                             AccessType type,
                             uint64_t& paddr,
                             TrapRequest& fault,
                             bool update_access_bits) {
    const uint64_t satp = csr_.read(CSR_SATP, core_);
    const uint64_t mode = (satp & SATP_MODE_MASK) >> SATP_MODE_SHIFT;
    const uint64_t mstatus = csr_.read(CSR_MSTATUS, core_);
    const PrivilegeMode effective_mode = effective_privilege_mode(type, core_, mstatus);

    if (mode == SATP_MODE_BARE || effective_mode == PrivilegeMode::Machine) {
        paddr = vaddr;
        return true;
    }

    if (mode != SATP_MODE_SV39) {
        fault = make_fault(page_fault_cause(type), vaddr);
        return false;
    }

    if (!is_sv39_canonical(vaddr)) {
        fault = make_fault(page_fault_cause(type), vaddr);
        return false;
    }

    if (TlbEntry* entry = lookup_tlb(satp, vaddr)) {
        const uint64_t page_mask = (1ULL << entry->page_shift) - 1ULL;
        paddr = entry->ppage_base | (vaddr & page_mask);
        if (!check_leaf_permissions(entry->pte, type, effective_mode, vaddr, fault)) {
            return false;
        }
        if (!update_access_bits) {
            return true;
        }
        return update_pte_access_bits(bus, entry->pte_addr, entry->pte, type, vaddr, fault);
    }

    return walk_page_table(bus, vaddr, type, paddr, fault, update_access_bits);
}

AddressSpace::AccessResult AddressSpace::access_result(Bus& bus, uint64_t vaddr, int size, AccessType type) {
    AccessResult result;
    uint64_t current_addr = vaddr;
    int remaining = size;
    int shift = 0;

    while (remaining > 0) {
        const int chunk = page_chunk_size(current_addr, remaining);
        uint64_t paddr = 0;
        if (!translate(bus, current_addr, type, paddr, result.fault, true)) {
            return result;
        }

        uint64_t chunk_value = 0;
        const bool loaded =
            type == AccessType::Load
                ? try_data_load(bus, paddr, chunk, chunk_value)
                : bus.try_load_observed(paddr,
                                        chunk,
                                        chunk_value,
                                        access_source(type),
                                        access_kind(type));
        if (!loaded) {
            result.fault = make_fault(access_fault_cause(type), current_addr);
            return result;
        }

        result.value |= (chunk_value & size_mask(chunk)) << shift;
        current_addr += static_cast<uint64_t>(chunk);
        remaining -= chunk;
        shift += chunk * 8;
    }
    result.ok = true;
    return result;
}

bool AddressSpace::try_data_load(Bus& bus, uint64_t paddr, int size, uint64_t& value) {
    if (l1_data_cache_ != nullptr && l1_data_cache_->enabled()) {
        return l1_data_cache_->load(bus, paddr, size, value);
    }
    return bus.try_load_observed(paddr, size, value, "guest-data", "data-load");
}

bool AddressSpace::try_data_store(Bus& bus, uint64_t paddr, uint64_t value, int size) {
    if (l1_data_cache_ != nullptr && l1_data_cache_->enabled()) {
        return l1_data_cache_->store(bus, paddr, value, size);
    }
    return bus.try_store_observed(paddr, value, size, "guest-data", "data-store");
}

void AddressSpace::apply_fault(const TrapRequest& fault) {
    if (fault.valid) {
        trap_.enter_exception(fault.cause, fault.tval);
    }
}

bool AddressSpace::walk_page_table(Bus& bus,
                                   uint64_t vaddr,
                                   AccessType type,
                                   uint64_t& paddr,
                                   TrapRequest& fault,
                                   bool update_access_bits) {
    const uint64_t satp = csr_.read(CSR_SATP, core_);
    const uint64_t mstatus = csr_.read(CSR_MSTATUS, core_);
    const PrivilegeMode effective_mode = effective_privilege_mode(type, core_, mstatus);
    uint64_t ppn = satp & SATP_PPN_MASK;

    const uint64_t vpn[3] = {
        (vaddr >> 12) & SV39_VPN_MASK,
        (vaddr >> 21) & SV39_VPN_MASK,
        (vaddr >> 30) & SV39_VPN_MASK,
    };

    for (int level = SV39_LEVELS - 1; level >= 0; --level) {
        const uint64_t pte_addr = (ppn << 12) | (vpn[level] << 3);
        uint64_t pte = 0;

        // Page table walk uses physical addresses directly, bypassing translation
        if (!bus.try_load_observed(pte_addr, SV39_PTESIZE, pte, "page-walk", "pte-read")) {
            fault = make_fault(access_fault_cause(type), vaddr);
            return false;
        }

        if (!(pte & PTE_V)) {
            fault = make_fault(page_fault_cause(type), vaddr);
            return false;
        }

        if (pte & PTE_SV39_RESERVED_MASK) {
            fault = make_fault(page_fault_cause(type), vaddr);
            return false;
        }

        const bool is_leaf = (pte & (PTE_R | PTE_W | PTE_X)) != 0;

        if (is_leaf) {
            if (!check_leaf_permissions(pte, type, effective_mode, vaddr, fault)) {
                return false;
            }

            const uint64_t pte_ppn = (pte >> 10) & PTE_PPN_MASK;
            const uint64_t page_offset = vaddr & ((1ULL << (12 + level * 9)) - 1);

            // For 4KB pages (level 0), no misalignment check needed
            // For 2MB pages (level 1), PPN[0] must be 0
            // For 1GB pages (level 2), PPN[1:0] must be 0
            if (level == 1) {
                if ((pte_ppn & SV39_VPN_MASK) != 0) {
                    fault = make_fault(page_fault_cause(type), vaddr);
                    return false;
                }
            } else if (level == 2) {
                if ((pte_ppn & ((1ULL << 18) - 1)) != 0) {
                    fault = make_fault(page_fault_cause(type), vaddr);
                    return false;
                }
            }

            uint64_t updated_pte = pte;
            if (update_access_bits &&
                !update_pte_access_bits(bus, pte_addr, updated_pte, type, vaddr, fault)) {
                return false;
            }

            paddr = (pte_ppn << 12) | page_offset;
            fill_tlb(satp, vaddr, paddr, updated_pte, pte_addr, static_cast<uint8_t>(SV39_PAGE_SHIFT + level * 9));
            return true;
        }

        // For non-leaf PTEs, U/A/D are reserved in the current Sv39 model and
        // must not be accepted as valid next-level pointers.
        if (pte & (PTE_U | PTE_A | PTE_D)) {
            fault = make_fault(page_fault_cause(type), vaddr);
            return false;
        }

        ppn = (pte >> 10) & PTE_PPN_MASK;
    }

    fault = make_fault(page_fault_cause(type), vaddr);
    return false;
}
