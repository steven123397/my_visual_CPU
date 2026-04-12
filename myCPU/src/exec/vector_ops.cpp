#include "vector_ops.h"

#include "../../include/platform_mmio.h"
#include "../cpu.h"
#include "../mem/bus.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;
constexpr uint64_t CAUSE_LOAD_ACCESS_FAULT = 5;
constexpr uint64_t CAUSE_STORE_ACCESS_FAULT = 7;

TrapRequest illegal_trap(uint64_t tval) {
    TrapRequest trap;
    trap.valid = true;
    trap.cause = CAUSE_ILLEGAL_INSN;
    trap.tval = tval;
    return trap;
}

InsnEffects illegal_effect(uint32_t raw) {
    InsnEffects effects;
    effects.trap = illegal_trap(raw);
    effects.retired = false;
    return effects;
}

uint8_t decode_sew_bytes(uint8_t sew_code) {
    return static_cast<uint8_t>(sew_code + 1U);
}

uint64_t element_mask(uint8_t sew_bytes) {
    if (sew_bytes >= 8) {
        return ~0ULL;
    }
    return (1ULL << (static_cast<unsigned>(sew_bytes) * 8U)) - 1ULL;
}

uint64_t load_element(const VectorState::VectorReg& reg, size_t offset, uint8_t sew_bytes) {
    uint64_t value = 0;
    for (uint8_t i = 0; i < sew_bytes; ++i) {
        value |= static_cast<uint64_t>(reg[offset + i]) << (static_cast<unsigned>(i) * 8U);
    }
    return value;
}

void store_element(VectorState::VectorReg& reg, size_t offset, uint8_t sew_bytes, uint64_t value) {
    for (uint8_t i = 0; i < sew_bytes; ++i) {
        reg[offset + i] =
            static_cast<uint8_t>((value >> (static_cast<unsigned>(i) * 8U)) & 0xFFU);
    }
}

int64_t sign_extend_element(uint64_t value, uint8_t sew_bytes) {
    if (sew_bytes >= 8) {
        return static_cast<int64_t>(value);
    }
    const unsigned bits = static_cast<unsigned>(sew_bytes) * 8U;
    const unsigned shift = 64U - bits;
    return static_cast<int64_t>(value << shift) >> shift;
}

bool read_runtime_config(const VectorState& state, uint8_t& sew_bytes, uint8_t& vl) {
    sew_bytes = state.sew_bytes();
    vl = state.vl();
    return VectorState::is_valid_config(sew_bytes, vl, true);
}

TrapRequest access_fault(AccessType type, uint64_t tval) {
    TrapRequest trap;
    trap.valid = true;
    trap.cause = (type == AccessType::Store) ? CAUSE_STORE_ACCESS_FAULT : CAUSE_LOAD_ACCESS_FAULT;
    trap.tval = tval;
    return trap;
}

bool is_ram_physical_access(uint64_t addr, int size) {
    if (size <= 0) {
        return false;
    }
    const uint64_t end = addr + static_cast<uint64_t>(size);
    return addr >= MEM_BASE && end > addr && end <= MEM_BASE + MEM_SIZE;
}

bool validate_vector_memory_span(CPU& cpu,
                                 Bus& bus,
                                 uint64_t addr,
                                 size_t bytes,
                                 AccessType type,
                                 TrapRequest& fault) {
    for (size_t i = 0; i < bytes; ++i) {
        const uint64_t current_addr = addr + i;
        const AddressSpace::TranslateResult translated =
            cpu.address_space().translate_result(bus, current_addr, type, false);
        if (!translated.ok) {
            fault = translated.fault;
            return false;
        }
        if (!is_ram_physical_access(translated.paddr, 1)) {
            fault = access_fault(type, current_addr);
            return false;
        }
    }
    return true;
}

}  // namespace

bool is_vector_opcode(uint8_t opcode) {
    return opcode == 0x57 || opcode == 0x07 || opcode == 0x27;
}

bool is_serializing_vector_insn(const Insn& insn) {
    if (!is_vector_opcode(insn.opcode)) {
        return false;
    }
    if (insn.opcode == 0x07 || insn.opcode == 0x27) {
        return true;
    }
    if (insn.opcode != 0x57) {
        return false;
    }
    if (insn.funct3 == 7) {
        return true;
    }
    return insn.funct3 != 0;
}

bool is_non_memory_vector_alu_insn(const Insn& insn) {
    if (insn.opcode != 0x57 || insn.funct3 != 0) {
        return false;
    }
    switch (insn.funct7) {
    case 0x00:
    case 0x20:
    case 0x21:
    case 0x22:
        return true;
    default:
        return false;
    }
}

bool is_non_memory_vector_alu(const VectorRequest& request) {
    switch (request.kind) {
    case VectorRequest::Kind::Add:
    case VectorRequest::Kind::Mul:
    case VectorRequest::Kind::Max:
    case VectorRequest::Kind::Dot:
        return true;
    default:
        return false;
    }
}

InsnEffects build_vector_effects(const Insn& insn, uint64_t rs1v) {
    if (!is_vector_opcode(insn.opcode)) {
        return illegal_effect(insn.raw);
    }

    InsnEffects effects;
    switch (insn.opcode) {
    case 0x57:
        if (insn.funct3 == 7) {
            if ((insn.funct7 >> 3) != 0x8U) {
                return illegal_effect(insn.raw);
            }
            const uint8_t sew_bytes = decode_sew_bytes(insn.funct7 & 0x7U);
            const uint8_t vl = static_cast<uint8_t>(insn.rs2 + 1U);
            if (!VectorState::is_valid_config(sew_bytes, vl, false)) {
                return illegal_effect(insn.raw);
            }
            effects.vector.kind = VectorRequest::Kind::SetConfig;
            effects.vector.sew_bytes = sew_bytes;
            effects.vector.vl = vl;
            return effects;
        }

        if (insn.funct3 != 0) {
            return illegal_effect(insn.raw);
        }
        effects.vector.vd = insn.rd;
        effects.vector.vs1 = insn.rs1;
        effects.vector.vs2 = insn.rs2;
        switch (insn.funct7) {
        case 0x00:
            effects.vector.kind = VectorRequest::Kind::Add;
            return effects;
        case 0x20:
            effects.vector.kind = VectorRequest::Kind::Mul;
            return effects;
        case 0x21:
            effects.vector.kind = VectorRequest::Kind::Max;
            return effects;
        case 0x22:
            effects.vector.kind = VectorRequest::Kind::Dot;
            return effects;
        default:
            return illegal_effect(insn.raw);
        }
    case 0x07:
        if (insn.funct3 != 0) {
            return illegal_effect(insn.raw);
        }
        effects.vector.kind = VectorRequest::Kind::Load;
        effects.vector.vd = insn.rd;
        effects.vector.addr = rs1v + static_cast<uint64_t>(insn.imm);
        return effects;
    case 0x27:
        if (insn.funct3 != 0) {
            return illegal_effect(insn.raw);
        }
        effects.vector.kind = VectorRequest::Kind::Store;
        effects.vector.vs2 = insn.rs2;
        effects.vector.addr = rs1v + static_cast<uint64_t>(insn.imm);
        return effects;
    default:
        return illegal_effect(insn.raw);
    }
}

VectorComputeResult compute_vector_alu_result(const VectorState& vector,
                                              const VectorRequest& request) {
    VectorComputeResult result;
    result.result.fill(0);

    if (!is_non_memory_vector_alu(request)) {
        result.ok = false;
        result.trap = illegal_trap(0);
        return result;
    }

    uint8_t sew_bytes = 0;
    uint8_t vl = 0;
    if (!read_runtime_config(vector, sew_bytes, vl)) {
        result.ok = false;
        result.trap = illegal_trap(0);
        return result;
    }

    const VectorState::VectorReg& lhs = vector.read_reg(request.vs1);
    const VectorState::VectorReg& rhs = vector.read_reg(request.vs2);
    if (request.kind == VectorRequest::Kind::Dot) {
        __int128 sum = 0;
        for (uint8_t lane = 0; lane < vl; ++lane) {
            const size_t offset = static_cast<size_t>(lane) * static_cast<size_t>(sew_bytes);
            const int64_t a = sign_extend_element(load_element(lhs, offset, sew_bytes), sew_bytes);
            const int64_t b = sign_extend_element(load_element(rhs, offset, sew_bytes), sew_bytes);
            sum += static_cast<__int128>(a) * static_cast<__int128>(b);
        }
        store_element(result.result, 0, 8, static_cast<uint64_t>(sum));
        return result;
    }

    const uint64_t mask = element_mask(sew_bytes);
    for (uint8_t lane = 0; lane < vl; ++lane) {
        const size_t offset = static_cast<size_t>(lane) * static_cast<size_t>(sew_bytes);
        const uint64_t a = load_element(lhs, offset, sew_bytes);
        const uint64_t b = load_element(rhs, offset, sew_bytes);
        uint64_t value = 0;
        if (request.kind == VectorRequest::Kind::Add) {
            value = (a + b) & mask;
        } else if (request.kind == VectorRequest::Kind::Mul) {
            value = static_cast<uint64_t>(static_cast<__uint128_t>(a) *
                                          static_cast<__uint128_t>(b)) &
                    mask;
        } else {
            const int64_t sa = sign_extend_element(a, sew_bytes);
            const int64_t sb = sign_extend_element(b, sew_bytes);
            value = (sa >= sb ? a : b) & mask;
        }
        store_element(result.result, offset, sew_bytes, value);
    }
    return result;
}

VectorApplyResult apply_vector_request(CPU& cpu, Bus& bus, const VectorRequest& request) {
    VectorApplyResult result;
    VectorState& vector = cpu.core().vector();

    switch (request.kind) {
    case VectorRequest::Kind::None:
        return result;
    case VectorRequest::Kind::SetConfig:
        if (!vector.set_config(request.sew_bytes, request.vl)) {
            result.ok = false;
            result.trap = illegal_trap(0);
        }
        return result;
    case VectorRequest::Kind::Load: {
        uint8_t sew_bytes = 0;
        uint8_t vl = 0;
        if (!read_runtime_config(vector, sew_bytes, vl)) {
            result.ok = false;
            result.trap = illegal_trap(0);
            return result;
        }
        if (vl == 0) {
            return result;
        }

        VectorState::VectorReg reg{};
        reg.fill(0);
        const size_t bytes = static_cast<size_t>(vl) * static_cast<size_t>(sew_bytes);
        if (!validate_vector_memory_span(cpu, bus, request.addr, bytes, AccessType::Load, result.trap)) {
            result.ok = false;
            return result;
        }
        for (size_t i = 0; i < bytes; ++i) {
            const AddressSpace::AccessResult access =
                cpu.address_space().load_result(bus, request.addr + i, 1);
            if (!access.ok) {
                result.ok = false;
                result.trap = access.fault;
                return result;
            }
            reg[i] = static_cast<uint8_t>(access.value & 0xFFU);
        }
        vector.write_reg(request.vd, reg);
        return result;
    }
    case VectorRequest::Kind::Store: {
        uint8_t sew_bytes = 0;
        uint8_t vl = 0;
        if (!read_runtime_config(vector, sew_bytes, vl)) {
            result.ok = false;
            result.trap = illegal_trap(0);
            return result;
        }
        if (vl == 0) {
            return result;
        }

        const VectorState::VectorReg& reg = vector.read_reg(request.vs2);
        const size_t bytes = static_cast<size_t>(vl) * static_cast<size_t>(sew_bytes);
        if (!validate_vector_memory_span(cpu, bus, request.addr, bytes, AccessType::Store, result.trap)) {
            result.ok = false;
            return result;
        }
        for (size_t i = 0; i < bytes; ++i) {
            const AddressSpace::AccessResult access =
                cpu.address_space().store_result(bus, request.addr + i, reg[i], 1);
            if (!access.ok) {
                result.ok = false;
                result.trap = access.fault;
                return result;
            }
            result.platform_state_changed |= bus.last_access().valid && bus.last_access().mmio;
        }
        return result;
    }
    case VectorRequest::Kind::Add:
    case VectorRequest::Kind::Mul:
    case VectorRequest::Kind::Max: {
        if (request.result_valid) {
            vector.write_reg(request.vd, request.result);
            return result;
        }
        const VectorComputeResult compute = compute_vector_alu_result(vector, request);
        if (!compute.ok) {
            result.ok = false;
            result.trap = compute.trap;
            return result;
        }
        vector.write_reg(request.vd, compute.result);
        return result;
    }
    case VectorRequest::Kind::Dot: {
        if (request.result_valid) {
            vector.write_reg(request.vd, request.result);
            return result;
        }
        const VectorComputeResult compute = compute_vector_alu_result(vector, request);
        if (!compute.ok) {
            result.ok = false;
            result.trap = compute.trap;
            return result;
        }
        vector.write_reg(request.vd, compute.result);
        return result;
    }
    }

    result.ok = false;
    result.trap = illegal_trap(0);
    return result;
}
