#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../cpu.h"
#include "../mem/bus.h"
#include "dbt_executable_cache.h"
#include "dbt_ir.h"
#include "dbt_runtime_invalidation.h"

struct DbtRuntimeHarnessResult {
    bool ok{false};
    uint64_t start_pc{0};
    uint64_t end_pc{0};
    DbtRejectKind reject_kind{DbtRejectKind::None};
    uint64_t reject_pc{0};
    uint32_t reject_raw{0};
    std::string reject_reason{};
    bool fallback_required{false};
    bool executed_host_code{false};
    bool used_executable_memory{false};
    bool used_executable_cache{false};
    bool inserted_executable_cache{false};
    bool cache_lookup{false};
    bool cache_hit{false};
    bool cache_miss{false};
    bool emitted_on_miss{false};
    bool executed_on_hit{false};
    bool executed_guest_code{false};
    bool mutated_cpu_state{false};
    bool differential_checked{false};
    bool differential_matched{false};
    uint64_t next_pc{0};
    uint64_t retired_instructions{0};
};

struct DbtRuntimeHarnessStats {
    uint64_t dispatches{0};
    uint64_t cache_lookups{0};
    uint64_t cache_hits{0};
    uint64_t cache_misses{0};
    uint64_t host_emits{0};
    uint64_t host_executes{0};
    uint64_t fallbacks{0};
    uint64_t invalidations{0};
    uint64_t stale_dispatches_prevented{0};
    uint64_t differential_checks{0};
    uint64_t differential_mismatches{0};
    uint64_t helper_executions{0};
    uint64_t reference_fallback_executions{0};
};

struct DbtRuntimeLoopRequest {
    uint64_t max_steps{0};
    bool enable_executable_cache{true};
    bool enable_helper_execution{true};
    bool enable_reference_fallback{true};
    bool apply_guest_store_invalidation{true};
};

enum class DbtRuntimeLoopStepKind : uint8_t {
    None,
    HostExecutable,
    HelperExecution,
    ReferenceFallback,
};

struct DbtRuntimeLoopStepResult {
    bool ok{false};
    DbtRuntimeLoopStepKind kind{DbtRuntimeLoopStepKind::None};
    uint64_t pc{0};
    uint64_t next_pc{0};
    bool cache_hit{false};
    bool cache_miss{false};
    bool emitted_on_miss{false};
    bool invalidated_after_store{false};
    bool stale_dispatch_prevented{false};
    std::string reason{};
};

struct DbtRuntimeLoopResult {
    bool ok{false};
    uint64_t steps_requested{0};
    uint64_t steps_executed{0};
    uint64_t host_executions{0};
    uint64_t helper_executions{0};
    uint64_t reference_fallbacks{0};
    uint64_t invalidations{0};
    uint64_t stale_dispatches_prevented{0};
    bool stopped_on_error{false};
    bool default_backend_enabled{false};
    std::string stop_reason{};
    DbtRuntimeHarnessStats stats{};
    std::vector<DbtRuntimeLoopStepResult> steps{};
};

DbtRuntimeHarnessResult run_dbt_runtime_harness_block(CPU& cpu,
                                                      Bus& bus,
                                                      uint64_t start_pc,
                                                      uint64_t end_pc);
DbtRuntimeHarnessResult run_dbt_runtime_harness_block_with_cache(
    CPU& cpu,
    Bus& bus,
    DbtExecutableCacheRuntime& cache,
    uint64_t start_pc,
    uint64_t end_pc);
bool dbt_runtime_harness_is_default_enabled();
DbtRuntimeLoopResult run_dbt_runtime_harness_loop(
    CPU& cpu,
    Bus& bus,
    DbtExecutableCacheRuntime& cache,
    const DbtRuntimeLoopRequest& request);
std::string format_dbt_runtime_harness_result(const DbtRuntimeHarnessResult& result);
void record_dbt_runtime_harness_result(DbtRuntimeHarnessStats& stats,
                                       const DbtRuntimeHarnessResult& result);
void record_dbt_runtime_invalidation_result(DbtRuntimeHarnessStats& stats,
                                            const DbtRuntimeInvalidationHookResult& result);
std::string format_dbt_runtime_harness_stats(const DbtRuntimeHarnessStats& stats);
const char* dbt_runtime_loop_step_kind_name(DbtRuntimeLoopStepKind kind);
std::string format_dbt_runtime_loop_result(const DbtRuntimeLoopResult& result);
