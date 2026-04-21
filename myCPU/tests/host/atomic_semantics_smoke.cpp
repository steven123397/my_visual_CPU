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
constexpr uint32_t kWfi = 0x10500073U;

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

constexpr uint32_t encode_store(int32_t imm,
                                uint32_t rs2,
                                uint32_t rs1,
                                uint32_t funct3) {
    const uint32_t uimm = static_cast<uint32_t>(imm) & 0xfffU;
    return ((uimm >> 5) << 25) |
           (rs2 << 20) |
           (rs1 << 15) |
           (funct3 << 12) |
           ((uimm & 0x1fU) << 7) |
           kOpcodeStore;
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

}  // namespace

int main() {
    return test_atomic_opcode_supported() &&
                   test_lr_sc_round_trip() &&
                   test_second_lr_replaces_reservation() &&
                   test_overlapping_store_invalidates_reservation() &&
                   test_commit_boundary_store_invalidates_reservation() &&
                   test_amoadd_and_amomaxu64() &&
                   test_misa_mhartid_and_wfi()
               ? 0
               : 1;
}
