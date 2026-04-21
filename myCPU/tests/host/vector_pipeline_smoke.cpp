#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

#include "../../include/platform_mmio.h"
#include "../../src/cpu.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

extern "C" {
#include "../../src/decode.h"
}

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint32_t kOlderScalarRaw = 0x00100093U;  // addi x1, x0, 1

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

uint32_t encode_rtype(uint8_t opcode,
                      uint8_t funct3,
                      uint8_t funct7,
                      uint8_t rd,
                      uint8_t rs1,
                      uint8_t rs2) {
    return (static_cast<uint32_t>(funct7) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

uint32_t encode_vv(uint8_t funct7, uint8_t vd, uint8_t vs1, uint8_t vs2) {
    return encode_rtype(0x57, 0, funct7, vd, vs1, vs2);
}

std::array<uint8_t, VectorState::kRegBytes> pack_i16x4(int16_t a,
                                                       int16_t b,
                                                       int16_t c,
                                                       int16_t d) {
    const std::array<int16_t, 4> values{a, b, c, d};
    std::array<uint8_t, VectorState::kRegBytes> out{};
    for (size_t i = 0; i < values.size(); ++i) {
        const uint16_t bits = static_cast<uint16_t>(values[i]);
        out[i * 2] = static_cast<uint8_t>(bits & 0xFFU);
        out[i * 2 + 1] = static_cast<uint8_t>((bits >> 8) & 0xFFU);
    }
    return out;
}

StageSlot make_vector_slot(uint64_t sequence_id, uint64_t pc, uint32_t raw, RobIndex rob_index) {
    StageSlot slot{};
    slot.valid = true;
    slot.sequence_id.value = sequence_id;
    slot.pc = pc;
    slot.raw = raw;
    decode(raw, &slot.insn);
    slot.insn.raw = raw;
    slot.rob_index = rob_index;
    return slot;
}

bool test_vector_alu_executes_before_rob_head_commit() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    VectorState& vector = cpu.core().vector();
    if (!expect(vector.set_config(2, 4), "vector config should accept sew=2 vl=4")) {
        return false;
    }
    const auto lhs = pack_i16x4(1, -2, 10, 7);
    const auto rhs = pack_i16x4(3, 5, -4, 7);
    const auto expected = pack_i16x4(4, 3, 6, 14);
    vector.write_reg(1, lhs);
    vector.write_reg(2, rhs);

    PipelineBackend backend(cpu, bus);
    PipelineCoreState& state = backend.testing_state();
    const RobIndex older_scalar = state.rob().allocate({
        .sequence_id = 1,
        .pc = kEntry,
        .raw = kOlderScalarRaw,
        .arch_rd = 1,
        .phys_rd = 1,
        .previous_phys_rd = 1,
    });
    (void)older_scalar;

    const uint32_t kVaddRaw = encode_vv(0x00, 3, 1, 2);
    const RobIndex vector_rob = state.rob().allocate({
        .sequence_id = 2,
        .pc = kEntry + 4,
        .raw = kVaddRaw,
    });
    state.id_ex.slot = make_vector_slot(2, kEntry + 4, kVaddRaw, vector_rob);

    backend.step();

    const auto first_snapshot = backend.debug_snapshot();
    if (!expect(first_snapshot.pipeline.stall_reason != "serializing_system_wait_for_rob_head",
                "vector ALU should no longer stall as serializing system when an older scalar ROB head is pending")) {
        return false;
    }
    if (!expect(state.mem_wb.slot.valid && state.mem_wb.slot.raw == kVaddRaw,
                "vector ALU should reach the pipeline completion slot before becoming ROB head")) {
        return false;
    }
    if (!expect(vector.read_reg(3) != expected,
                "vector ALU should not update architected vector state before commit")) {
        return false;
    }

    state.rob().mark_ready({.value = 1}, {});
    backend.step();
    if (!expect(vector.read_reg(3) != expected,
                "older scalar commit should still not make vector ALU visible architecturally in the same cycle")) {
        return false;
    }

    backend.step();
    if (!expect(vector.read_reg(3) == expected,
                "vector ALU should update architected vector state once it reaches commit")) {
        return false;
    }
    return true;
}

bool test_younger_vector_alu_waits_for_older_vector_state() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    VectorState& vector = cpu.core().vector();
    if (!expect(vector.set_config(2, 4), "vector config should accept sew=2 vl=4")) {
        return false;
    }
    vector.write_reg(1, pack_i16x4(1, 2, 3, 4));
    vector.write_reg(2, pack_i16x4(5, 6, 7, 8));

    PipelineBackend backend(cpu, bus);
    PipelineCoreState& state = backend.testing_state();
    const uint32_t kOlderVaddRaw = encode_vv(0x00, 3, 1, 2);
    state.rob().allocate({
        .sequence_id = 1,
        .pc = kEntry,
        .raw = kOlderVaddRaw,
    });

    const uint32_t kYoungerVmaxRaw = encode_vv(0x21, 4, 3, 2);
    const RobIndex younger_rob = state.rob().allocate({
        .sequence_id = 2,
        .pc = kEntry + 4,
        .raw = kYoungerVmaxRaw,
    });
    state.id_ex.slot = make_vector_slot(2, kEntry + 4, kYoungerVmaxRaw, younger_rob);

    backend.step();

    const auto snapshot = backend.debug_snapshot();
    if (!expect(snapshot.pipeline.stall_reason == "vector_state_busy",
                "younger vector ALU should wait while an older vector state update is still pending")) {
        return false;
    }
    if (!expect(state.id_ex.slot.valid && state.id_ex.slot.raw == kYoungerVmaxRaw,
                "vector-state hazard should keep the younger vector ALU in ID/EX")) {
        return false;
    }
    if (!expect(!state.mem_wb.slot.valid,
                "vector-state hazard should prevent the younger vector ALU from completing early")) {
        return false;
    }
    if (!expect(vector.read_reg(4) == std::array<uint8_t, VectorState::kRegBytes>{},
                "vector-state hazard should leave younger destination register architecturally unchanged")) {
        return false;
    }
    return true;
}

bool test_ready_vector_dependency_executes_across_scalar_rob_head() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    VectorState& vector = cpu.core().vector();
    if (!expect(vector.set_config(2, 4), "vector config should accept sew=2 vl=4")) {
        return false;
    }

    const auto lhs = pack_i16x4(1, -2, 10, 7);
    const auto rhs = pack_i16x4(3, 5, -4, 7);
    const auto bias = pack_i16x4(2, 9, 5, 20);
    const auto expected_add = pack_i16x4(4, 3, 6, 14);
    const auto expected_vmax = pack_i16x4(4, 9, 6, 20);
    vector.write_reg(1, lhs);
    vector.write_reg(2, rhs);
    vector.write_reg(5, bias);

    PipelineBackend backend(cpu, bus);
    PipelineCoreState& state = backend.testing_state();
    const RobIndex scalar_head = state.rob().allocate({
        .sequence_id = 1,
        .pc = kEntry,
        .raw = kOlderScalarRaw,
        .arch_rd = 1,
        .phys_rd = 1,
        .previous_phys_rd = 1,
    });

    const uint32_t kOlderVaddRaw = encode_vv(0x00, 3, 1, 2);
    const RobIndex older_vadd = state.rob().allocate({
        .sequence_id = 2,
        .pc = kEntry + 4,
        .raw = kOlderVaddRaw,
    });
    InsnEffects older_effects;
    older_effects.vector.kind = VectorRequest::Kind::Add;
    older_effects.vector.vd = 3;
    older_effects.vector.vs1 = 1;
    older_effects.vector.vs2 = 2;
    older_effects.vector.result_valid = true;
    older_effects.vector.result = expected_add;
    state.rob().mark_ready(older_vadd, {
        .effects = older_effects,
    });

    const uint32_t kYoungerVmaxRaw = encode_vv(0x21, 4, 3, 5);
    const RobIndex younger_rob = state.rob().allocate({
        .sequence_id = 3,
        .pc = kEntry + 8,
        .raw = kYoungerVmaxRaw,
    });
    state.id_ex.slot = make_vector_slot(3, kEntry + 8, kYoungerVmaxRaw, younger_rob);

    backend.step();

    const auto first_snapshot = backend.debug_snapshot();
    if (!expect(first_snapshot.pipeline.stall_reason != "vector_state_busy",
                "a ready older vector producer should not block a direct dependent vector ALU behind an older scalar ROB head")) {
        return false;
    }
    if (!expect(state.mem_wb.slot.valid && state.mem_wb.slot.raw == kYoungerVmaxRaw,
                "dependent vector ALU should complete once its older vector source is already materialized")) {
        return false;
    }
    if (!expect(vector.read_reg(3) != expected_add && vector.read_reg(4) != expected_vmax,
                "dependency forwarding should stay speculative until ordered commit")) {
        return false;
    }

    state.rob().mark_ready(scalar_head, {
        .value_ready = true,
        .value = 1,
    });

    backend.step();
    if (!expect(vector.read_reg(3) != expected_add,
                "committing the older scalar head should still not retire the older vector producer in the same cycle")) {
        return false;
    }

    backend.step();
    if (!expect(vector.read_reg(3) == expected_add && vector.read_reg(4) != expected_vmax,
                "older vector producer should commit before the dependent consumer becomes architecturally visible")) {
        return false;
    }

    backend.step();
    if (!expect(vector.read_reg(4) == expected_vmax,
                "dependent vector ALU should eventually commit the forwarded-chain result")) {
        return false;
    }
    const auto snapshot = backend.debug_snapshot();
    if (!expect(!snapshot.profile.hot_paths.empty(),
                "vector dependency chain should expose hot-path profile entries once it commits")) {
        return false;
    }
    if (!expect(snapshot.profile.total_retirements >= 3,
                "vector dependency chain should contribute committed retirements to the execution profile")) {
        return false;
    }
    return true;
}

bool test_pending_vector_config_still_blocks_mixed_chain() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    VectorState& vector = cpu.core().vector();
    if (!expect(vector.set_config(2, 4), "vector config should accept sew=2 vl=4")) {
        return false;
    }
    vector.write_reg(1, pack_i16x4(2, -3, 4, 5));
    vector.write_reg(2, pack_i16x4(6, 7, -8, 9));
    vector.write_reg(5, pack_i16x4(1, 1, 1, 1));

    PipelineBackend backend(cpu, bus);
    PipelineCoreState& state = backend.testing_state();
    state.rob().allocate({
        .sequence_id = 1,
        .pc = kEntry,
        .raw = kOlderScalarRaw,
        .arch_rd = 1,
        .phys_rd = 1,
        .previous_phys_rd = 1,
    });

    const uint32_t kOlderVdotRaw = encode_vv(0x22, 3, 1, 2);
    const RobIndex older_vdot = state.rob().allocate({
        .sequence_id = 2,
        .pc = kEntry + 4,
        .raw = kOlderVdotRaw,
    });
    InsnEffects vdot_effects;
    vdot_effects.vector.kind = VectorRequest::Kind::Dot;
    vdot_effects.vector.vd = 3;
    vdot_effects.vector.vs1 = 1;
    vdot_effects.vector.vs2 = 2;
    vdot_effects.vector.result_valid = true;
    vdot_effects.vector.result = pack_i16x4(0, 0, 0, 0);
    state.rob().mark_ready(older_vdot, {
        .effects = vdot_effects,
    });

    const uint32_t kOlderVsetcfgRaw = encode_rtype(0x57, 7, 0x47, 0, 0, 0);
    const RobIndex older_vsetcfg = state.rob().allocate({
        .sequence_id = 3,
        .pc = kEntry + 8,
        .raw = kOlderVsetcfgRaw,
    });
    InsnEffects vsetcfg_effects;
    vsetcfg_effects.vector.kind = VectorRequest::Kind::SetConfig;
    vsetcfg_effects.vector.sew_bytes = 8;
    vsetcfg_effects.vector.vl = 1;
    state.rob().mark_ready(older_vsetcfg, {
        .effects = vsetcfg_effects,
    });

    const uint32_t kYoungerVmaxRaw = encode_vv(0x21, 4, 3, 5);
    const RobIndex younger_rob = state.rob().allocate({
        .sequence_id = 4,
        .pc = kEntry + 12,
        .raw = kYoungerVmaxRaw,
    });
    state.id_ex.slot = make_vector_slot(4, kEntry + 12, kYoungerVmaxRaw, younger_rob);

    backend.step();

    const auto snapshot = backend.debug_snapshot();
    if (!expect(snapshot.pipeline.stall_reason == "vector_state_busy",
                "a pending older vector config update should still block a younger vector dependency chain")) {
        return false;
    }
    if (!expect(state.id_ex.slot.valid && state.id_ex.slot.raw == kYoungerVmaxRaw,
                "mixed chain should stay blocked in ID/EX while vector config is not yet committed")) {
        return false;
    }
    if (!expect(!state.mem_wb.slot.valid,
                "pending vector config should prevent the younger mixed chain from completing")) {
        return false;
    }
    return true;
}

}  // namespace

int main() {
    if (!test_vector_alu_executes_before_rob_head_commit()) {
        return 1;
    }
    if (!test_younger_vector_alu_waits_for_older_vector_state()) {
        return 1;
    }
    if (!test_ready_vector_dependency_executes_across_scalar_rob_head()) {
        return 1;
    }
    if (!test_pending_vector_config_still_blocks_mixed_chain()) {
        return 1;
    }
    std::puts("vector_pipeline_smoke: PASS");
    return 0;
}
