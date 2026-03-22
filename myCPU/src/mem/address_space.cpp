#include "address_space.h"

#include "../arch/core_state.h"
#include "../arch/csr_file.h"
#include "../trap.h"
#include "bus.h"

namespace {

constexpr uint64_t CAUSE_INSN_ACCESS_FAULT = 1;
constexpr uint64_t CAUSE_LOAD_ACCESS_FAULT = 5;
constexpr uint64_t CAUSE_STORE_ACCESS_FAULT = 7;

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
    if (!translate(addr, AccessType::Store, paddr)) {
        raise_access_fault(AccessType::Store, addr);
        return false;
    }
    if (!bus.try_store(paddr, value, size)) {
        raise_access_fault(AccessType::Store, addr);
        return false;
    }
    return true;
}

bool AddressSpace::translate(uint64_t vaddr, AccessType /*type*/, uint64_t& paddr) const {
    (void)csr_;
    paddr = vaddr;
    return true;
}

bool AddressSpace::access(Bus& bus, uint64_t vaddr, int size, AccessType type, uint64_t& value) {
    uint64_t paddr = 0;
    if (!translate(vaddr, type, paddr)) {
        raise_access_fault(type, vaddr);
        return false;
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
