#include "address_space.h"

#include "../arch/core_state.h"
#include "../arch/csr_file.h"
#include "../trap.h"
#include "bus.h"

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

constexpr uint64_t PTE_V = 1ULL << 0;
constexpr uint64_t PTE_R = 1ULL << 1;
constexpr uint64_t PTE_W = 1ULL << 2;
constexpr uint64_t PTE_X = 1ULL << 3;
constexpr uint64_t PTE_U = 1ULL << 4;
constexpr uint64_t PTE_A = 1ULL << 6;
constexpr uint64_t PTE_D = 1ULL << 7;

constexpr int SV39_LEVELS = 3;
constexpr int SV39_PTESIZE = 8;
constexpr uint64_t SV39_VPN_MASK = 0x1FF;

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

}  // namespace

AddressSpace::AddressSpace(CoreState& core, CsrFile& csr, TrapController& trap)
    : core_(core), csr_(csr), trap_(trap) {}

bool AddressSpace::fetch32(Bus& bus, uint32_t& raw) {
    uint64_t value = 0;
    if (!access(bus, core_.pc(), 4, AccessType::Instruction, value)) {
        return false;
    }
    raw = static_cast<uint32_t>(value);
    return true;
}

bool AddressSpace::load(Bus& bus, uint64_t addr, int size, uint64_t& value) {
    return access(bus, addr, size, AccessType::Load, value);
}

bool AddressSpace::store(Bus& bus, uint64_t addr, uint64_t value, int size) {
    uint64_t paddr = 0;
    const uint64_t satp = csr_.read(0x180, core_);
    const uint64_t mode = (satp & SATP_MODE_MASK) >> SATP_MODE_SHIFT;

    if (mode == SATP_MODE_BARE || core_.privilege_mode() == PrivilegeMode::Machine) {
        paddr = addr;
    } else if (mode == SATP_MODE_SV39) {
        if (!walk_page_table(bus, addr, AccessType::Store, paddr)) {
            return false;
        }
    } else {
        paddr = addr;
    }

    if (!bus.try_store(paddr, value, size)) {
        raise_access_fault(AccessType::Store, addr);
        return false;
    }
    return true;
}

bool AddressSpace::translate(uint64_t vaddr, AccessType /*type*/, uint64_t& paddr) {
    const uint64_t satp = csr_.read(0x180, core_);
    const uint64_t mode = (satp & SATP_MODE_MASK) >> SATP_MODE_SHIFT;

    if (mode == SATP_MODE_BARE || core_.privilege_mode() == PrivilegeMode::Machine) {
        paddr = vaddr;
        return true;
    }

    if (mode != SATP_MODE_SV39) {
        paddr = vaddr;
        return true;
    }

    return true;
}

bool AddressSpace::access(Bus& bus, uint64_t vaddr, int size, AccessType type, uint64_t& value) {
    uint64_t paddr = 0;
    const uint64_t satp = csr_.read(0x180, core_);
    const uint64_t mode = (satp & SATP_MODE_MASK) >> SATP_MODE_SHIFT;

    if (mode == SATP_MODE_BARE || core_.privilege_mode() == PrivilegeMode::Machine) {
        paddr = vaddr;
    } else if (mode == SATP_MODE_SV39) {
        if (!walk_page_table(bus, vaddr, type, paddr)) {
            return false;
        }
    } else {
        paddr = vaddr;
    }

    if (!bus.try_load(paddr, size, value)) {
        raise_access_fault(type, vaddr);
        return false;
    }
    return true;
}

void AddressSpace::raise_access_fault(AccessType type, uint64_t addr) {
    trap_.enter_exception(access_fault_cause(type), addr);
}

void AddressSpace::raise_page_fault(AccessType type, uint64_t addr) {
    trap_.enter_exception(page_fault_cause(type), addr);
}

bool AddressSpace::walk_page_table(Bus& bus, uint64_t vaddr, AccessType type, uint64_t& paddr) {
    const uint64_t satp = csr_.read(0x180, core_);
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
        if (!bus.try_load(pte_addr, SV39_PTESIZE, pte)) {
            raise_page_fault(type, vaddr);
            return false;
        }

        if (!(pte & PTE_V)) {
            raise_page_fault(type, vaddr);
            return false;
        }

        const bool is_leaf = (pte & (PTE_R | PTE_W | PTE_X)) != 0;

        if (is_leaf) {
            const bool readable = pte & PTE_R;
            const bool writable = pte & PTE_W;
            const bool executable = pte & PTE_X;
            const bool user_accessible = pte & PTE_U;

            if (writable && !readable) {
                raise_page_fault(type, vaddr);
                return false;
            }

            const bool is_user_mode = core_.privilege_mode() == PrivilegeMode::User;
            if (is_user_mode && !user_accessible) {
                raise_page_fault(type, vaddr);
                return false;
            }
            if (!is_user_mode && user_accessible) {
                raise_page_fault(type, vaddr);
                return false;
            }

            switch (type) {
            case AccessType::Instruction:
                if (!executable) {
                    raise_page_fault(type, vaddr);
                    return false;
                }
                break;
            case AccessType::Load:
                if (!readable) {
                    raise_page_fault(type, vaddr);
                    return false;
                }
                break;
            case AccessType::Store:
                if (!writable) {
                    raise_page_fault(type, vaddr);
                    return false;
                }
                break;
            }

            // Set A (Accessed) bit on any access, D (Dirty) bit on stores
            uint64_t updated_pte = pte | PTE_A;
            if (type == AccessType::Store) {
                updated_pte |= PTE_D;
            }

            // Write back updated PTE if A or D bits changed
            if (updated_pte != pte) {
                bus.try_store(pte_addr, updated_pte, SV39_PTESIZE);
            }

            const uint64_t pte_ppn = (pte >> 10) & ((1ULL << 44) - 1);
            const uint64_t page_offset = vaddr & ((1ULL << (12 + level * 9)) - 1);

            // For 4KB pages (level 0), no misalignment check needed
            // For 2MB pages (level 1), PPN[0] must be 0
            // For 1GB pages (level 2), PPN[1:0] must be 0
            if (level == 1) {
                if ((pte_ppn & SV39_VPN_MASK) != 0) {
                    raise_page_fault(type, vaddr);
                    return false;
                }
            } else if (level == 2) {
                if ((pte_ppn & ((1ULL << 18) - 1)) != 0) {
                    raise_page_fault(type, vaddr);
                    return false;
                }
            }

            paddr = (pte_ppn << 12) | page_offset;
            return true;
        }

        ppn = (pte >> 10) & ((1ULL << 44) - 1);
    }

    raise_page_fault(type, vaddr);
    return false;
}
