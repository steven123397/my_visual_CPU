#include "memory_ops.h"

#include "../cpu.h"
#include "../isa/effects.h"
#include "../mem/bus.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;
constexpr uint64_t kPageSize = 4096;

TrapRequest illegal_instruction_trap(uint32_t raw) {
    TrapRequest trap;
    trap.valid = true;
    trap.cause = CAUSE_ILLEGAL_INSN;
    trap.tval = raw;
    return trap;
}

InsnEffects illegal_memory_effect(uint32_t raw) {
    InsnEffects effects;
    effects.trap = illegal_instruction_trap(raw);
    effects.retired = false;
    return effects;
}

bool access_crosses_page(uint64_t addr, int size) {
    const uint64_t page_offset = addr & (kPageSize - 1);
    return page_offset + static_cast<uint64_t>(size) > kPageSize;
}

}  // namespace

uint64_t extend_loaded_value(uint64_t value, int size, bool sign_extend) {
    if (!sign_extend) {
        switch (size) {
        case 1:
            return static_cast<uint8_t>(value);
        case 2:
            return static_cast<uint16_t>(value);
        case 4:
            return static_cast<uint32_t>(value);
        default:
            return value;
        }
    }

    switch (size) {
    case 1:
        return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(value)));
    case 2:
        return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(value)));
    case 4:
        return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value)));
    default:
        return value;
    }
}

bool is_standard_fp_load(const Insn& insn) {
    return insn.opcode == 0x07 && (insn.funct3 == 2 || insn.funct3 == 3);
}

bool is_standard_fp_store(const Insn& insn) {
    return insn.opcode == 0x27 && (insn.funct3 == 2 || insn.funct3 == 3);
}

InsnEffects build_memory_effects(const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm) {
    InsnEffects effects;
    effects.mem.addr = rs1v + static_cast<uint64_t>(imm);

    switch (insn.opcode) {
    case 0x03:
        effects.mem.kind = MemoryRequest::Kind::Load;
        effects.mem.rd = insn.rd;
        switch (insn.funct3) {
        case 0:
            effects.mem.size = 1;
            effects.mem.sign_extend = true;
            return effects;
        case 1:
            effects.mem.size = 2;
            effects.mem.sign_extend = true;
            return effects;
        case 2:
            effects.mem.size = 4;
            effects.mem.sign_extend = true;
            return effects;
        case 3:
            effects.mem.size = 8;
            return effects;
        case 4:
            effects.mem.size = 1;
            return effects;
        case 5:
            effects.mem.size = 2;
            return effects;
        case 6:
            effects.mem.size = 4;
            return effects;
        default:
            return illegal_memory_effect(insn.raw);
        }
    case 0x07:
        effects.mem.kind = MemoryRequest::Kind::Load;
        effects.mem.target = MemoryRequest::Target::Float;
        effects.mem.rd = insn.rd;
        effects.floating_state_touched = true;
        switch (insn.funct3) {
        case 2:
            effects.mem.size = 4;
            return effects;
        case 3:
            effects.mem.size = 8;
            return effects;
        default:
            return illegal_memory_effect(insn.raw);
        }
    case 0x23:
        effects.mem.kind = MemoryRequest::Kind::Store;
        effects.mem.store_value = rs2v;
        effects.mem.commit_at_boundary = true;
        effects.mem.non_speculative = true;
        switch (insn.funct3) {
        case 0:
            effects.mem.size = 1;
            return effects;
        case 1:
            effects.mem.size = 2;
            return effects;
        case 2:
            effects.mem.size = 4;
            return effects;
        case 3:
            effects.mem.size = 8;
            return effects;
        default:
            return illegal_memory_effect(insn.raw);
        }
    case 0x27:
        effects.mem.kind = MemoryRequest::Kind::Store;
        effects.mem.target = MemoryRequest::Target::Float;
        effects.mem.store_value = rs2v;
        effects.mem.commit_at_boundary = true;
        effects.mem.non_speculative = true;
        effects.floating_state_touched = true;
        switch (insn.funct3) {
        case 2:
            effects.mem.size = 4;
            return effects;
        case 3:
            effects.mem.size = 8;
            return effects;
        default:
            return illegal_memory_effect(insn.raw);
        }
    default:
        return illegal_memory_effect(insn.raw);
    }
}

bool apply_memory_effects(CPU& cpu, Bus& bus, const InsnEffects& effects) {
    if (effects.trap.valid) {
        cpu.trap().enter_exception(effects.trap.cause, effects.trap.tval);
        return false;
    }

    switch (effects.mem.kind) {
    case MemoryRequest::Kind::Load: {
        uint64_t value = 0;
        if (!cpu.address_space().load(bus, effects.mem.addr, effects.mem.size, value)) {
            return false;
        }
        if (effects.mem.target == MemoryRequest::Target::Float) {
            cpu.core().write_fpr(effects.mem.rd, value);
        } else {
            cpu.core().write_gpr(effects.mem.rd, extend_loaded_value(value, effects.mem.size, effects.mem.sign_extend));
        }
        return effects.retired;
    }
    case MemoryRequest::Kind::Store: {
        AddressSpace::TranslateResult translated{};
        if (!access_crosses_page(effects.mem.addr, effects.mem.size)) {
            translated =
                cpu.address_space().translate_result(bus, effects.mem.addr, AccessType::Store, false);
        }
        if (!cpu.address_space().store(bus, effects.mem.addr, effects.mem.store_value, effects.mem.size)) {
            return false;
        }
        if (translated.ok) {
            cpu.trap().invalidate_reservation(translated.paddr, effects.mem.size);
        } else {
            cpu.trap().clear_reservation();
        }
        return effects.retired;
    }
    case MemoryRequest::Kind::None:
        return effects.retired;
    }

    return effects.retired;
}

bool execute_memory_instruction(CPU& cpu, Bus& bus, const Insn& insn, uint64_t rs1v, uint64_t rs2v, int64_t imm) {
    switch (insn.opcode) {
    case 0x03:
    case 0x07:
    case 0x23:
    case 0x27:
        return apply_memory_effects(cpu, bus, build_memory_effects(insn, rs1v, rs2v, imm));
    default:
        return false;
    }
}
