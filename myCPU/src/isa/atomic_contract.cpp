#include "atomic_contract.h"

#include "../cpu.h"
#include "../mem/address_space.h"
#include "../mem/bus.h"
#include "../trap.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;
constexpr uint64_t CAUSE_LOAD_ACCESS_FAULT = 5;
constexpr uint64_t CAUSE_STORE_ACCESS_FAULT = 7;
constexpr uint64_t kPageSize = 4096;

TrapRequest trap_request(uint64_t cause, uint64_t tval) {
    TrapRequest trap;
    trap.valid = true;
    trap.cause = cause;
    trap.tval = tval;
    return trap;
}

InsnEffects illegal_atomic_effect(uint32_t raw) {
    InsnEffects effects;
    effects.trap = trap_request(CAUSE_ILLEGAL_INSN, raw);
    effects.retired = false;
    return effects;
}

RegWrite rd_write(uint8_t rd, uint64_t value) {
    RegWrite write;
    write.enable = true;
    write.rd = rd;
    write.value = value;
    return write;
}

uint64_t sign_extend_word(uint64_t value) {
    return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(value))));
}

uint64_t load_result_value(uint64_t value, int size) {
    return size == 4 ? sign_extend_word(value) : value;
}

uint64_t store_masked_value(uint64_t value, int size) {
    return size == 4 ? static_cast<uint32_t>(value) : value;
}

uint64_t compute_amo_result(AtomicRequest::Kind kind,
                            uint64_t loaded,
                            uint64_t operand,
                            int size) {
    if (size == 4) {
        const uint32_t lhs = static_cast<uint32_t>(loaded);
        const uint32_t rhs = static_cast<uint32_t>(operand);
        switch (kind) {
        case AtomicRequest::Kind::Swap:
            return rhs;
        case AtomicRequest::Kind::Add:
            return static_cast<uint32_t>(lhs + rhs);
        case AtomicRequest::Kind::Xor:
            return lhs ^ rhs;
        case AtomicRequest::Kind::And:
            return lhs & rhs;
        case AtomicRequest::Kind::Or:
            return lhs | rhs;
        case AtomicRequest::Kind::Min:
            return static_cast<uint32_t>(static_cast<int32_t>(lhs) < static_cast<int32_t>(rhs) ? lhs : rhs);
        case AtomicRequest::Kind::Max:
            return static_cast<uint32_t>(static_cast<int32_t>(lhs) > static_cast<int32_t>(rhs) ? lhs : rhs);
        case AtomicRequest::Kind::MinUnsigned:
            return lhs < rhs ? lhs : rhs;
        case AtomicRequest::Kind::MaxUnsigned:
            return lhs > rhs ? lhs : rhs;
        case AtomicRequest::Kind::None:
        case AtomicRequest::Kind::LoadReserved:
        case AtomicRequest::Kind::StoreConditional:
            return lhs;
        }
    }

    switch (kind) {
    case AtomicRequest::Kind::Swap:
        return operand;
    case AtomicRequest::Kind::Add:
        return loaded + operand;
    case AtomicRequest::Kind::Xor:
        return loaded ^ operand;
    case AtomicRequest::Kind::And:
        return loaded & operand;
    case AtomicRequest::Kind::Or:
        return loaded | operand;
    case AtomicRequest::Kind::Min:
        return static_cast<uint64_t>(static_cast<int64_t>(loaded) < static_cast<int64_t>(operand) ? loaded : operand);
    case AtomicRequest::Kind::Max:
        return static_cast<uint64_t>(static_cast<int64_t>(loaded) > static_cast<int64_t>(operand) ? loaded : operand);
    case AtomicRequest::Kind::MinUnsigned:
        return loaded < operand ? loaded : operand;
    case AtomicRequest::Kind::MaxUnsigned:
        return loaded > operand ? loaded : operand;
    case AtomicRequest::Kind::None:
    case AtomicRequest::Kind::LoadReserved:
    case AtomicRequest::Kind::StoreConditional:
        return loaded;
    }

    return loaded;
}

bool is_misaligned(uint64_t addr, int size) {
    return size == 4 ? (addr & 0x3ULL) != 0 : (addr & 0x7ULL) != 0;
}

bool access_crosses_page(uint64_t addr, int size) {
    if (size <= 0) {
        return false;
    }
    const uint64_t page_offset = addr & (kPageSize - 1);
    return page_offset + static_cast<uint64_t>(size) > kPageSize;
}

AtomicRequest::Kind decode_atomic_kind(const Insn& insn) {
    switch (insn.funct5) {
    case 0x02:
        return AtomicRequest::Kind::LoadReserved;
    case 0x03:
        return AtomicRequest::Kind::StoreConditional;
    case 0x01:
        return AtomicRequest::Kind::Swap;
    case 0x00:
        return AtomicRequest::Kind::Add;
    case 0x04:
        return AtomicRequest::Kind::Xor;
    case 0x0c:
        return AtomicRequest::Kind::And;
    case 0x08:
        return AtomicRequest::Kind::Or;
    case 0x10:
        return AtomicRequest::Kind::Min;
    case 0x14:
        return AtomicRequest::Kind::Max;
    case 0x18:
        return AtomicRequest::Kind::MinUnsigned;
    case 0x1c:
        return AtomicRequest::Kind::MaxUnsigned;
    default:
        return AtomicRequest::Kind::None;
    }
}

}  // namespace

void invalidate_reservation_for_store(CPU& cpu, Bus& bus, uint64_t addr, int size) {
    if (size <= 0 || access_crosses_page(addr, size)) {
        cpu.trap().clear_reservation();
        return;
    }

    const AddressSpace::TranslateResult translated =
        cpu.address_space().translate_result(bus, addr, AccessType::Store, false);
    if (translated.ok) {
        cpu.trap().invalidate_reservation(translated.paddr, size);
    } else {
        cpu.trap().clear_reservation();
    }
}

InsnEffects build_atomic_effects(const Insn& insn, uint64_t rs1v, uint64_t rs2v) {
    const AtomicRequest::Kind kind = decode_atomic_kind(insn);
    if (kind == AtomicRequest::Kind::None) {
        return illegal_atomic_effect(insn.raw);
    }

    int size = 0;
    switch (insn.funct3) {
    case 0x2:
        size = 4;
        break;
    case 0x3:
        size = 8;
        break;
    default:
        return illegal_atomic_effect(insn.raw);
    }

    if (kind == AtomicRequest::Kind::LoadReserved && insn.rs2 != 0) {
        return illegal_atomic_effect(insn.raw);
    }

    InsnEffects effects;
    effects.atomic.kind = kind;
    effects.atomic.addr = rs1v;
    effects.atomic.store_value = rs2v;
    effects.atomic.rd = insn.rd;
    effects.atomic.size = size;
    effects.atomic.aq = insn.aq != 0;
    effects.atomic.rl = insn.rl != 0;
    return effects;
}

AtomicApplyResult apply_atomic_request(CPU& cpu, Bus& bus, const AtomicRequest& request) {
    AtomicApplyResult result;
    result.ok = false;
    result.bytes = static_cast<uint64_t>(request.size);

    if (request.kind == AtomicRequest::Kind::None) {
        result.ok = true;
        return result;
    }

    if ((request.size != 4 && request.size != 8) || is_misaligned(request.addr, request.size)) {
        const uint64_t cause = request.kind == AtomicRequest::Kind::LoadReserved
                                   ? CAUSE_LOAD_ACCESS_FAULT
                                   : CAUSE_STORE_ACCESS_FAULT;
        result.trap = trap_request(cause, request.addr);
        return result;
    }

    const AccessType access_type = request.kind == AtomicRequest::Kind::LoadReserved
                                       ? AccessType::Load
                                       : AccessType::Store;
    const AddressSpace::TranslateResult translated =
        cpu.address_space().translate_result(bus, request.addr, access_type, true);
    if (!translated.ok) {
        result.trap = translated.fault;
        return result;
    }
    result.paddr_valid = true;
    result.paddr = translated.paddr;

    const auto note_platform_state = [&]() {
        result.platform_state_changed |= bus.last_access().valid && bus.last_access().mmio;
    };

    switch (request.kind) {
    case AtomicRequest::Kind::LoadReserved: {
        uint64_t loaded = 0;
        if (!bus.try_load_observed(translated.paddr, request.size, loaded, "guest-data", "atomic-load-reserved")) {
            result.trap = trap_request(CAUSE_LOAD_ACCESS_FAULT, request.addr);
            return result;
        }
        note_platform_state();
        result.memory_observed = true;
        result.write_observed = false;
        cpu.trap().set_reservation(translated.paddr, request.size);
        result.rd_write = rd_write(request.rd, load_result_value(loaded, request.size));
        result.ok = true;
        return result;
    }
    case AtomicRequest::Kind::StoreConditional: {
        const bool matched = cpu.trap().reservation_matches(translated.paddr, request.size);
        cpu.trap().clear_reservation();
        if (!matched) {
            result.rd_write = rd_write(request.rd, 1);
            result.ok = true;
            return result;
        }
        if (!bus.try_store_observed(translated.paddr,
                                    store_masked_value(request.store_value, request.size),
                                    request.size,
                                    "guest-data",
                                    "atomic-store-conditional")) {
            result.trap = trap_request(CAUSE_STORE_ACCESS_FAULT, request.addr);
            return result;
        }
        note_platform_state();
        result.memory_observed = true;
        result.write_observed = true;
        result.rd_write = rd_write(request.rd, 0);
        result.ok = true;
        return result;
    }
    case AtomicRequest::Kind::Swap:
    case AtomicRequest::Kind::Add:
    case AtomicRequest::Kind::Xor:
    case AtomicRequest::Kind::And:
    case AtomicRequest::Kind::Or:
    case AtomicRequest::Kind::Min:
    case AtomicRequest::Kind::Max:
    case AtomicRequest::Kind::MinUnsigned:
    case AtomicRequest::Kind::MaxUnsigned: {
        uint64_t loaded = 0;
        if (!bus.try_load_observed(translated.paddr, request.size, loaded, "guest-data", "atomic-load")) {
            result.trap = trap_request(CAUSE_STORE_ACCESS_FAULT, request.addr);
            return result;
        }
        note_platform_state();
        const uint64_t stored = compute_amo_result(request.kind,
                                                   loaded,
                                                   request.store_value,
                                                   request.size);
        if (!bus.try_store_observed(translated.paddr,
                                    store_masked_value(stored, request.size),
                                    request.size,
                                    "guest-data",
                                    "atomic-store")) {
            result.trap = trap_request(CAUSE_STORE_ACCESS_FAULT, request.addr);
            return result;
        }
        note_platform_state();
        result.memory_observed = true;
        result.write_observed = true;
        cpu.trap().invalidate_reservation(translated.paddr, request.size);
        result.rd_write = rd_write(request.rd, load_result_value(loaded, request.size));
        result.ok = true;
        return result;
    }
    case AtomicRequest::Kind::None:
        result.ok = true;
        return result;
    }

    return result;
}
