#pragma once

#include <array>
#include <cstdint>

class CoreState;
class Clint;
class AddressSpace;

constexpr uint32_t CSR_SSTATUS = 0x100;
constexpr uint32_t CSR_SIE = 0x104;
constexpr uint32_t CSR_STVEC = 0x105;
constexpr uint32_t CSR_SCOUNTEREN = 0x106;
constexpr uint32_t CSR_SSCRATCH = 0x140;
constexpr uint32_t CSR_SEPC = 0x141;
constexpr uint32_t CSR_SCAUSE = 0x142;
constexpr uint32_t CSR_STVAL = 0x143;
constexpr uint32_t CSR_SIP = 0x144;
constexpr uint32_t CSR_SATP = 0x180;
constexpr uint32_t CSR_MSTATUS = 0x300;
constexpr uint32_t CSR_MISA = 0x301;
constexpr uint32_t CSR_MEDELEG = 0x302;
constexpr uint32_t CSR_MIDELEG = 0x303;
constexpr uint32_t CSR_MIE = 0x304;
constexpr uint32_t CSR_MTVEC = 0x305;
constexpr uint32_t CSR_MCOUNTEREN = 0x306;
constexpr uint32_t CSR_MENVCFG = 0x30A;
constexpr uint32_t CSR_PMPCFG0 = 0x3A0;
constexpr uint32_t CSR_PMPADDR0 = 0x3B0;
constexpr uint32_t CSR_MCYCLE = 0xB00;
constexpr uint32_t CSR_MINSTRET = 0xB02;
constexpr uint32_t CSR_MSCRATCH = 0x340;
constexpr uint32_t CSR_MEPC = 0x341;
constexpr uint32_t CSR_MCAUSE = 0x342;
constexpr uint32_t CSR_MTVAL = 0x343;
constexpr uint32_t CSR_MIP = 0x344;
constexpr uint32_t CSR_MHARTID = 0xF14;
constexpr uint32_t CSR_CYCLE = 0xC00;
constexpr uint32_t CSR_TIME = 0xC01;
constexpr uint32_t CSR_INSTRET = 0xC02;
constexpr uint32_t CSR_STIMECMP = 0x14D;

constexpr uint64_t MSTATUS_SIE = 1ULL << 1;
constexpr uint64_t MSTATUS_MIE = 1ULL << 3;
constexpr uint64_t MSTATUS_SPIE = 1ULL << 5;
constexpr uint64_t MSTATUS_MPIE = 1ULL << 7;
constexpr uint64_t MSTATUS_SPP = 1ULL << 8;
constexpr uint64_t MSTATUS_MPRV = 1ULL << 17;
constexpr uint64_t MSTATUS_SUM = 1ULL << 18;
constexpr uint64_t MSTATUS_MXR = 1ULL << 19;
constexpr uint64_t MSTATUS_MPP_SHIFT = 11;
constexpr uint64_t MSTATUS_MPP_MASK = 0x3ULL << MSTATUS_MPP_SHIFT;

constexpr uint64_t MIE_SSIE = 1ULL << 1;
constexpr uint64_t MIE_STIE = 1ULL << 5;
constexpr uint64_t MIE_MSIE = 1ULL << 3;
constexpr uint64_t MIE_MTIE = 1ULL << 7;
constexpr uint64_t MIE_SEIE = 1ULL << 9;
constexpr uint64_t MIE_MEIE = 1ULL << 11;

class CsrFile {
public:
    CsrFile() = default;
    CsrFile(const CsrFile& other);
    CsrFile& operator=(const CsrFile& other);

    void reset();
    void bind_clint(const Clint* clint);
    void bind_address_space(AddressSpace* address_space);
    uint64_t read(uint32_t addr, const CoreState& core) const;
    void write(uint32_t addr, uint64_t value);
    void write(uint32_t addr, uint64_t value, CoreState& core);
    bool is_implemented(uint32_t addr) const;

private:
    std::array<uint64_t, 4096> regs_{};
    const Clint* clint_{nullptr};
    AddressSpace* address_space_{nullptr};
};
