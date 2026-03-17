#pragma once

#include <array>
#include <cstdint>

class CoreState;

constexpr uint32_t CSR_MSTATUS = 0x300;
constexpr uint32_t CSR_MISA = 0x301;
constexpr uint32_t CSR_MIE = 0x304;
constexpr uint32_t CSR_MTVEC = 0x305;
constexpr uint32_t CSR_MSCRATCH = 0x340;
constexpr uint32_t CSR_MEPC = 0x341;
constexpr uint32_t CSR_MCAUSE = 0x342;
constexpr uint32_t CSR_MTVAL = 0x343;
constexpr uint32_t CSR_MIP = 0x344;
constexpr uint32_t CSR_CYCLE = 0xC00;
constexpr uint32_t CSR_TIME = 0xC01;

constexpr uint64_t MSTATUS_MIE = 1ULL << 3;
constexpr uint64_t MSTATUS_MPIE = 1ULL << 7;

constexpr uint64_t MIE_MSIE = 1ULL << 3;
constexpr uint64_t MIE_MTIE = 1ULL << 7;
constexpr uint64_t MIE_MEIE = 1ULL << 11;

class CsrFile {
public:
    void reset();
    uint64_t read(uint32_t addr, const CoreState& core) const;
    void write(uint32_t addr, uint64_t value);

private:
    std::array<uint64_t, 4096> regs_{};
};
