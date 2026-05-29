#include "csr_file.h"

#include "core_state.h"
#include "../devices/clint.h"
#include "../mem/address_space.h"

namespace {

constexpr uint64_t SSTATUS_MASK = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_SPP | MSTATUS_SUM | MSTATUS_MXR;
constexpr uint64_t SIE_MASK = MIE_SSIE | MIE_STIE | MIE_SEIE;
constexpr uint64_t MIP_MIE_MASK = MIE_SSIE | MIE_MSIE | MIE_STIE | MIE_MTIE | MIE_SEIE | MIE_MEIE;
constexpr uint64_t MENVCFG_STCE = 1ULL << 63;
constexpr uint64_t FCSR_FFLAGS_MASK = 0x1FULL;
constexpr uint64_t FCSR_FRM_MASK = 0x7ULL << 5;
constexpr uint64_t FCSR_MASK = FCSR_FFLAGS_MASK | FCSR_FRM_MASK;
constexpr uint64_t SATP_MODE_SHIFT = 60;
constexpr uint64_t SATP_MODE_MASK = 0xFULL << SATP_MODE_SHIFT;
constexpr uint64_t SATP_MODE_BARE = 0ULL;
constexpr uint64_t SATP_MODE_SV39 = 8ULL;
constexpr uint64_t SATP_PPN_MASK = (1ULL << 44) - 1ULL;
constexpr uint64_t MISA_IMPLEMENTED_VALUE =
    (2ULL << 62) | (1ULL << 20) | (1ULL << 18) | (1ULL << 12) | (1ULL << 8) | (1ULL << 2) | (1ULL << 0);
constexpr uint64_t MEDELEG_MASK =
    (1ULL << 1) |   // instruction access fault
    (1ULL << 2) |   // illegal instruction
    (1ULL << 3) |   // breakpoint
    (1ULL << 5) |   // load access fault
    (1ULL << 7) |   // store/AMO access fault
    (1ULL << 8) |   // environment call from U-mode
    (1ULL << 9) |   // environment call from S-mode
    (1ULL << 12) |  // instruction page fault
    (1ULL << 13) |  // load page fault
    (1ULL << 15);   // store/AMO page fault
constexpr uint64_t MIDELEG_MASK = SIE_MASK;

constexpr uint64_t SSTATUS_STATUS_MASK = SSTATUS_MASK | MSTATUS_FS_MASK | MSTATUS_VS_MASK;

uint64_t with_sd_summary(uint64_t mstatus) {
    const bool dirty = (mstatus & MSTATUS_FS_MASK) == MSTATUS_FS_DIRTY ||
                       (mstatus & MSTATUS_VS_MASK) == MSTATUS_VS_DIRTY;
    if (dirty) {
        return mstatus | MSTATUS_SD;
    }
    return mstatus & ~MSTATUS_SD;
}

bool is_hpmcounter_shadow(uint32_t addr) {
    return addr >= CSR_HPMCOUNTER3 && addr <= CSR_HPMCOUNTER31;
}

bool is_mhpmcounter(uint32_t addr) {
    return addr >= CSR_MHPMCOUNTER3 && addr <= CSR_MHPMCOUNTER31;
}

bool is_mhpmevent(uint32_t addr) {
    return addr >= CSR_MHPMEVENT3 && addr <= CSR_MHPMEVENT31;
}

uint32_t hpm_shadow_to_machine(uint32_t addr) {
    return CSR_MHPMCOUNTER3 + (addr - CSR_HPMCOUNTER3);
}

bool is_supported_csr(uint32_t addr) {
    if (is_hpmcounter_shadow(addr) || is_mhpmcounter(addr) || is_mhpmevent(addr)) {
        return true;
    }
    switch (addr & 0xFFF) {
    case CSR_SSTATUS:
    case CSR_SIE:
    case CSR_STVEC:
    case CSR_SCOUNTEREN:
    case CSR_SSCRATCH:
    case CSR_SEPC:
    case CSR_SCAUSE:
    case CSR_STVAL:
    case CSR_SIP:
    case CSR_SATP:
    case CSR_FFLAGS:
    case CSR_FRM:
    case CSR_FCSR:
    case CSR_MSTATUS:
    case CSR_MISA:
    case CSR_MEDELEG:
    case CSR_MIDELEG:
    case CSR_MIE:
    case CSR_MTVEC:
    case CSR_MCOUNTEREN:
    case CSR_MCOUNTINHIBIT:
    case CSR_MENVCFG:
    case CSR_PMPCFG0:
    case CSR_PMPADDR0:
    case CSR_MCYCLE:
    case CSR_MINSTRET:
    case CSR_MSCRATCH:
    case CSR_MEPC:
    case CSR_MCAUSE:
    case CSR_MTVAL:
    case CSR_MIP:
    case CSR_MHARTID:
    case CSR_CYCLE:
    case CSR_TIME:
    case CSR_INSTRET:
    case CSR_STIMECMP:
        return true;
    default:
        return false;
    }
}

uint64_t current_time(const Clint* clint, const CoreState& core) {
    return clint ? clint->mtime() : core.cycle();
}

bool sstc_enabled(const std::array<uint64_t, 4096>& regs) {
    return (regs[CSR_MENVCFG] & MENVCFG_STCE) != 0;
}

bool stimecmp_pending(const std::array<uint64_t, 4096>& regs,
                      const Clint* clint,
                      const CoreState& core) {
    return sstc_enabled(regs) && current_time(clint, core) >= regs[CSR_STIMECMP];
}

uint64_t sanitize_mstatus(uint64_t value) {
    value &= ~MSTATUS_SD;
    const uint64_t mpp = (value & MSTATUS_MPP_MASK) >> MSTATUS_MPP_SHIFT;
    if (mpp == 2ULL) {
        value &= ~MSTATUS_MPP_MASK;
    }
    return with_sd_summary(value);
}

}

CsrFile::CsrFile(const CsrFile& other)
    : regs_(other.regs_), clint_(other.clint_) {}

CsrFile& CsrFile::operator=(const CsrFile& other) {
    if (this == &other) {
        return *this;
    }
    regs_ = other.regs_;
    clint_ = other.clint_;
    address_space_ = nullptr;
    return *this;
}

void CsrFile::reset() {
    regs_.fill(0);
    regs_[CSR_MISA] = MISA_IMPLEMENTED_VALUE;
    regs_[CSR_STIMECMP] = UINT64_MAX;
}

void CsrFile::bind_clint(const Clint* clint) {
    clint_ = clint;
}

void CsrFile::bind_address_space(AddressSpace* address_space) {
    address_space_ = address_space;
}

uint64_t CsrFile::read(uint32_t addr, const CoreState& core) const {
    addr &= 0xFFF;
    if (addr == CSR_CYCLE || addr == CSR_MCYCLE) {
        return core.cycle();
    }
    if (addr == CSR_TIME) {
        return clint_ ? clint_->mtime() : core.cycle();
    }
    if (addr == CSR_INSTRET || addr == CSR_MINSTRET) {
        return core.instret();
    }
    if (is_hpmcounter_shadow(addr)) {
        return regs_[hpm_shadow_to_machine(addr)];
    }
    if (is_mhpmcounter(addr)) {
        return regs_[addr];
    }
    if (addr == CSR_SSTATUS) {
        return with_sd_summary(regs_[CSR_MSTATUS]) & (SSTATUS_STATUS_MASK | MSTATUS_SD);
    }
    if (addr == CSR_FFLAGS) {
        return regs_[CSR_FCSR] & FCSR_FFLAGS_MASK;
    }
    if (addr == CSR_FRM) {
        return (regs_[CSR_FCSR] & FCSR_FRM_MASK) >> 5;
    }
    if (addr == CSR_FCSR) {
        return regs_[CSR_FCSR] & FCSR_MASK;
    }
    if (addr == CSR_MHARTID) {
        return 0;
    }
    if (addr == CSR_MEDELEG) {
        return regs_[CSR_MEDELEG] & MEDELEG_MASK;
    }
    if (addr == CSR_MIDELEG) {
        return regs_[CSR_MIDELEG] & MIDELEG_MASK;
    }
    if (addr == CSR_MIE) {
        return regs_[CSR_MIE] & MIP_MIE_MASK;
    }
    if (addr == CSR_MIP) {
        uint64_t value = regs_[CSR_MIP] & MIP_MIE_MASK;
        if (stimecmp_pending(regs_, clint_, core)) {
            value |= MIE_STIE;
        }
        return value;
    }
    if (addr == CSR_SIE) {
        return regs_[CSR_MIE] & regs_[CSR_MIDELEG] & SIE_MASK;
    }
    if (addr == CSR_SIP) {
        uint64_t value = regs_[CSR_MIP] & regs_[CSR_MIDELEG] & SIE_MASK;
        if (stimecmp_pending(regs_, clint_, core) && (regs_[CSR_MIDELEG] & MIE_STIE) != 0) {
            value |= MIE_STIE;
        }
        return value;
    }
    if (addr == CSR_MENVCFG) {
        return regs_[CSR_MENVCFG] & MENVCFG_STCE;
    }
    if (addr == CSR_MSTATUS) {
        return with_sd_summary(regs_[CSR_MSTATUS]);
    }
    return regs_[addr];
}

void CsrFile::write(uint32_t addr, uint64_t value) {
    addr &= 0xFFF;
    if (addr == CSR_CYCLE || addr == CSR_TIME || addr == CSR_INSTRET || addr == CSR_MCYCLE || addr == CSR_MINSTRET) {
        return;
    }
    if (addr == CSR_MISA || addr == CSR_MHARTID) {
        return;
    }
    if (addr == CSR_SSTATUS) {
        regs_[CSR_MSTATUS] = sanitize_mstatus((regs_[CSR_MSTATUS] & ~SSTATUS_STATUS_MASK) |
                                              (value & SSTATUS_STATUS_MASK));
        return;
    }
    if (addr == CSR_FFLAGS) {
        regs_[CSR_FCSR] = (regs_[CSR_FCSR] & ~FCSR_FFLAGS_MASK) | (value & FCSR_FFLAGS_MASK);
        return;
    }
    if (addr == CSR_FRM) {
        regs_[CSR_FCSR] = (regs_[CSR_FCSR] & ~FCSR_FRM_MASK) | ((value & 0x7ULL) << 5);
        return;
    }
    if (addr == CSR_FCSR) {
        regs_[CSR_FCSR] = value & FCSR_MASK;
        return;
    }
    if (addr == CSR_MSTATUS) {
        regs_[CSR_MSTATUS] = sanitize_mstatus(value);
        return;
    }
    if (addr == CSR_SATP) {
        const uint64_t mode = (value & SATP_MODE_MASK) >> SATP_MODE_SHIFT;
        if (mode == SATP_MODE_SV39) {
            regs_[CSR_SATP] = (SATP_MODE_SV39 << SATP_MODE_SHIFT) | (value & SATP_PPN_MASK);
        } else {
            regs_[CSR_SATP] = SATP_MODE_BARE;
        }
        if (address_space_ != nullptr) {
            address_space_->flush_tlb();
        }
        return;
    }
    if (addr == CSR_MEDELEG) {
        regs_[CSR_MEDELEG] = value & MEDELEG_MASK;
        return;
    }
    if (addr == CSR_MIDELEG) {
        regs_[CSR_MIDELEG] = value & MIDELEG_MASK;
        return;
    }
    if (addr == CSR_MIE) {
        regs_[CSR_MIE] = value & MIP_MIE_MASK;
        return;
    }
    if (addr == CSR_MCOUNTINHIBIT) {
        regs_[CSR_MCOUNTINHIBIT] = value;
        return;
    }
    if (addr == CSR_MIP) {
        regs_[CSR_MIP] = value & MIP_MIE_MASK;
        return;
    }
    if (addr == CSR_MENVCFG) {
        regs_[CSR_MENVCFG] = value & MENVCFG_STCE;
        return;
    }
    if (is_hpmcounter_shadow(addr)) {
        regs_[hpm_shadow_to_machine(addr)] = value;
        return;
    }
    if (is_mhpmcounter(addr) || is_mhpmevent(addr)) {
        regs_[addr] = value;
        return;
    }
    if (addr == CSR_SIE) {
        const uint64_t mask = regs_[CSR_MIDELEG] & SIE_MASK;
        regs_[CSR_MIE] = (regs_[CSR_MIE] & ~mask) | (value & mask);
        return;
    }
    if (addr == CSR_SIP) {
        const uint64_t mask = regs_[CSR_MIDELEG] & SIE_MASK;
        regs_[CSR_MIP] = (regs_[CSR_MIP] & ~mask) | (value & mask);
        return;
    }
    regs_[addr] = value;
}

void CsrFile::write(uint32_t addr, uint64_t value, CoreState& core) {
    addr &= 0xFFF;
    if (addr == CSR_MCYCLE) {
        core.set_cycle(value);
        return;
    }
    if (addr == CSR_MINSTRET) {
        core.set_instret(value);
        return;
    }
    if (is_mhpmcounter(addr)) {
        regs_[addr] = value;
        return;
    }
    write(addr, value);
}

bool CsrFile::is_implemented(uint32_t addr) const {
    return is_supported_csr(addr);
}
