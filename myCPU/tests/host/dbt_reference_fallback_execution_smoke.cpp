#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/exec/dbt_reference_fallback.h"
#include "../../src/exec/dbt_reference_fallback_execution.h"

namespace {

constexpr uint64_t kPc = 0x80000000ULL;
constexpr uint32_t kRawJal = 0x0080006fU;
constexpr uint32_t kRawLoad = 0x00002083U;

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
    std::puts("dbt_reference_fallback_execution_smoke: PASS");
    return 0;
}
