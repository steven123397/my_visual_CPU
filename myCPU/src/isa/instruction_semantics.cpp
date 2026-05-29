#include "instruction_semantics.h"

#include "atomic_contract.h"
#include "execution_context.h"

#include "../arch/csr_file.h"
#include "../arch/core_state.h"
#include "../exec/control_flow_ops.h"
#include "../exec/floating_ops.h"
#include "../exec/integer_ops.h"
#include "../exec/memory_ops.h"
#include "../exec/system_ops.h"
#include "../exec/vector_ops.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;

bool is_floating_csr_access(const Insn& insn) {
    if (insn.opcode != 0x73 || insn.funct3 == 0) {
        return false;
    }
    const uint32_t csr_addr = (insn.raw >> 20) & 0xFFFU;
    return csr_addr == CSR_FFLAGS || csr_addr == CSR_FRM || csr_addr == CSR_FCSR;
}

bool requires_enabled_floating_state(const Insn& insn) {
    return insn.opcode == 0x53 ||
           insn.opcode == 0x43 ||
           insn.opcode == 0x47 ||
           insn.opcode == 0x4B ||
           insn.opcode == 0x4F ||
           is_standard_fp_load(insn) ||
           is_standard_fp_store(insn) ||
           is_floating_csr_access(insn);
}

InsnEffects illegal_instruction_effect(uint32_t raw) {
    InsnEffects effects;
    effects.trap.valid = true;
    effects.trap.cause = CAUSE_ILLEGAL_INSN;
    effects.trap.tval = raw;
    effects.retired = false;
    return effects;
}

}  // namespace

bool InstructionSemantics::supports(const Insn& insn) {
    switch (insn.opcode) {
    case 0x37:
    case 0x17:
    case 0x13:
    case 0x1B:
    case 0x33:
    case 0x3B:
    case 0x0F:
    case 0x6F:
    case 0x67:
    case 0x63:
    case 0x03:
    case 0x23:
    case 0x2F:
    case 0x73:
    case 0x07:
    case 0x27:
    case 0x53:
    case 0x43:
    case 0x47:
    case 0x4B:
    case 0x4F:
    case 0x57:
        return true;
    default:
        return false;
    }
}

InstructionRegisterDescriptor InstructionSemantics::describe_registers(const Insn& insn) {
    InstructionRegisterDescriptor descriptor;

    if (floating_rs1_from_fpr(insn)) {
        descriptor.rs1 = RegisterOperandKind::Fpr;
    } else if (is_fmv_d_x(insn) || is_fmv_w_x(insn) ||
               is_fcvt_d_w(insn) || is_fcvt_d_wu(insn) || is_fcvt_d_l(insn) || is_fcvt_d_lu(insn) ||
               is_fcvt_s_w(insn) || is_fcvt_s_wu(insn) || is_fcvt_s_l(insn) || is_fcvt_s_lu(insn)) {
        descriptor.rs1 = RegisterOperandKind::Gpr;
    } else {
        switch (insn.opcode) {
        case 0x13:
        case 0x1B:
        case 0x33:
        case 0x3B:
        case 0x67:
        case 0x63:
        case 0x03:
        case 0x07:
        case 0x23:
        case 0x27:
        case 0x2F:
            descriptor.rs1 = RegisterOperandKind::Gpr;
            break;
        case 0x73:
            if (insn.funct3 == 1 || insn.funct3 == 2 || insn.funct3 == 3) {
                descriptor.rs1 = RegisterOperandKind::Gpr;
            }
            break;
        default:
            break;
        }
    }

    if (floating_rs2_from_fpr(insn)) {
        descriptor.rs2 = RegisterOperandKind::Fpr;
    } else if (is_standard_fp_store(insn)) {
        descriptor.rs2 = RegisterOperandKind::Fpr;
    } else {
        switch (insn.opcode) {
        case 0x33:
        case 0x3B:
        case 0x63:
        case 0x23:
            descriptor.rs2 = RegisterOperandKind::Gpr;
            break;
        case 0x2F:
            if (insn.funct5 != 0x02) {
                descriptor.rs2 = RegisterOperandKind::Gpr;
            }
            break;
        default:
            break;
        }
    }

    if (floating_rs3_from_fpr(insn)) {
        descriptor.rs3 = RegisterOperandKind::Fpr;
    }

    if (is_standard_fp_load(insn)) {
        descriptor.rd = RegisterOperandKind::Fpr;
    } else if (is_fmv_d_x(insn) || is_fmv_w_x(insn) || is_fmv_d(insn) || is_fneg_d(insn) ||
               is_fsgnj_d(insn) || is_fsgnjn_d(insn) || is_fsgnjx_d(insn) ||
               is_fsgnj_s(insn) || is_fsgnjn_s(insn) || is_fsgnjx_s(insn) ||
               is_fadd_s(insn) || is_fsub_s(insn) || is_fmul_s(insn) || is_fdiv_s(insn) ||
               is_fadd_d(insn) || is_fsub_d(insn) || is_fmul_d(insn) || is_fdiv_d(insn) ||
               is_fmax_s(insn) || is_fmin_s(insn) || is_fmax_d(insn) || is_fmin_d(insn) ||
               is_fsqrt_s(insn) || is_fsqrt_d(insn) ||
               is_fmadd_s(insn) || is_fmsub_s(insn) || is_fnmsub_s(insn) || is_fnmadd_s(insn) ||
               is_fmadd_d(insn) || is_fmsub_d(insn) || is_fnmsub_d(insn) || is_fnmadd_d(insn) ||
               is_fcvt_d_w(insn) || is_fcvt_d_wu(insn) || is_fcvt_d_l(insn) || is_fcvt_d_lu(insn) ||
               is_fcvt_s_w(insn) || is_fcvt_s_wu(insn) || is_fcvt_s_l(insn) || is_fcvt_s_lu(insn) ||
               is_fcvt_d_s(insn) || is_fcvt_s_d(insn)) {
        descriptor.rd = RegisterOperandKind::Fpr;
    } else if (insn.rd != 0 &&
               (is_fmv_x_d(insn) || is_fmv_x_w(insn) ||
                is_fcvt_w_d(insn) || is_fcvt_wu_d(insn) || is_fcvt_l_d(insn) || is_fcvt_lu_d(insn) ||
                is_fcvt_w_s(insn) || is_fcvt_wu_s(insn) || is_fcvt_l_s(insn) || is_fcvt_lu_s(insn) ||
                is_feq_s(insn) || is_flt_s(insn) || is_fle_s(insn) ||
                is_feq_d(insn) || is_flt_d(insn) || is_fle_d(insn) ||
                is_fclass_s(insn) || is_fclass_d(insn))) {
        descriptor.rd = RegisterOperandKind::Gpr;
    } else {
        switch (insn.opcode) {
        case 0x03:
        case 0x13:
        case 0x17:
        case 0x1B:
        case 0x33:
        case 0x37:
        case 0x3B:
        case 0x67:
        case 0x6F:
        case 0x2F:
            descriptor.rd = insn.rd != 0 ? RegisterOperandKind::Gpr : RegisterOperandKind::None;
            break;
        case 0x73:
            descriptor.rd = (insn.funct3 != 0 && insn.rd != 0) ? RegisterOperandKind::Gpr
                                                               : RegisterOperandKind::None;
            break;
        default:
            break;
        }
    }

    return descriptor;
}

InstructionMemoryDescriptor InstructionSemantics::describe_memory(const Insn& insn) {
    InstructionMemoryDescriptor descriptor;
    const auto set_load = [&](MemoryRequest::Target target, int size, bool sign_extend) {
        descriptor.valid = true;
        descriptor.kind = MemoryRequest::Kind::Load;
        descriptor.target = target;
        descriptor.size = size;
        descriptor.sign_extend = sign_extend;
    };
    const auto set_store = [&](MemoryRequest::Target target, int size) {
        descriptor.valid = true;
        descriptor.kind = MemoryRequest::Kind::Store;
        descriptor.target = target;
        descriptor.size = size;
        descriptor.commit_at_boundary = true;
        descriptor.non_speculative = true;
    };

    switch (insn.opcode) {
    case 0x03:
        switch (insn.funct3) {
        case 0:
            set_load(MemoryRequest::Target::Integer, 1, true);
            break;
        case 1:
            set_load(MemoryRequest::Target::Integer, 2, true);
            break;
        case 2:
            set_load(MemoryRequest::Target::Integer, 4, true);
            break;
        case 3:
            set_load(MemoryRequest::Target::Integer, 8, false);
            break;
        case 4:
            set_load(MemoryRequest::Target::Integer, 1, false);
            break;
        case 5:
            set_load(MemoryRequest::Target::Integer, 2, false);
            break;
        case 6:
            set_load(MemoryRequest::Target::Integer, 4, false);
            break;
        default:
            break;
        }
        break;
    case 0x07:
        if (is_standard_fp_load(insn)) {
            set_load(MemoryRequest::Target::Float, insn.funct3 == 3 ? 8 : 4, false);
        }
        break;
    case 0x23:
        switch (insn.funct3) {
        case 0:
            set_store(MemoryRequest::Target::Integer, 1);
            break;
        case 1:
            set_store(MemoryRequest::Target::Integer, 2);
            break;
        case 2:
            set_store(MemoryRequest::Target::Integer, 4);
            break;
        case 3:
            set_store(MemoryRequest::Target::Integer, 8);
            break;
        default:
            break;
        }
        break;
    case 0x27:
        if (is_standard_fp_store(insn)) {
            set_store(MemoryRequest::Target::Float, insn.funct3 == 3 ? 8 : 4);
        }
        break;
    default:
        break;
    }
    return descriptor;
}

InstructionAtomicDescriptor InstructionSemantics::describe_atomic(const Insn& insn) {
    InstructionAtomicDescriptor descriptor;
    if (insn.opcode != 0x2F || (insn.funct3 != 2 && insn.funct3 != 3)) {
        return descriptor;
    }

    descriptor.valid = true;
    descriptor.size = insn.funct3 == 3 ? 8 : 4;
    descriptor.aq = (insn.raw & (1U << 26)) != 0;
    descriptor.rl = (insn.raw & (1U << 25)) != 0;
    switch (insn.funct5) {
    case 0x02:
        descriptor.kind = AtomicRequest::Kind::LoadReserved;
        break;
    case 0x03:
        descriptor.kind = AtomicRequest::Kind::StoreConditional;
        break;
    case 0x01:
        descriptor.kind = AtomicRequest::Kind::Swap;
        break;
    case 0x00:
        descriptor.kind = AtomicRequest::Kind::Add;
        break;
    case 0x04:
        descriptor.kind = AtomicRequest::Kind::Xor;
        break;
    case 0x0C:
        descriptor.kind = AtomicRequest::Kind::And;
        break;
    case 0x08:
        descriptor.kind = AtomicRequest::Kind::Or;
        break;
    case 0x10:
        descriptor.kind = AtomicRequest::Kind::Min;
        break;
    case 0x14:
        descriptor.kind = AtomicRequest::Kind::Max;
        break;
    case 0x18:
        descriptor.kind = AtomicRequest::Kind::MinUnsigned;
        break;
    case 0x1C:
        descriptor.kind = AtomicRequest::Kind::MaxUnsigned;
        break;
    default:
        descriptor.valid = false;
        descriptor.size = 0;
        descriptor.kind = AtomicRequest::Kind::None;
        break;
    }
    return descriptor;
}

InsnEffects InstructionSemantics::execute(const Insn& insn, ExecutionContext& ctx) {
    SemanticInputs inputs;
    const InstructionRegisterDescriptor registers = describe_registers(insn);
    inputs.pc = ctx.core().pc();
    inputs.rs1v = registers.rs1 == RegisterOperandKind::Fpr ? ctx.core().read_fpr(insn.rs1)
                                                            : ctx.core().read_gpr(insn.rs1);
    inputs.rs2v = registers.rs2 == RegisterOperandKind::Fpr ? ctx.core().read_fpr(insn.rs2)
                                                            : ctx.core().read_gpr(insn.rs2);
    inputs.rs3v = registers.rs3 == RegisterOperandKind::Fpr ? ctx.core().read_fpr(insn.rs3) : 0;
    return execute(insn, ctx, inputs);
}

InsnEffects InstructionSemantics::execute(const Insn& insn, ExecutionContext& ctx, const SemanticInputs& inputs) {
    if (requires_enabled_floating_state(insn) &&
        (ctx.csr().read(CSR_MSTATUS, ctx.core()) & MSTATUS_FS_MASK) == MSTATUS_FS_OFF) {
        return illegal_instruction_effect(insn.raw);
    }

    switch (insn.opcode) {
    case 0x37:
    case 0x17:
    case 0x13:
    case 0x1B:
    case 0x33:
    case 0x3B:
    case 0x0F:
        return build_integer_effects(insn, inputs.rs1v, inputs.rs2v, insn.imm, inputs.pc);
    case 0x6F:
    case 0x67:
    case 0x63:
        return build_control_flow_effects(insn, inputs.rs1v, inputs.rs2v, insn.imm, inputs.pc);
    case 0x03:
    case 0x23:
        return build_memory_effects(insn, inputs.rs1v, inputs.rs2v, insn.imm);
    case 0x2F:
        return build_atomic_effects(insn, inputs.rs1v, inputs.rs2v);
    case 0x07:
    case 0x27:
        if (is_standard_fp_load(insn) || is_standard_fp_store(insn)) {
            return build_memory_effects(insn, inputs.rs1v, inputs.rs2v, insn.imm);
        }
        return build_vector_effects(insn, inputs.rs1v);
    case 0x53:
    case 0x43:
    case 0x47:
    case 0x4B:
    case 0x4F:
        return build_floating_effects(insn,
                                      inputs.rs1v,
                                      inputs.rs2v,
                                      inputs.rs3v,
                                      ctx.csr().read(CSR_FCSR, ctx.core()));
    case 0x57:
        return build_vector_effects(insn, inputs.rs1v);
    case 0x73:
        return build_system_effects(insn, ctx, inputs);
    default:
        return illegal_instruction_effect(insn.raw);
    }
}
