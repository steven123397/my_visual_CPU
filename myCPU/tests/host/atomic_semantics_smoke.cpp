#include <cstdio>
#include <cstdint>

#include "../../src/cpu.h"
#include "../../src/exec/pipeline_commit_boundary.h"
#include "../../src/isa/execution_context.h"
#include "../../src/isa/atomic_contract.h"
#include "../../src/isa/instruction_semantics.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../src/platform/address_map.h"

extern "C" {
#include "../../src/decode.h"
}

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint64_t kWordSlotA = MEM_BASE + 0x100;
constexpr uint64_t kWordSlotB = MEM_BASE + 0x104;
constexpr uint64_t kWordSlotC = MEM_BASE + 0x108;
constexpr uint64_t kDwordSlot = MEM_BASE + 0x110;

constexpr uint32_t kOpcodeSystem = 0x73U;
constexpr uint32_t kOpcodeStore = 0x23U;
constexpr uint32_t kOpcodeAmo = 0x2fU;
constexpr uint32_t kEcall = 0x00000073U;
constexpr uint32_t kWfi = 0x10500073U;
constexpr uint32_t CSR_HPMCOUNTER3 = 0xC03U;
constexpr uint32_t CSR_MHPMCOUNTER3 = 0xB03U;
constexpr uint32_t CSR_MHPMEVENT3 = 0x323U;
constexpr uint32_t CSR_MTVEC = 0x305U;
constexpr uint32_t CSR_MEPC = 0x341U;
constexpr uint32_t CSR_MCAUSE = 0x342U;
constexpr uint64_t kMstatusFsInitial = MSTATUS_FS_INITIAL;
constexpr uint64_t kMstatusFsDirty = MSTATUS_FS_DIRTY;
constexpr uint64_t kMstatusSd = 1ULL << 63;
constexpr uint32_t kFrcsrT0 = 0x003022f3U;
constexpr uint32_t kFmvXDFromF1 = 0xe20082d3U;

constexpr uint32_t encode_amo(uint32_t funct5,
                              bool aq,
                              bool rl,
                              uint32_t rs2,
                              uint32_t rs1,
                              uint32_t funct3,
                              uint32_t rd) {
    return (funct5 << 27) |
           (static_cast<uint32_t>(aq) << 26) |
           (static_cast<uint32_t>(rl) << 25) |
           (rs2 << 20) |
           (rs1 << 15) |
           (funct3 << 12) |
           (rd << 7) |
           kOpcodeAmo;
}

constexpr uint32_t encode_csrr(uint32_t rd, uint32_t csr) {
    return (csr << 20) | (0x2U << 12) | (rd << 7) | kOpcodeSystem;
}

constexpr uint32_t encode_csrrc(uint32_t rd, uint32_t csr, uint32_t rs1) {
    return (csr << 20) | (rs1 << 15) | (0x3U << 12) | (rd << 7) | kOpcodeSystem;
}

constexpr uint32_t encode_csrrs(uint32_t rd, uint32_t csr, uint32_t rs1) {
    return (csr << 20) | (rs1 << 15) | (0x2U << 12) | (rd << 7) | kOpcodeSystem;
}

constexpr uint32_t encode_load(int32_t imm,
                               uint32_t rd,
                               uint32_t rs1,
                               uint32_t funct3,
                               uint32_t opcode = 0x03U) {
    const uint32_t uimm = static_cast<uint32_t>(imm) & 0xfffU;
    return (uimm << 20) |
           (rs1 << 15) |
           (funct3 << 12) |
           (rd << 7) |
           opcode;
}

constexpr uint32_t encode_store(int32_t imm,
                                uint32_t rs2,
                                uint32_t rs1,
                                uint32_t funct3,
                                uint32_t opcode = kOpcodeStore) {
    const uint32_t uimm = static_cast<uint32_t>(imm) & 0xfffU;
    return ((uimm >> 5) << 25) |
           (rs2 << 20) |
           (rs1 << 15) |
           (funct3 << 12) |
           ((uimm & 0x1fU) << 7) |
           opcode;
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

void write_program(Ram& ram, const uint32_t* words, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        ram.store(kEntry + static_cast<uint64_t>(i) * 4, words[i], 4);
    }
}

bool test_atomic_opcode_supported() {
    Insn insn{};
    decode(encode_amo(0x02, false, false, 0, 10, 0x2, 5), &insn);
    return expect(InstructionSemantics::supports(insn),
                  "A-extension opcode should be routed through InstructionSemantics");
}

bool test_lr_sc_round_trip() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    ram.store(kWordSlotA, 0x81234567U, 4);
    const uint32_t program[] = {
        encode_amo(0x02, false, false, 0, 10, 0x2, 5),
        encode_amo(0x03, false, false, 11, 10, 0x2, 6),
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu.core().write_gpr(10, kWordSlotA);
    cpu.core().write_gpr(11, 0x12345678U);

    cpu_step(cpu, bus);
    cpu_step(cpu, bus);

    return expect(cpu.core().read_gpr(5) == 0xffffffff81234567ULL,
                  "lr.w should sign-extend the loaded value") &&
           expect(cpu.core().read_gpr(6) == 0,
                  "sc.w should report success after matching lr.w") &&
           expect(ram.load(kWordSlotA, 4) == 0x12345678U,
                  "successful sc.w should update memory");
}

bool test_second_lr_replaces_reservation() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    ram.store(kWordSlotA, 1, 4);
    ram.store(kWordSlotB, 2, 4);
    const uint32_t program[] = {
        encode_amo(0x02, false, false, 0, 10, 0x2, 5),
        encode_amo(0x02, false, false, 0, 12, 0x2, 6),
        encode_amo(0x03, false, false, 11, 10, 0x2, 7),
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu.core().write_gpr(10, kWordSlotA);
    cpu.core().write_gpr(11, 0x55aa55aaU);
    cpu.core().write_gpr(12, kWordSlotB);

    cpu_step(cpu, bus);
    cpu_step(cpu, bus);
    cpu_step(cpu, bus);

    return expect(cpu.core().read_gpr(7) == 1,
                  "sc.w should fail after a later lr.w replaces the reservation") &&
           expect(ram.load(kWordSlotA, 4) == 1,
                  "failed sc.w must not modify memory");
}

bool test_overlapping_store_invalidates_reservation() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    ram.store(kWordSlotA, 0x11223344U, 4);
    const uint32_t program[] = {
        encode_amo(0x02, false, false, 0, 10, 0x2, 5),
        encode_store(0, 11, 10, 0x2),
        encode_amo(0x03, false, false, 12, 10, 0x2, 6),
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu.core().write_gpr(10, kWordSlotA);
    cpu.core().write_gpr(11, 0xaabbccddu);
    cpu.core().write_gpr(12, 0x55667788U);

    cpu_step(cpu, bus);
    cpu_step(cpu, bus);
    cpu_step(cpu, bus);

    return expect(cpu.core().read_gpr(6) == 1,
                  "sc.w should fail after an overlapping ordinary store") &&
           expect(ram.load(kWordSlotA, 4) == 0xaabbccddu,
                  "ordinary store should remain visible when the later sc.w fails");
}

bool test_commit_boundary_store_invalidates_reservation() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    ram.store(kWordSlotA, 0x11223344U, 4);
    cpu.trap().set_reservation(kWordSlotA, 4);

    InsnEffects store_effects;
    store_effects.mem.kind = MemoryRequest::Kind::Store;
    store_effects.mem.addr = kWordSlotA;
    store_effects.mem.store_value = 0xaabbccddu;
    store_effects.mem.size = 4;
    store_effects.retired = true;

    const CommitBoundaryResult store_result =
        apply_commit_boundary(cpu,
                              bus,
                              CommitBoundaryInput{
                                  .pc = kEntry,
                                  .next_pc = kEntry + 4,
                                  .effects = store_effects,
                              });
    if (!expect(store_result.retired && !store_result.trap_taken,
                "ordinary store commit should retire cleanly while testing reservation invalidation")) {
        return false;
    }

    const AtomicApplyResult sc_result =
        apply_atomic_request(cpu,
                             bus,
                             AtomicRequest{
                                 .kind = AtomicRequest::Kind::StoreConditional,
                                 .addr = kWordSlotA,
                                 .store_value = 0x55667788U,
                                 .rd = 6,
                                 .size = 4,
                             });

    return expect(sc_result.ok, "store-conditional apply should complete after an ordinary store commit") &&
           expect(sc_result.rd_write.enable && sc_result.rd_write.value == 1,
                  "store-conditional should fail after commit-boundary ordinary store invalidates the reservation") &&
           expect(ram.load(kWordSlotA, 4) == 0xaabbccddu,
                  "commit-boundary ordinary store should remain visible when the later store-conditional fails");
}

bool test_amoadd_and_amomaxu64() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    ram.store(kWordSlotC, 7, 4);
    ram.store(kDwordSlot, 5, 8);
    const uint32_t program[] = {
        encode_amo(0x00, false, false, 11, 10, 0x2, 5),
        encode_amo(0x1c, false, false, 13, 12, 0x3, 6),
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu.core().write_gpr(10, kWordSlotC);
    cpu.core().write_gpr(11, 5);
    cpu.core().write_gpr(12, kDwordSlot);
    cpu.core().write_gpr(13, 8);

    cpu_step(cpu, bus);
    cpu_step(cpu, bus);

    return expect(cpu.core().read_gpr(5) == 7,
                  "amoadd.w should return the prior memory value") &&
           expect(ram.load(kWordSlotC, 4) == 12,
                  "amoadd.w should write the 32-bit sum") &&
           expect(cpu.core().read_gpr(6) == 5,
                  "amomaxu.d should return the prior 64-bit memory value") &&
           expect(ram.load(kDwordSlot, 8) == 8,
                  "amomaxu.d should keep the larger unsigned operand");
}

bool test_misa_mhartid_and_wfi() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const uint32_t program[] = {
        encode_csrr(5, CSR_MISA),
        encode_csrr(6, 0xf14U),
        kWfi,
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu_step(cpu, bus);
    cpu_step(cpu, bus);
    cpu_step(cpu, bus);

    return expect((cpu.core().read_gpr(5) & 1ULL) != 0,
                  "misa should advertise the A extension") &&
           expect(cpu.core().read_gpr(6) == 0,
                  "mhartid should read as hart 0") &&
           expect(cpu.core().pc() == kEntry + 12,
                  "wfi should retire as a legal hint in machine mode");
}

bool test_fcsr_frm_aliases() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const uint32_t kCsrrA0Frm = 0x00202573U;   // frrm a0
    const uint32_t kCsrrA1Fcsr = 0x003025f3U;  // frcsr a1
    const uint32_t kCsrwFrmA2 = 0x00261573U;   // fsrm a0? actually csrrw a0, frm, a2 shape not needed for this path
    (void)kCsrwFrmA2;

    cpu.csr().write(CSR_MSTATUS, kMstatusFsInitial, cpu.core());
    cpu.csr().write(CSR_FRM, 3, cpu.core());
    cpu.csr().write(CSR_FFLAGS, 0x1b, cpu.core());

    const uint32_t program[] = {
        kCsrrA0Frm,
        kCsrrA1Fcsr,
        kWfi,
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu_step(cpu, bus);
    cpu_step(cpu, bus);
    cpu_step(cpu, bus);

    return expect(cpu.core().read_gpr(10) == 3,
                  "frrm should read the current rounding mode") &&
           expect(cpu.core().read_gpr(11) == ((3ULL << 5) | 0x1bULL),
                  "frcsr should read the combined fcsr alias value") &&
           expect(cpu.csr().read(CSR_FRM, cpu.core()) == 3,
                  "frm alias should preserve the programmed rounding mode") &&
           expect(cpu.csr().read(CSR_FFLAGS, cpu.core()) == 0x1b,
                  "fflags alias should preserve the programmed exception flags");
}

bool test_fcsr_csr_write_marks_fs_dirty() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const uint32_t kFscsrX5 = (CSR_FCSR << 20) | (5U << 15) | (1U << 12) | kOpcodeSystem;
    cpu.csr().write(CSR_MSTATUS, kMstatusFsInitial, cpu.core());
    cpu.core().write_gpr(5, 2ULL << 5);

    const uint32_t program[] = {
        kFscsrX5,
        kWfi,
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu_step(cpu, bus);

    return expect(cpu.csr().read(CSR_FCSR, cpu.core()) == (2ULL << 5),
                  "fscsr should write the combined fcsr alias") &&
           expect((cpu.csr().read(CSR_MSTATUS, cpu.core()) & kMstatusFsDirty) == kMstatusFsDirty,
                  "fcsr CSR writes should mark floating-point state dirty for Linux fstate save/restore") &&
           expect((cpu.csr().read(CSR_SSTATUS, cpu.core()) & kMstatusFsDirty) == kMstatusFsDirty,
                  "sstatus should expose dirty FS after fcsr writes for Linux trap entry") &&
           expect((cpu.csr().read(CSR_SSTATUS, cpu.core()) & kMstatusSd) == kMstatusSd,
                  "sstatus should expose SD when FS is dirty so Linux can save fstate across scheduling");
}

bool test_sstatus_csrrc_exposes_dirty_fs_to_linux_trap_entry() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    cpu.csr().write(CSR_MSTATUS, kMstatusFsDirty, cpu.core());
    cpu.core().write_gpr(6, kMstatusFsDirty);

    const uint32_t program[] = {
        encode_csrrc(5, CSR_SSTATUS, 6),
        kWfi,
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu_step(cpu, bus);

    const uint64_t old_sstatus = cpu.core().read_gpr(5);
    const uint64_t after_sstatus = cpu.csr().read(CSR_SSTATUS, cpu.core());
    return expect((old_sstatus & kMstatusFsDirty) == kMstatusFsDirty,
                  "csrrc sstatus must return old FS=DIRTY to Linux PT_STATUS") &&
           expect((old_sstatus & kMstatusSd) == kMstatusSd,
                  "csrrc sstatus must return old SD to Linux PT_STATUS") &&
           expect((after_sstatus & kMstatusFsDirty) == 0,
                  "csrrc sstatus should clear FS bits after saving old PT_STATUS") &&
           expect((after_sstatus & kMstatusSd) == 0,
                  "clearing FS through sstatus should clear SD summary");
}

bool test_linux_trap_entry_status_preserves_sd_for_fstate_switch() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    constexpr uint64_t kLinuxTrapClearMask = MSTATUS_SUM | kMstatusFsDirty | MSTATUS_VS_DIRTY;
    cpu.csr().write(CSR_MSTATUS, kMstatusFsDirty | MSTATUS_SUM | MSTATUS_VS_CLEAN, cpu.core());
    cpu.core().write_gpr(6, kLinuxTrapClearMask);

    const uint32_t program[] = {
        encode_csrrc(5, CSR_SSTATUS, 6),
        kWfi,
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu_step(cpu, bus);

    const uint64_t saved_status = cpu.core().read_gpr(5);
    const uint64_t after_sstatus = cpu.csr().read(CSR_SSTATUS, cpu.core());
    return expect((saved_status & kMstatusFsDirty) == kMstatusFsDirty,
                  "Linux trap entry csrrc should save old FS=DIRTY in pt_regs.status") &&
           expect((saved_status & kMstatusSd) == kMstatusSd,
                  "Linux switch_to should see SR_SD in pt_regs.status and call fstate_save") &&
           expect((after_sstatus & kMstatusFsDirty) == 0,
                  "Linux trap entry should clear live sstatus.FS after saving pt_regs.status") &&
           expect((after_sstatus & MSTATUS_VS_MASK) == 0,
                  "Linux trap entry should clear live sstatus.VS after saving pt_regs.status") &&
           expect((after_sstatus & kMstatusSd) == 0,
                  "clearing FS/VS should clear live SD summary");
}

bool test_linux_fstate_save_restore_sequence_preserves_fcsr_and_fpr() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const uint64_t save_area = MEM_BASE + 0x200;
    const uint64_t marker = 0x0123456789abcdefULL;
    const uint64_t fcsr = (2ULL << 5) | 0x1ULL;
    const uint32_t kCsrsSstatusT1 = encode_csrrs(0, CSR_SSTATUS, 6);
    const uint32_t kCsrcSstatusT1 = encode_csrrc(0, CSR_SSTATUS, 6);
    const uint32_t kFrcsrT0 = encode_csrrs(5, CSR_FCSR, 0);
    const uint32_t kFscsrT0 = (CSR_FCSR << 20) | (5U << 15) | (1U << 12) | kOpcodeSystem;
    const uint32_t kFsdF1A0_8 = encode_store(8, 1, 10, 0x3, 0x27U);
    const uint32_t kFldF1A0_8 = encode_load(8, 1, 10, 0x3, 0x07U);
    const uint32_t kSwT0A0_256 = encode_store(256, 5, 10, 0x2);
    const uint32_t kLwT0A0_256 = encode_load(256, 5, 10, 0x2);

    cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
    cpu.core().write_gpr(6, kMstatusFsDirty);
    cpu.core().write_gpr(10, save_area);
    cpu.core().write_fpr(1, marker);
    cpu.csr().write(CSR_FCSR, fcsr, cpu.core());
    cpu.csr().write(CSR_MSTATUS, kMstatusFsDirty, cpu.core());

    const uint32_t save_program[] = {
        kCsrsSstatusT1,
        kFrcsrT0,
        kFsdF1A0_8,
        kSwT0A0_256,
        kCsrcSstatusT1,
        kWfi,
    };
    write_program(ram, save_program, sizeof(save_program) / sizeof(save_program[0]));
    for (size_t i = 0; i < sizeof(save_program) / sizeof(save_program[0]); ++i) {
        cpu_step(cpu, bus);
    }

    if (!expect(ram.load(save_area + 8, 8) == marker,
                "Linux fstate save sequence should store the current raw f1 value")) {
        return false;
    }
    if (!expect((ram.load(save_area + 256, 4) & 0xffULL) == fcsr,
                "Linux fstate save sequence should store the current fcsr value")) {
        return false;
    }

    cpu.core().set_pc(kEntry);
    cpu.core().write_fpr(1, 0);
    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.csr().write(CSR_MSTATUS, 0, cpu.core());

    const uint32_t restore_program[] = {
        kLwT0A0_256,
        kCsrsSstatusT1,
        kFldF1A0_8,
        kFscsrT0,
        kCsrcSstatusT1,
        kWfi,
    };
    write_program(ram, restore_program, sizeof(restore_program) / sizeof(restore_program[0]));
    for (size_t i = 0; i < sizeof(restore_program) / sizeof(restore_program[0]); ++i) {
        cpu_step(cpu, bus);
    }

    return expect(cpu.core().read_fpr(1) == marker,
                  "Linux fstate restore sequence should restore the raw f1 value") &&
           expect(cpu.csr().read(CSR_FCSR, cpu.core()) == fcsr,
                  "Linux fstate restore sequence should restore fcsr") &&
           expect((cpu.csr().read(CSR_SSTATUS, cpu.core()) & kMstatusFsDirty) == 0,
                  "Linux fstate restore sequence should leave FS cleared after csrc sstatus, SR_FS");
}

bool test_user_fp_access_with_fs_off_traps_for_linux_lazy_restore() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const uint64_t trap_vector = MEM_BASE + 0x200;
    cpu.core().set_privilege_mode(PrivilegeMode::User);
    cpu.core().write_fpr(1, 0x0123456789abcdefULL);
    cpu.csr().write(CSR_FCSR, (2ULL << 5) | 0x1ULL, cpu.core());
    cpu.csr().write(CSR_MSTATUS, 0, cpu.core());
    cpu.csr().write(CSR_MTVEC, trap_vector, cpu.core());

    const uint32_t program[] = {
        kFrcsrT0,
        kWfi,
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu_step(cpu, bus);

    if (!expect(cpu.core().privilege_mode() == PrivilegeMode::Machine,
                "U-mode frcsr with FS=Off should trap for Linux lazy fstate restore")) {
        return false;
    }
    if (!expect(cpu.core().pc() == trap_vector,
                "U-mode frcsr with FS=Off should redirect to mtvec")) {
        return false;
    }
    if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 2,
                "U-mode frcsr with FS=Off should report illegal-instruction cause")) {
        return false;
    }
    if (!expect(cpu.core().read_gpr(5) == 0,
                "U-mode frcsr with FS=Off must not read stale fcsr before Linux restores fstate")) {
        return false;
    }

    cpu_init(cpu, kEntry);
    cpu.core().set_privilege_mode(PrivilegeMode::User);
    cpu.core().write_fpr(1, 0x0123456789abcdefULL);
    cpu.csr().write(CSR_MSTATUS, 0, cpu.core());
    cpu.csr().write(CSR_MTVEC, trap_vector, cpu.core());

    const uint32_t fpr_program[] = {
        kFmvXDFromF1,
        kWfi,
    };
    write_program(ram, fpr_program, sizeof(fpr_program) / sizeof(fpr_program[0]));

    cpu_step(cpu, bus);

    return expect(cpu.core().privilege_mode() == PrivilegeMode::Machine,
                  "U-mode fmv.x.d with FS=Off should trap for Linux lazy fstate restore") &&
           expect(cpu.core().pc() == trap_vector,
                  "U-mode fmv.x.d with FS=Off should redirect to mtvec") &&
           expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 2,
                  "U-mode fmv.x.d with FS=Off should report illegal-instruction cause") &&
           expect(cpu.core().read_gpr(5) == 0,
                  "U-mode fmv.x.d with FS=Off must not read stale FPR state before Linux restores fstate");
}

bool test_hpm_counter_aliases() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    cpu.csr().write(CSR_MHPMCOUNTER3, 0x1234ULL, cpu.core());
    cpu.csr().write(CSR_MHPMEVENT3, 0x55aaULL, cpu.core());

    const uint32_t program[] = {
        encode_csrr(5, CSR_HPMCOUNTER3),
        encode_csrr(6, CSR_MHPMCOUNTER3),
        kWfi,
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu_step(cpu, bus);
    cpu_step(cpu, bus);
    cpu_step(cpu, bus);

    return expect(cpu.core().read_gpr(5) == 0x1234ULL,
                  "hpmcounter3 should mirror mhpmcounter3 rather than trap or diverge from the machine counter value") &&
           expect(cpu.core().read_gpr(6) == 0x1234ULL,
                  "mhpmcounter3 should preserve the programmed machine counter value") &&
           expect(cpu.csr().read(CSR_MHPMEVENT3, cpu.core()) == 0x55aaULL,
                  "mhpmevent3 should preserve the programmed event selector value") &&
           expect(cpu.core().pc() == kEntry + 12,
                  "hpmcounter3 aliases should not trap or block forward progress");
}

bool test_mstatus_sstatus_fs_aliases() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    cpu.csr().write(CSR_MSTATUS, kMstatusFsInitial, cpu.core());

    const uint32_t program[] = {
        encode_csrr(5, CSR_SSTATUS),
        encode_csrr(6, CSR_MSTATUS),
        kWfi,
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu_step(cpu, bus);
    cpu_step(cpu, bus);
    cpu_step(cpu, bus);

    return expect((cpu.core().read_gpr(5) & kMstatusFsInitial) == kMstatusFsInitial,
                  "sstatus should expose the FS field alias from mstatus") &&
           expect((cpu.core().read_gpr(6) & kMstatusFsInitial) == kMstatusFsInitial,
                  "mstatus should preserve the programmed FS field") &&
           expect((cpu.core().read_gpr(5) & kMstatusSd) == 0,
                  "sstatus should not raise SD when FS is only initial") &&
           expect((cpu.core().read_gpr(6) & kMstatusSd) == 0,
                  "mstatus should not raise SD when FS is only initial");
}

bool test_user_exit_ecall_traps_instead_of_halting() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const uint64_t trap_vector = MEM_BASE + 0x200;
    cpu.core().set_privilege_mode(PrivilegeMode::User);
    cpu.core().write_gpr(17, 93);
    cpu.csr().write(CSR_MTVEC, trap_vector, cpu.core());

    const uint32_t program[] = {
        kEcall,
        kWfi,
    };
    write_program(ram, program, sizeof(program) / sizeof(program[0]));

    cpu_step(cpu, bus);

    return expect(!cpu.core().halted(),
                  "U-mode Linux exit ecall must trap to the kernel instead of halting the simulator") &&
           expect(cpu.core().privilege_mode() == PrivilegeMode::Machine,
                  "U-mode ecall should enter M-mode when not delegated") &&
           expect(cpu.core().pc() == trap_vector,
                  "U-mode ecall should redirect to mtvec") &&
           expect(cpu.csr().read(CSR_MEPC, cpu.core()) == kEntry,
                  "U-mode ecall should preserve the faulting user PC in mepc") &&
           expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 8,
                  "U-mode ecall should report cause 8, not a host halt");
}

}  // namespace

int main() {
    return test_atomic_opcode_supported() &&
                   test_lr_sc_round_trip() &&
                   test_second_lr_replaces_reservation() &&
                   test_overlapping_store_invalidates_reservation() &&
                   test_commit_boundary_store_invalidates_reservation() &&
                   test_amoadd_and_amomaxu64() &&
                   test_misa_mhartid_and_wfi() &&
                   test_fcsr_frm_aliases() &&
                   test_fcsr_csr_write_marks_fs_dirty() &&
                   test_sstatus_csrrc_exposes_dirty_fs_to_linux_trap_entry() &&
                   test_linux_trap_entry_status_preserves_sd_for_fstate_switch() &&
                   test_linux_fstate_save_restore_sequence_preserves_fcsr_and_fpr() &&
                   test_user_fp_access_with_fs_off_traps_for_linux_lazy_restore() &&
                   test_hpm_counter_aliases() &&
                   test_mstatus_sstatus_fs_aliases() &&
                   test_user_exit_ecall_traps_instead_of_halting()
               ? 0
               : 1;
}
