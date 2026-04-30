#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/cpu.h"
#include "../../src/exec/execution_profile.h"
#include "../../src/exec/dbt_helper_replay.h"
#include "../../src/exec/dbt_jit_engine.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint32_t kAddiX1One = 0x00100093U;    // addi x1, x0, 1
constexpr uint32_t kAddiX2X1Two = 0x00208113U;  // addi x2, x1, 2
constexpr uint32_t kLwX1FromX0 = 0x00002083U;   // lw x1, 0(x0)
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;   // jal x0, 8

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

bool test_jit_skeleton_lowers_inline_blocks_and_reuses_metadata_cache() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    cpu.core().write_gpr(1, 41);
    cpu.core().write_gpr(2, 42);
    write32(ram, kEntry + 0, kAddiX1One);
    write32(ram, kEntry + 4, kAddiX2X1Two);

    DbtJitEngineDryRun engine;
    const uint64_t before_x1 = cpu.core().read_gpr(1);
    const uint64_t before_x2 = cpu.core().read_gpr(2);
    const uint64_t before_pc = cpu.core().pc();

    const DbtJitDryRunResult first = engine.dry_run_block(cpu, bus, kEntry, kEntry + 4);
    const DbtJitEngineDryRunStats first_stats = engine.stats();
    const DbtJitDryRunResult second = engine.dry_run_block(cpu, bus, kEntry, kEntry + 4);
    const DbtJitEngineDryRunStats second_stats = engine.stats();
    engine.clear();
    const DbtJitEngineDryRunStats cleared_stats = engine.stats();
    const DbtBlockCacheStats cleared_cache_stats = engine.cache().stats();

    return expect(first.ok, "first inline dry-run should be ready for lowered execution") &&
           expect(first.action == DbtJitDryRunAction::LoweredReady,
                  "first inline dry-run should lower to backend-neutral ops") &&
           expect(first.planned && first.translated && first.lowered,
                  "first inline dry-run should plan, translate, and lower") &&
           expect(!first.used_cache && first.inserted_cache,
                  "first inline dry-run should miss cache and insert metadata") &&
           expect(first.lowering.ok && first.lowering.instructions.size() == 3,
                  "first inline dry-run should expose lowered ops plus fallthrough") &&
           expect(!first.generated_host_code && !first.requested_executable_memory &&
                      !first.executed_guest_code,
                  "JIT skeleton must not generate host code or execute guest code") &&
           expect(cpu.core().read_gpr(1) == before_x1 &&
                      cpu.core().read_gpr(2) == before_x2 &&
                      cpu.core().pc() == before_pc,
                  "JIT dry-run should not mutate CPU state") &&
           expect(first_stats.requests == 1 && first_stats.cache_misses == 1 &&
                      first_stats.cache_insertions == 1 &&
                      first_stats.lowered_ready == 1,
                  "first dry-run stats should record miss, insert, and lowered-ready") &&
           expect(second.ok && second.action == DbtJitDryRunAction::LoweredReady,
                  "second inline dry-run should also be lowered-ready") &&
           expect(second.used_cache && !second.planned && !second.translated &&
                      second.lowered && !second.inserted_cache,
                  "second inline dry-run should lower cached metadata without replanning") &&
           expect(second_stats.requests == 2 && second_stats.cache_hits == 1 &&
                      second_stats.cache_misses == 1 &&
                      second_stats.translations == 1 &&
                      second_stats.lowerings == 2,
                  "second dry-run stats should record cache hit without new translation") &&
           expect(cleared_stats.requests == 0 && cleared_cache_stats.entries == 0 &&
                      cleared_cache_stats.lookups == 0,
                  "JIT dry-run clear should reset engine stats and metadata cache stats");
}

bool test_jit_skeleton_bridges_helper_required_units_to_reference_fallback() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry, kLwX1FromX0);

    DbtJitEngineDryRun engine;
    const DbtJitDryRunResult result = engine.dry_run_block(cpu, bus, kEntry, kEntry);
    const DbtJitEngineDryRunStats stats = engine.stats();

    return expect(!result.ok, "helper-required unit should not be lowered-ready") &&
           expect(result.action == DbtJitDryRunAction::HelperBridgeRequired,
                  "memory helper should produce helper bridge action") &&
           expect(result.fallback_to_reference,
                  "helper bridge should still fall back to reference execution") &&
           expect(result.translation.reject_kind == DbtRejectKind::MemoryLoad,
                  "helper bridge should preserve memory-load reject kind") &&
           expect(result.helper_replay.ok &&
                      result.helper_replay.kind == DbtHelperReplayKind::ScalarMemoryLoad,
                  "helper bridge should expose scalar load replay contract") &&
           expect(!result.inserted_cache && engine.cache().size() == 0,
                  "helper-required units should not enter metadata cache") &&
           expect(stats.helper_bridges == 1 && stats.reference_fallbacks == 0,
                  "helper bridge should be counted separately from plain fallback");
}

bool test_jit_skeleton_falls_back_without_helper_metadata_for_control_flow() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry, kJalX0Skip8);

    DbtJitEngineDryRun engine;
    const DbtJitDryRunResult result = engine.dry_run_block(cpu, bus, kEntry, kEntry);

    return expect(!result.ok, "control-flow unit should not be lowered-ready") &&
           expect(result.action == DbtJitDryRunAction::ReferenceFallback,
                  "control-flow unit should choose reference fallback") &&
           expect(result.fallback_to_reference,
                  "control-flow fallback should explicitly select reference path") &&
           expect(result.translation.reject_kind == DbtRejectKind::ControlFlow,
                  "control-flow fallback should preserve reject kind") &&
           expect(!result.helper_replay.ok,
                  "control-flow fallback should not expose helper replay metadata") &&
           expect(result.lowering.instructions.empty(),
                  "fallback should not expose lowered prefix ops") &&
           expect(dbt_jit_dry_run_action_name(DbtJitDryRunAction::ReferenceFallback) ==
                      std::string("reference-fallback"),
                      "JIT dry-run action names should be stable");
}

bool test_jit_skeleton_dispatches_from_profile_hot_path_candidates() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0x10, kAddiX1One);
    write32(ram, kEntry + 0x14, kAddiX2X1Two);

    ExecutionProfileSnapshot profile;
    profile.hot_paths = {
        ExecutionHotPathEntry{
            .start_pc = kEntry,
            .end_pc = kEntry,
            .executions = 1,
            .retired_instructions = 1,
        },
        ExecutionHotPathEntry{
            .start_pc = kEntry + 0x10,
            .end_pc = kEntry + 0x14,
            .executions = 4,
            .retired_instructions = 8,
        },
    };

    DbtJitEngineDryRun engine;
    const DbtJitDryRunResult result = engine.dry_run_hot_path(cpu, bus, profile);
    const DbtJitEngineDryRunStats stats = engine.stats();

    return expect(result.ok, "profile-selected hot path should become lowered-ready") &&
           expect(result.source == DbtJitDryRunSource::HotPathProfile,
                  "profile-selected dispatch should expose hot-path source") &&
           expect(result.action == DbtJitDryRunAction::LoweredReady,
                  "profile-selected hot path should lower to backend-neutral ops") &&
           expect(result.start_pc == kEntry + 0x10 && result.end_pc == kEntry + 0x14,
                  "profile dispatch should select the hottest translatable range") &&
           expect(result.block_plan.candidate_executions == 4 &&
                      result.block_plan.candidate_retired_instructions == 8,
                  "profile dispatch should preserve candidate evidence") &&
           expect(result.planned && result.translated && result.lowered && result.inserted_cache,
                  "profile dispatch should plan, translate, lower, and cache metadata") &&
           expect(stats.requests == 1 && stats.profile_requests == 1 &&
                      stats.lowered_ready == 1,
                  "profile dispatch stats should record request and lowered-ready outcome") &&
           expect(dbt_jit_dry_run_source_name(DbtJitDryRunSource::HotPathProfile) ==
                      std::string("hot-path-profile"),
                  "JIT dry-run source names should be stable");
}

bool test_jit_skeleton_reports_profile_without_candidates_as_reference_fallback() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    ExecutionProfileSnapshot profile;
    DbtJitEngineDryRun engine;
    const DbtJitDryRunResult result = engine.dry_run_hot_path(cpu, bus, profile);
    const DbtJitEngineDryRunStats stats = engine.stats();

    return expect(!result.ok, "empty profile should not become lowered-ready") &&
           expect(result.source == DbtJitDryRunSource::HotPathProfile,
                  "empty-profile dispatch should expose hot-path source") &&
           expect(result.action == DbtJitDryRunAction::ReferenceFallback,
                  "empty profile should select reference fallback") &&
           expect(result.fallback_to_reference,
                  "empty profile fallback should explicitly select reference path") &&
           expect(result.translation.reject_kind == DbtRejectKind::FallbackRequired,
                  "empty profile should preserve typed fallback reject kind") &&
           expect(result.translation.reject_reason == "no-hot-paths",
                  "empty profile should preserve stable fallback reason") &&
           expect(stats.profile_requests == 1 && stats.profile_no_candidates == 1 &&
                      stats.reference_fallbacks == 1,
                  "empty profile stats should record no-candidate fallback");
}

bool test_jit_skeleton_serializes_dispatch_result_for_probe_visibility() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    write32(ram, kEntry + 0x20, kAddiX1One);
    write32(ram, kEntry + 0x24, kAddiX2X1Two);

    ExecutionProfileSnapshot profile;
    profile.hot_paths = {
        ExecutionHotPathEntry{
            .start_pc = kEntry + 0x20,
            .end_pc = kEntry + 0x24,
            .executions = 5,
            .retired_instructions = 10,
        },
    };

    DbtJitEngineDryRun engine;
    const DbtJitDryRunResult lowered = engine.dry_run_hot_path(cpu, bus, profile);
    const DbtJitDryRunSummary lowered_summary = summarize_dbt_jit_dry_run(lowered);
    const std::string lowered_line = format_dbt_jit_dry_run_summary(lowered_summary);

    write32(ram, kEntry + 0x40, kLwX1FromX0);
    const DbtJitDryRunResult helper = engine.dry_run_block(cpu, bus, kEntry + 0x40, kEntry + 0x40);
    const DbtJitDryRunSummary helper_summary = summarize_dbt_jit_dry_run(helper);
    const std::string helper_line = format_dbt_jit_dry_run_summary(helper_summary);

    return expect(lowered_summary.ok,
                  "lowered dispatch summary should preserve ok state") &&
           expect(lowered_summary.source == "hot-path-profile" &&
                      lowered_summary.action == "lowered-ready",
                  "lowered dispatch summary should expose stable source and action") &&
           expect(lowered_summary.start_pc == "0x80000020" &&
                      lowered_summary.end_pc == "0x80000024",
                  "lowered dispatch summary should expose hex block range") &&
           expect(lowered_summary.cache_state == "miss-inserted",
                  "lowered dispatch summary should expose cache state") &&
           expect(lowered_summary.lowered_instruction_count == 3,
                  "lowered dispatch summary should expose lowered op count") &&
           expect(lowered_summary.candidate_executions == 5 &&
                      lowered_summary.candidate_retired_instructions == 10,
                  "lowered dispatch summary should preserve hot-path evidence") &&
           expect(lowered_summary.reject_kind == "none" &&
                      lowered_summary.helper_replay_kind == "none",
                  "lowered dispatch summary should not report reject or helper metadata") &&
           expect(!lowered_summary.generated_host_code &&
                      !lowered_summary.requested_executable_memory &&
                      !lowered_summary.executed_guest_code,
                  "lowered dispatch summary should preserve no-execution flags") &&
           expect(lowered_line.find("jit-dispatch: source=hot-path-profile action=lowered-ready") !=
                      std::string::npos,
                  "lowered dispatch summary line should expose stable prefix") &&
           expect(lowered_line.find("host-code=false exec-mem=false guest-exec=false") !=
                      std::string::npos,
                  "lowered dispatch summary line should expose non-execution flags") &&
           expect(!helper_summary.ok &&
                      helper_summary.action == "helper-bridge-required" &&
                      helper_summary.source == "explicit-block",
                  "helper dispatch summary should expose helper bridge action") &&
           expect(helper_summary.reject_kind == "memory-load" &&
                      helper_summary.reject_reason == "helper-required",
                  "helper dispatch summary should expose typed reject metadata") &&
           expect(helper_summary.helper_replay_kind == "scalar-memory-load",
                  "helper dispatch summary should expose helper replay kind") &&
           expect(helper_line.find("helper=scalar-memory-load") != std::string::npos,
                  "helper dispatch summary line should expose helper replay kind");
}

}  // namespace

int main() {
    if (!test_jit_skeleton_lowers_inline_blocks_and_reuses_metadata_cache()) {
        return 1;
    }
    if (!test_jit_skeleton_bridges_helper_required_units_to_reference_fallback()) {
        return 1;
    }
    if (!test_jit_skeleton_falls_back_without_helper_metadata_for_control_flow()) {
        return 1;
    }
    if (!test_jit_skeleton_dispatches_from_profile_hot_path_candidates()) {
        return 1;
    }
    if (!test_jit_skeleton_reports_profile_without_candidates_as_reference_fallback()) {
        return 1;
    }
    if (!test_jit_skeleton_serializes_dispatch_result_for_probe_visibility()) {
        return 1;
    }
    std::puts("dbt_jit_engine_smoke: PASS");
    return 0;
}
