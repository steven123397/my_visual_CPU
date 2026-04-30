#include "dbt_host_emitter.h"

#include <cstring>
#include <sstream>
#include <vector>

namespace {

constexpr uint8_t kX86Ret = 0xc3U;

const char* bool_name(bool value) {
    return value ? "true" : "false";
}

std::string reject_reason_or_default(const std::string& reason, const char* fallback) {
    return reason.empty() ? fallback : reason;
}

DbtHostExecutable reject_from_lowering(const DbtIrLoweringResult& lowered) {
    return DbtHostExecutable{
        .ok = false,
        .start_pc = lowered.start_pc,
        .end_pc = lowered.end_pc,
        .reject_kind = lowered.reject_kind,
        .reject_pc = lowered.reject_pc,
        .reject_raw = lowered.reject_raw,
        .reject_reason = reject_reason_or_default(lowered.reject_reason, "lowering-rejected"),
    };
}

DbtHostExecutable reject_emission(const DbtIrLoweringResult& lowered,
                                  const DbtLoweredInstruction* instruction,
                                  const char* reason) {
    return DbtHostExecutable{
        .ok = false,
        .backend = DbtHostEmitterBackend::X86_64SysV,
        .start_pc = lowered.start_pc,
        .end_pc = lowered.end_pc,
        .reject_kind = DbtRejectKind::UnsupportedIr,
        .reject_pc = instruction != nullptr ? instruction->pc : lowered.start_pc,
        .reject_raw = instruction != nullptr ? instruction->raw : 0,
        .reject_reason = reason,
    };
}

void emit_u8(std::vector<uint8_t>& code, uint8_t value) {
    code.push_back(value);
}

void emit_u32(std::vector<uint8_t>& code, uint32_t value) {
    for (uint32_t shift = 0; shift < 32; shift += 8) {
        code.push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

void emit_u64(std::vector<uint8_t>& code, uint64_t value) {
    for (uint32_t shift = 0; shift < 64; shift += 8) {
        code.push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

uint32_t gpr_offset(uint8_t reg) {
    return static_cast<uint32_t>((reg & 0x1fU) * sizeof(uint64_t));
}

void emit_load_gpr_to_rax(std::vector<uint8_t>& code, uint8_t reg) {
    emit_u8(code, 0x48);
    emit_u8(code, 0x8b);
    emit_u8(code, 0x87);
    emit_u32(code, gpr_offset(reg));
}

void emit_load_gpr_to_rcx(std::vector<uint8_t>& code, uint8_t reg) {
    emit_u8(code, 0x48);
    emit_u8(code, 0x8b);
    emit_u8(code, 0x8f);
    emit_u32(code, gpr_offset(reg));
}

void emit_store_rax_to_gpr(std::vector<uint8_t>& code, uint8_t reg) {
    emit_u8(code, 0x48);
    emit_u8(code, 0x89);
    emit_u8(code, 0x87);
    emit_u32(code, gpr_offset(reg));
}

void emit_zero_x0(std::vector<uint8_t>& code) {
    emit_u8(code, 0x48);
    emit_u8(code, 0xc7);
    emit_u8(code, 0x87);
    emit_u32(code, 0);
    emit_u32(code, 0);
}

void emit_mov_rax_imm(std::vector<uint8_t>& code, uint64_t value) {
    emit_u8(code, 0x48);
    emit_u8(code, 0xb8);
    emit_u64(code, value);
}

void emit_mov_rcx_imm(std::vector<uint8_t>& code, uint64_t value) {
    emit_u8(code, 0x48);
    emit_u8(code, 0xb9);
    emit_u64(code, value);
}

bool emit_operand_to_rax(std::vector<uint8_t>& code,
                         const DbtLoweredInstruction& instruction,
                         DbtLoweredOperandKind kind) {
    switch (kind) {
    case DbtLoweredOperandKind::Gpr:
        emit_load_gpr_to_rax(code, instruction.rs1);
        return true;
    case DbtLoweredOperandKind::Immediate:
        emit_mov_rax_imm(code, static_cast<uint64_t>(instruction.imm));
        return true;
    case DbtLoweredOperandKind::Pc:
        emit_mov_rax_imm(code, instruction.pc);
        return true;
    case DbtLoweredOperandKind::None:
        return false;
    }
    return false;
}

bool emit_rhs_to_rcx(std::vector<uint8_t>& code,
                     const DbtLoweredInstruction& instruction) {
    switch (instruction.rhs_kind) {
    case DbtLoweredOperandKind::Gpr:
        emit_load_gpr_to_rcx(code, instruction.rs2);
        return true;
    case DbtLoweredOperandKind::Immediate:
        emit_mov_rcx_imm(code, static_cast<uint64_t>(instruction.imm));
        return true;
    case DbtLoweredOperandKind::Pc:
        emit_mov_rcx_imm(code, instruction.pc);
        return true;
    case DbtLoweredOperandKind::None:
        return false;
    }
    return false;
}

void emit_alu64_rax_rcx(std::vector<uint8_t>& code, DbtLoweredAluOp op) {
    emit_u8(code, 0x48);
    switch (op) {
    case DbtLoweredAluOp::Add:
        emit_u8(code, 0x01);
        emit_u8(code, 0xc8);
        return;
    case DbtLoweredAluOp::Sub:
        emit_u8(code, 0x29);
        emit_u8(code, 0xc8);
        return;
    case DbtLoweredAluOp::Xor:
        emit_u8(code, 0x31);
        emit_u8(code, 0xc8);
        return;
    case DbtLoweredAluOp::Or:
        emit_u8(code, 0x09);
        emit_u8(code, 0xc8);
        return;
    case DbtLoweredAluOp::And:
        emit_u8(code, 0x21);
        emit_u8(code, 0xc8);
        return;
    default:
        return;
    }
}

void emit_alu32_eax_ecx(std::vector<uint8_t>& code, DbtLoweredAluOp op) {
    switch (op) {
    case DbtLoweredAluOp::Add:
        emit_u8(code, 0x01);
        emit_u8(code, 0xc8);
        return;
    case DbtLoweredAluOp::Sub:
        emit_u8(code, 0x29);
        emit_u8(code, 0xc8);
        return;
    case DbtLoweredAluOp::Xor:
        emit_u8(code, 0x31);
        emit_u8(code, 0xc8);
        return;
    case DbtLoweredAluOp::Or:
        emit_u8(code, 0x09);
        emit_u8(code, 0xc8);
        return;
    case DbtLoweredAluOp::And:
        emit_u8(code, 0x21);
        emit_u8(code, 0xc8);
        return;
    default:
        return;
    }
}

void emit_shift_rax_imm(std::vector<uint8_t>& code, DbtLoweredAluOp op, uint8_t amount) {
    emit_u8(code, 0x48);
    emit_u8(code, 0xc1);
    switch (op) {
    case DbtLoweredAluOp::ShiftLeft:
        emit_u8(code, 0xe0);
        break;
    case DbtLoweredAluOp::ShiftRightLogical:
        emit_u8(code, 0xe8);
        break;
    case DbtLoweredAluOp::ShiftRightArithmetic:
        emit_u8(code, 0xf8);
        break;
    default:
        emit_u8(code, 0xe0);
        break;
    }
    emit_u8(code, amount);
}

void emit_shift_eax_imm(std::vector<uint8_t>& code, DbtLoweredAluOp op, uint8_t amount) {
    emit_u8(code, 0xc1);
    switch (op) {
    case DbtLoweredAluOp::ShiftLeft:
        emit_u8(code, 0xe0);
        break;
    case DbtLoweredAluOp::ShiftRightLogical:
        emit_u8(code, 0xe8);
        break;
    case DbtLoweredAluOp::ShiftRightArithmetic:
        emit_u8(code, 0xf8);
        break;
    default:
        emit_u8(code, 0xe0);
        break;
    }
    emit_u8(code, amount);
}

void emit_shift_rax_cl(std::vector<uint8_t>& code, DbtLoweredAluOp op) {
    emit_u8(code, 0x48);
    emit_u8(code, 0xd3);
    switch (op) {
    case DbtLoweredAluOp::ShiftLeft:
        emit_u8(code, 0xe0);
        break;
    case DbtLoweredAluOp::ShiftRightLogical:
        emit_u8(code, 0xe8);
        break;
    case DbtLoweredAluOp::ShiftRightArithmetic:
        emit_u8(code, 0xf8);
        break;
    default:
        emit_u8(code, 0xe0);
        break;
    }
}

void emit_shift_eax_cl(std::vector<uint8_t>& code, DbtLoweredAluOp op) {
    emit_u8(code, 0xd3);
    switch (op) {
    case DbtLoweredAluOp::ShiftLeft:
        emit_u8(code, 0xe0);
        break;
    case DbtLoweredAluOp::ShiftRightLogical:
        emit_u8(code, 0xe8);
        break;
    case DbtLoweredAluOp::ShiftRightArithmetic:
        emit_u8(code, 0xf8);
        break;
    default:
        emit_u8(code, 0xe0);
        break;
    }
}

void emit_movsxd_rax_eax(std::vector<uint8_t>& code) {
    emit_u8(code, 0x48);
    emit_u8(code, 0x63);
    emit_u8(code, 0xc0);
}

void emit_cmp_rax_rcx(std::vector<uint8_t>& code) {
    emit_u8(code, 0x48);
    emit_u8(code, 0x39);
    emit_u8(code, 0xc8);
}

void emit_setcc_to_rax(std::vector<uint8_t>& code, bool unsigned_compare) {
    emit_u8(code, 0x0f);
    emit_u8(code, unsigned_compare ? 0x92 : 0x9c);
    emit_u8(code, 0xc0);
    emit_u8(code, 0x0f);
    emit_u8(code, 0xb6);
    emit_u8(code, 0xc0);
}

bool is_binary_alu(DbtLoweredAluOp op) {
    switch (op) {
    case DbtLoweredAluOp::Add:
    case DbtLoweredAluOp::Sub:
    case DbtLoweredAluOp::Xor:
    case DbtLoweredAluOp::Or:
    case DbtLoweredAluOp::And:
        return true;
    default:
        return false;
    }
}

bool is_shift(DbtLoweredAluOp op) {
    switch (op) {
    case DbtLoweredAluOp::ShiftLeft:
    case DbtLoweredAluOp::ShiftRightLogical:
    case DbtLoweredAluOp::ShiftRightArithmetic:
        return true;
    default:
        return false;
    }
}

bool validate_instruction(const DbtIrLoweringResult& lowered,
                          const DbtLoweredInstruction& instruction,
                          const DbtHostExecutable*& rejection,
                          DbtHostExecutable& rejection_storage,
                          bool& seen_fallthrough) {
    if (seen_fallthrough) {
        rejection_storage = reject_emission(lowered, &instruction, "lowered-op-after-fallthrough");
        rejection = &rejection_storage;
        return false;
    }

    if (instruction.opcode == DbtLoweredOpcode::Fallthrough) {
        seen_fallthrough = true;
        return true;
    }
    if (instruction.opcode != DbtLoweredOpcode::Compute) {
        rejection_storage = reject_emission(lowered, &instruction, "unsupported-lowered-op");
        rejection = &rejection_storage;
        return false;
    }

    if (instruction.alu == DbtLoweredAluOp::Move) {
        if (instruction.lhs_kind == DbtLoweredOperandKind::None) {
            rejection_storage = reject_emission(lowered, &instruction, "unsupported-lowered-op");
            rejection = &rejection_storage;
            return false;
        }
        return true;
    }
    if (is_binary_alu(instruction.alu) ||
        instruction.alu == DbtLoweredAluOp::SetLessThan ||
        instruction.alu == DbtLoweredAluOp::SetLessThanUnsigned) {
        if (instruction.lhs_kind == DbtLoweredOperandKind::None ||
            instruction.rhs_kind == DbtLoweredOperandKind::None) {
            rejection_storage = reject_emission(lowered, &instruction, "unsupported-lowered-op");
            rejection = &rejection_storage;
            return false;
        }
        return true;
    }
    if (is_shift(instruction.alu)) {
        if (instruction.lhs_kind == DbtLoweredOperandKind::None ||
            (instruction.rhs_kind != DbtLoweredOperandKind::Immediate &&
             instruction.rhs_kind != DbtLoweredOperandKind::Gpr)) {
            rejection_storage = reject_emission(lowered, &instruction, "unsupported-lowered-op");
            rejection = &rejection_storage;
            return false;
        }
        return true;
    }

    rejection_storage = reject_emission(lowered, &instruction, "unsupported-lowered-op");
    rejection = &rejection_storage;
    return false;
}

bool validate_lowered(const DbtIrLoweringResult& lowered, DbtHostExecutable& rejection) {
    const DbtHostExecutable* rejection_ptr = nullptr;
    bool seen_fallthrough = false;
    for (const DbtLoweredInstruction& instruction : lowered.instructions) {
        if (!validate_instruction(lowered,
                                  instruction,
                                  rejection_ptr,
                                  rejection,
                                  seen_fallthrough)) {
            return false;
        }
    }
    if (!seen_fallthrough) {
        rejection = reject_emission(lowered, nullptr, "missing-fallthrough");
        return false;
    }
    return true;
}

bool emit_compute(std::vector<uint8_t>& code, const DbtLoweredInstruction& instruction) {
    if (instruction.alu == DbtLoweredAluOp::Move) {
        if (!emit_operand_to_rax(code, instruction, instruction.lhs_kind)) {
            return false;
        }
    } else if (is_binary_alu(instruction.alu)) {
        if (!emit_operand_to_rax(code, instruction, instruction.lhs_kind) ||
            !emit_rhs_to_rcx(code, instruction)) {
            return false;
        }
        if (instruction.width == DbtLoweredWidth::Word) {
            emit_alu32_eax_ecx(code, instruction.alu);
            emit_movsxd_rax_eax(code);
        } else {
            emit_alu64_rax_rcx(code, instruction.alu);
        }
    } else if (is_shift(instruction.alu)) {
        if (!emit_operand_to_rax(code, instruction, instruction.lhs_kind)) {
            return false;
        }
        if (instruction.rhs_kind == DbtLoweredOperandKind::Immediate) {
            const uint8_t amount = static_cast<uint8_t>(
                instruction.imm & (instruction.width == DbtLoweredWidth::Word ? 31 : 63));
            if (instruction.width == DbtLoweredWidth::Word) {
                emit_shift_eax_imm(code, instruction.alu, amount);
                emit_movsxd_rax_eax(code);
            } else {
                emit_shift_rax_imm(code, instruction.alu, amount);
            }
        } else {
            emit_rhs_to_rcx(code, instruction);
            if (instruction.width == DbtLoweredWidth::Word) {
                emit_shift_eax_cl(code, instruction.alu);
                emit_movsxd_rax_eax(code);
            } else {
                emit_shift_rax_cl(code, instruction.alu);
            }
        }
    } else if (instruction.alu == DbtLoweredAluOp::SetLessThan ||
               instruction.alu == DbtLoweredAluOp::SetLessThanUnsigned) {
        if (!emit_operand_to_rax(code, instruction, instruction.lhs_kind) ||
            !emit_rhs_to_rcx(code, instruction)) {
            return false;
        }
        emit_cmp_rax_rcx(code);
        emit_setcc_to_rax(code, instruction.alu == DbtLoweredAluOp::SetLessThanUnsigned);
    } else {
        return false;
    }

    if (instruction.writes_gpr) {
        emit_store_rax_to_gpr(code, instruction.rd);
    }
    return true;
}

bool emit_code(const DbtIrLoweringResult& lowered, std::vector<uint8_t>& code) {
    emit_zero_x0(code);
    for (const DbtLoweredInstruction& instruction : lowered.instructions) {
        if (instruction.opcode == DbtLoweredOpcode::Fallthrough) {
            emit_mov_rax_imm(code, instruction.next_pc);
            emit_u8(code, kX86Ret);
            continue;
        }
        if (!emit_compute(code, instruction)) {
            return false;
        }
    }
    return true;
}

}  // namespace

DbtHostExecutable emit_dbt_host_block(const DbtIrLoweringResult& lowered) {
    if (!lowered.ok) {
        return reject_from_lowering(lowered);
    }

#if !defined(__x86_64__)
    return DbtHostExecutable{
        .ok = false,
        .start_pc = lowered.start_pc,
        .end_pc = lowered.end_pc,
        .reject_kind = DbtRejectKind::UnsupportedIr,
        .reject_pc = lowered.start_pc,
        .reject_reason = "unsupported-host-backend",
    };
#else
    DbtHostExecutable validation_rejection{};
    if (!validate_lowered(lowered, validation_rejection)) {
        return validation_rejection;
    }

    std::vector<uint8_t> code;
    if (!emit_code(lowered, code)) {
        return reject_emission(lowered, nullptr, "host-emission-failed");
    }

    DbtExecutableMemoryBlock memory = allocate_dbt_executable_memory(code.size());
    if (!memory.allocated) {
        return DbtHostExecutable{
            .ok = false,
            .backend = DbtHostEmitterBackend::X86_64SysV,
            .start_pc = lowered.start_pc,
            .end_pc = lowered.end_pc,
            .reject_kind = DbtRejectKind::UnsupportedIr,
            .reject_pc = lowered.start_pc,
            .reject_reason = memory.error,
            .requested_executable_memory = true,
        };
    }

    DbtExecutableMemoryResult write =
        write_dbt_executable_memory(memory, 0, code.data(), code.size());
    if (!write.ok) {
        release_dbt_executable_memory(memory);
        return DbtHostExecutable{
            .ok = false,
            .backend = DbtHostEmitterBackend::X86_64SysV,
            .start_pc = lowered.start_pc,
            .end_pc = lowered.end_pc,
            .reject_kind = DbtRejectKind::UnsupportedIr,
            .reject_pc = lowered.start_pc,
            .reject_reason = write.error,
            .requested_executable_memory = true,
        };
    }

    DbtExecutableMemoryResult seal = seal_dbt_executable_memory(memory);
    if (!seal.ok) {
        release_dbt_executable_memory(memory);
        return DbtHostExecutable{
            .ok = false,
            .backend = DbtHostEmitterBackend::X86_64SysV,
            .start_pc = lowered.start_pc,
            .end_pc = lowered.end_pc,
            .reject_kind = DbtRejectKind::UnsupportedIr,
            .reject_pc = lowered.start_pc,
            .reject_reason = seal.error,
            .requested_executable_memory = true,
        };
    }

    return DbtHostExecutable{
        .ok = true,
        .backend = DbtHostEmitterBackend::X86_64SysV,
        .start_pc = lowered.start_pc,
        .end_pc = lowered.end_pc,
        .memory = memory,
        .code_size = code.size(),
        .generated_host_code = true,
        .requested_executable_memory = true,
        .executed_guest_code = false,
        .stats = DbtHostExecutableStats{
            .instructions_emitted = lowered.instructions.size(),
            .bytes_emitted = code.size(),
        },
    };
#endif
}

uint64_t execute_dbt_host_block(const DbtHostExecutable& executable,
                                uint64_t* gpr,
                                uint64_t pc) {
    if (!executable.ok || executable.memory.data == nullptr || gpr == nullptr) {
        return pc;
    }
    using HostBlockFn = uint64_t (*)(uint64_t*, uint64_t);
    HostBlockFn fn = reinterpret_cast<HostBlockFn>(executable.memory.data);
    return fn(gpr, pc);
}

void release_dbt_host_executable(DbtHostExecutable& executable) {
    if (executable.memory.allocated && executable.memory.data != nullptr) {
        release_dbt_executable_memory(executable.memory);
    }
}

const char* dbt_host_emitter_backend_name(DbtHostEmitterBackend backend) {
    switch (backend) {
    case DbtHostEmitterBackend::None:
        return "none";
    case DbtHostEmitterBackend::X86_64SysV:
        return "x86_64-sysv";
    }
    return "unknown";
}

std::string format_dbt_host_executable(const DbtHostExecutable& executable) {
    std::ostringstream out;
    out << "host-emitter:"
        << " ok=" << bool_name(executable.ok)
        << " backend=" << dbt_host_emitter_backend_name(executable.backend)
        << " start=0x" << std::hex << executable.start_pc
        << " end=0x" << executable.end_pc
        << std::dec
        << " code-size=" << executable.code_size
        << " emitted-instructions=" << executable.stats.instructions_emitted
        << " bytes=" << executable.stats.bytes_emitted
        << " host-code=" << bool_name(executable.generated_host_code)
        << " exec-mem=" << bool_name(executable.requested_executable_memory)
        << " guest-exec=" << bool_name(executable.executed_guest_code)
        << " reject=" << dbt_reject_kind_name(executable.reject_kind)
        << " reason=" << (executable.reject_reason.empty() ? "none" : executable.reject_reason);
    return out.str();
}
