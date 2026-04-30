#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_jit_engine.h"
#include "../../src/exec/dbt_reference_fallback.h"
#include "../../src/exec/dbt_reference_fallback_execution.h"
#include "../../src/exec/dbt_runtime_dispatch.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kPc = MEM_BASE;
constexpr uint64_t kData = MEM_BASE + 0x2000;
constexpr uint32_t kRawJal = 0x0080006fU;
constexpr uint32_t kRawLoad = 0x00002083U;
constexpr uint32_t kLwX5FromX6 = 0x00032283U;  // lw x5, 0(x6)
constexpr uint32_t kAddiX1One = 0x00100093U;   // addi x1, x0, 1

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

DbtReferenceFallbackPlan plain_plan(const char* reason = "fallback-required",
                                    DbtRejectKind reject = DbtRejectKind::ControlFlow) {
    return DbtReferenceFallbackPlan{
        .ok = true,
        .kind = DbtReferenceFallbackKind::ReferenceStep,
        .source = DbtJitDryRunSource::ExplicitBlock,
        .pc = kPc,
        .end_pc = kPc,
        .reference_step_count = 1,
        .reference_backend = "functional-reference",
        .reference_step_required = true,
        .dry_run_only = true,
        .reject_kind = reject,
        .reject_pc = kPc,
        .reject_raw = kRawJal,
        .reject_reason = reason,
    };
}

DbtReferenceFallbackPlan helper_plan() {
    return DbtReferenceFallbackPlan{
        .ok = true,
        .kind = DbtReferenceFallbackKind::HelperBridgeReferenceStep,
        .source = DbtJitDryRunSource::ExplicitBlock,
        .pc = kPc,
        .end_pc = kPc,
        .reference_step_count = 1,
        .reference_backend = "functional-reference",
        .reference_step_required = true,
        .requires_helper_bridge = true,
        .dry_run_only = true,
        .helper_may_trap = true,
        .reject_kind = DbtRejectKind::MemoryLoad,
        .reject_pc = kPc,
        .reject_raw = kRawLoad,
        .reject_reason = "helper-required",
        .helper_replay_kind = DbtHelperReplayKind::ScalarMemoryLoad,
    };
}

void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

DbtReferenceFallbackExecutionRequest request_from_dispatch(CPU& cpu,
                                                           Bus& bus,
                                                           uint64_t pc,
                                                           uint64_t end_pc) {
    DbtJitEngineDryRun engine;
    const DbtRuntimeDispatchContract contract =
        plan_dbt_runtime_dispatch_contract(engine.dry_run_block(cpu, bus, pc, end_pc));
    return plan_dbt_reference_fallback_execution(
        plan_dbt_reference_fallback_step(contract));
}

bool test_reference_fallback_execution_rejects_invalid_plan() {
    const DbtReferenceFallbackExecutionRequest request =
        plan_dbt_reference_fallback_execution(DbtReferenceFallbackPlan{});
    const std::string line = format_dbt_reference_fallback_execution_request(request);

    return expect(!request.ok, "fallback execution should reject invalid fallback plan") &&
           expect(request.kind == DbtReferenceFallbackExecutionKind::None,
                  "invalid fallback plan should not expose execution kind") &&
           expect(request.reject_reason == "invalid-reference-fallback-plan",
                  "invalid fallback plan should expose stable reason") &&
           expect(request.dry_run_only && !request.executed_reference_step &&
                      !request.mutates_cpu_state,
                  "invalid fallback execution request should remain non-executing") &&
           expect(line.find("fallback-exec: kind=none") != std::string::npos,
                  "fallback execution formatter should expose none kind");
}

bool test_reference_fallback_execution_classifies_plain_and_jit_miss() {
    const DbtReferenceFallbackExecutionRequest fallback =
        plan_dbt_reference_fallback_execution(plain_plan());
    const DbtReferenceFallbackExecutionRequest miss =
        plan_dbt_reference_fallback_execution(
            plain_plan("jit-cache-miss", DbtRejectKind::PlanRejected));

    return expect(fallback.ok &&
                      fallback.kind == DbtReferenceFallbackExecutionKind::ReferenceStep,
                  "plain fallback should request a reference step") &&
           expect(fallback.reference_backend == "functional-reference" &&
                      fallback.reference_step_count == 1 &&
                      fallback.will_call_reference_backend,
                  "plain fallback should preserve reference backend contract") &&
           expect(!fallback.executed_reference_step && !fallback.mutates_cpu_state &&
                      !fallback.executed_guest_code,
                  "plain fallback execution request should not execute reference step") &&
           expect(miss.ok &&
                      miss.kind == DbtReferenceFallbackExecutionKind::JitMissReferenceStep,
                  "JIT miss fallback should be separately classified") &&
           expect(miss.reject_reason == "jit-cache-miss" &&
                      miss.reject_kind == DbtRejectKind::PlanRejected,
                  "JIT miss fallback should preserve reject metadata");
}

bool test_reference_fallback_execution_classifies_helper_and_fault_placeholders() {
    const DbtReferenceFallbackExecutionRequest helper =
        plan_dbt_reference_fallback_execution(helper_plan());
    const DbtReferenceFallbackExecutionRequest fault =
        plan_dbt_reference_fallback_execution(
            plain_plan("fetch-fault", DbtRejectKind::FetchFault));

    return expect(helper.ok &&
                      helper.kind == DbtReferenceFallbackExecutionKind::HelperBridgeReferenceStep,
                  "helper fallback should request helper bridge reference step") &&
           expect(helper.requires_helper_bridge &&
                      helper.helper_replay_kind == DbtHelperReplayKind::ScalarMemoryLoad &&
                      helper.helper_may_trap,
                  "helper fallback execution should preserve helper metadata") &&
           expect(helper.will_call_reference_backend && !helper.executed_helper &&
                      !helper.executed_reference_step,
                  "helper fallback execution request should not execute helper or reference") &&
           expect(fault.ok &&
                      fault.kind == DbtReferenceFallbackExecutionKind::TrapOrFaultReferenceStep,
                  "trap/fault fallback should be separately classified") &&
           expect(fault.reject_kind == DbtRejectKind::FetchFault &&
                      fault.trap_or_fault_placeholder,
                  "trap/fault fallback should expose placeholder flag") &&
           expect(dbt_reference_fallback_execution_kind_name(
                      DbtReferenceFallbackExecutionKind::HelperBridgeReferenceStep) ==
                      std::string("helper-bridge-reference-step"),
                  "fallback execution kind name should be stable");
}

bool test_reference_fallback_execution_runs_plain_reference_step_opt_in() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kPc);
    write32(ram, kPc, kRawJal);

    DbtReferenceFallbackExecutionRequest request =
        request_from_dispatch(cpu, bus, kPc, kPc);
    const DbtReferenceFallbackExecutionResult result =
        execute_dbt_reference_fallback(cpu, bus, request);
    const std::string line = format_dbt_reference_fallback_execution_result(result);

    return expect(request.ok &&
                      request.kind == DbtReferenceFallbackExecutionKind::ReferenceStep,
                  "control-flow reject should produce executable reference-step request") &&
           expect(result.ok && result.executed_reference_step &&
                      result.executed_guest_code && result.mutated_cpu_state,
                  "opt-in fallback execution should call functional reference step") &&
           expect(result.reference_steps_executed == 1 && result.next_pc == kPc + 8,
                  "plain fallback execution should commit the reference redirect") &&
           expect(cpu.core().pc() == kPc + 8 && cpu.core().instret() == 1,
                  "plain fallback execution should mutate CPU through reference backend") &&
           expect(line.find("fallback-exec-result: kind=reference-step") != std::string::npos,
                  "fallback execution result formatter should expose stable prefix") &&
           expect(line.find("dry-run-only=false") != std::string::npos,
                  "fallback execution result should clear dry-run-only flag");
}

bool test_reference_fallback_execution_runs_helper_required_memory_load_via_reference() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kPc);
    write32(ram, kPc, kLwX5FromX6);
    ram.store(kData, 0x11223344, 4);
    cpu.core().write_gpr(6, kData);

    DbtReferenceFallbackExecutionRequest request =
        request_from_dispatch(cpu, bus, kPc, kPc);
    const DbtReferenceFallbackExecutionResult result =
        execute_dbt_reference_fallback(cpu, bus, request);

    return expect(request.ok &&
                      request.kind == DbtReferenceFallbackExecutionKind::HelperBridgeReferenceStep,
                  "helper-required block should produce helper bridge reference request") &&
           expect(result.ok && result.executed_reference_step &&
                      !result.executed_helper && result.executed_guest_code,
                  "helper-required fallback should execute reference step, not helper path") &&
           expect(cpu.core().read_gpr(5) == 0x11223344 &&
                      cpu.core().pc() == kPc + 4 && cpu.core().instret() == 1,
                  "helper-required fallback should commit load through reference backend");
}

bool test_reference_fallback_execution_runs_jit_miss_and_differential_mismatch_reasons() {
    Ram ram;
    Bus bus(ram);
    CPU miss_cpu;
    CPU mismatch_cpu;
    cpu_init(miss_cpu, kPc);
    cpu_init(mismatch_cpu, kPc);
    write32(ram, kPc, kAddiX1One);

    DbtReferenceFallbackExecutionRequest miss =
        plan_dbt_reference_fallback_execution(
            plain_plan("jit-cache-miss", DbtRejectKind::PlanRejected));
    miss.pc = kPc;
    miss.end_pc = kPc;
    DbtReferenceFallbackExecutionRequest mismatch =
        plan_dbt_reference_fallback_execution(
            plain_plan("differential-mismatch", DbtRejectKind::UnsupportedIr));
    mismatch.pc = kPc;
    mismatch.end_pc = kPc;

    const DbtReferenceFallbackExecutionResult miss_result =
        execute_dbt_reference_fallback(miss_cpu, bus, miss);
    const DbtReferenceFallbackExecutionResult mismatch_result =
        execute_dbt_reference_fallback(mismatch_cpu, bus, mismatch);

    return expect(miss.kind == DbtReferenceFallbackExecutionKind::JitMissReferenceStep,
                  "JIT miss request should preserve miss classification") &&
           expect(miss_result.ok && miss_result.executed_reference_step &&
                      miss_cpu.core().read_gpr(1) == 1,
                  "JIT miss fallback should execute one reference step") &&
           expect(mismatch.reject_reason == "differential-mismatch",
                  "mismatch request should preserve differential mismatch reason") &&
           expect(mismatch_result.ok && mismatch_result.executed_reference_step &&
                      mismatch_cpu.core().read_gpr(1) == 1,
                  "differential mismatch fallback should execute one reference step");
}

}  // namespace

int main() {
    if (!test_reference_fallback_execution_rejects_invalid_plan()) {
        return 1;
    }
    if (!test_reference_fallback_execution_classifies_plain_and_jit_miss()) {
        return 1;
    }
    if (!test_reference_fallback_execution_classifies_helper_and_fault_placeholders()) {
        return 1;
    }
    if (!test_reference_fallback_execution_runs_plain_reference_step_opt_in()) {
        return 1;
    }
    if (!test_reference_fallback_execution_runs_helper_required_memory_load_via_reference()) {
        return 1;
    }
    if (!test_reference_fallback_execution_runs_jit_miss_and_differential_mismatch_reasons()) {
        return 1;
    }
    std::puts("dbt_reference_fallback_execution_smoke: PASS");
    return 0;
}
