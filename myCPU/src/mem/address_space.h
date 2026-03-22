#pragma once

#include <cstdint>

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
    AddressSpace(CoreState& core, CsrFile& csr, TrapController& trap);

    bool fetch32(Bus& bus, uint32_t& raw);
    bool load(Bus& bus, uint64_t addr, int size, uint64_t& value);
    bool store(Bus& bus, uint64_t addr, uint64_t value, int size);

private:
    bool translate(uint64_t vaddr, AccessType type, uint64_t& paddr) const;
    bool access(Bus& bus, uint64_t vaddr, int size, AccessType type, uint64_t& value);
    void raise_access_fault(AccessType type, uint64_t addr);

    CoreState& core_;
    CsrFile& csr_;
    TrapController& trap_;
};
