#include <cstdint>
#include <cstdio>
#include <string>

#include "../../src/cpu.h"
#include "../../src/exec/dbt_helper_execution_bridge.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kPc = 0x80000000ULL;
constexpr uint64_t kData = 0x80002000ULL;
constexpr uint32_t kRawLoad = 0x0002a283U;   // lw x5, 0(x5)
constexpr uint32_t kRawStore = 0x0062a023U;  // sw x6, 0(x5)
constexpr uint32_t kRawCsr = 0x300312f3U;    // csrrw x5, mstatus, x6
constexpr uint32_t kRawAtomic = 0x0462a2afU; // amoadd.w x5, x6, (x5)
constexpr uint32_t kRawVector = 0x042082d7U; // vdot.vv v5, v1, v2

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

DbtHelperReplayPlan scalar_load_replay() {
    return DbtHelperReplayPlan{
        .ok = true,
        .kind = DbtHelperReplayKind::ScalarMemoryLoad,
        .helper_kind = DbtHelperKind::MemoryLoad,
        .pc = kPc,
        .raw = kRawLoad,
        .rd = 5,
        .addr = 0x1000,
        .size = 4,
        .sign_extend = true,
        .reads_memory = true,
        .writes_gpr = true,
        .may_trap = true,
    };
}

DbtHelperReplayPlan scalar_store_replay() {
    return DbtHelperReplayPlan{
        .ok = true,
        .kind = DbtHelperReplayKind::ScalarMemoryStore,
        .helper_kind = DbtHelperKind::MemoryStore,
        .pc = kPc,
        .raw = kRawStore,
        .addr = 0x1008,
        .size = 4,
        .value = 0x11223344,
        .writes_memory = true,
        .may_trap = true,
        .may_change_platform_state = true,
        .commit_at_boundary = true,
        .non_speculative = true,
        .serializing = true,
    };
}

DbtHelperReplayPlan scalar_load_replay(uint64_t addr, uint8_t rd, uint8_t size, bool sign_extend) {
    DbtHelperReplayPlan replay = scalar_load_replay();
    replay.addr = addr;
    replay.rd = rd;
    replay.size = size;
    replay.sign_extend = sign_extend;
    replay.writes_gpr = rd != 0;
    return replay;
}

DbtHelperReplayPlan scalar_store_replay(uint64_t addr, uint8_t size, uint64_t value) {
    DbtHelperReplayPlan replay = scalar_store_replay();
    replay.addr = addr;
    replay.size = size;
    replay.value = value;
    return replay;
}

DbtHelperReplayPlan csr_replay() {
    return DbtHelperReplayPlan{
        .ok = true,
        .kind = DbtHelperReplayKind::CsrWrite,
        .helper_kind = DbtHelperKind::CsrWrite,
        .pc = kPc,
        .raw = kRawCsr,
        .rd = 5,
        .csr_addr = 0x300,
        .value = 0x1800,
        .writes_gpr = true,
        .writes_csr = true,
        .serializing = true,
    };
}

DbtHelperReplayPlan atomic_replay() {
    return DbtHelperReplayPlan{
        .ok = true,
        .kind = DbtHelperReplayKind::AtomicMemory,
        .helper_kind = DbtHelperKind::Atomic,
        .pc = kPc,
        .raw = kRawAtomic,
        .rd = 5,
        .addr = 0x2000,
        .size = 4,
        .value = 0x12345678,
        .atomic_op = DbtAtomicHelperOp::Add,
        .atomic_aq = true,
        .reads_memory = true,
        .writes_memory = true,
        .writes_gpr = true,
        .may_trap = true,
        .may_change_platform_state = true,
        .commit_at_boundary = true,
        .non_speculative = true,
        .serializing = true,
    };
}

DbtHelperReplayPlan vector_replay() {
    return DbtHelperReplayPlan{
        .ok = true,
        .kind = DbtHelperReplayKind::VectorAlu,
        .helper_kind = DbtHelperKind::Vector,
        .pc = kPc,
        .raw = kRawVector,
        .rd = 5,
        .vector_op = DbtVectorHelperOp::Dot,
        .vector_vs1 = 1,
        .vector_vs2 = 2,
        .vector_sew_bytes = 4,
        .vector_vl = 8,
        .writes_vector = true,
        .may_trap = true,
    };
}

bool test_helper_execution_bridge_rejects_invalid_replay() {
    const DbtHelperReplayPlan replay{};
    const DbtHelperExecutionRequest request =
        plan_dbt_helper_execution_bridge(replay);
    const std::string line = format_dbt_helper_execution_request(request);

    return expect(!request.ok, "helper execution bridge should reject invalid replay") &&
           expect(request.kind == DbtHelperExecutionKind::None,
                  "invalid replay should not expose helper kind") &&
           expect(request.reject_reason == "invalid-helper-replay-plan",
                  "invalid replay should expose stable rejection reason") &&
           expect(request.dry_run_only && !request.executed_helper &&
                      !request.mutates_cpu_state,
                  "invalid replay should remain non-executing") &&
           expect(line.find("helper-exec: kind=none") != std::string::npos,
                  "formatter should expose stable none kind");
}

bool test_helper_execution_bridge_plans_scalar_helpers_without_execution() {
    const DbtHelperExecutionRequest load =
        plan_dbt_helper_execution_bridge(scalar_load_replay());
    const DbtHelperExecutionRequest store =
        plan_dbt_helper_execution_bridge(scalar_store_replay());
    const DbtHelperExecutionRequest csr =
        plan_dbt_helper_execution_bridge(csr_replay());

    return expect(load.ok && load.kind == DbtHelperExecutionKind::ScalarMemoryLoad,
                  "load replay should become scalar memory load execution request") &&
           expect(load.pc == kPc && load.addr == 0x1000 && load.size == 4 &&
                      load.rd == 5 && load.sign_extend,
                  "load execution request should preserve operands") &&
           expect(load.reads_memory && load.writes_gpr && load.may_trap &&
                      load.fallback_to_reference_on_trap,
                  "load execution request should expose effects and trap fallback") &&
           expect(!load.executed_helper && !load.mutates_cpu_state,
                  "load execution request should not execute helper") &&
           expect(store.ok && store.kind == DbtHelperExecutionKind::ScalarMemoryStore,
                  "store replay should become scalar memory store execution request") &&
           expect(store.writes_memory && store.value == 0x11223344 &&
                      store.requires_commit_boundary && store.non_speculative &&
                      store.serializing,
                  "store execution request should preserve memory and ordering effects") &&
           expect(csr.ok && csr.kind == DbtHelperExecutionKind::CsrWrite,
                  "CSR replay should become CSR execution request") &&
           expect(csr.writes_csr && csr.writes_gpr && csr.csr_addr == 0x300 &&
                      csr.value == 0x1800 && csr.serializing,
                  "CSR execution request should preserve CSR effects");
}

bool test_helper_execution_bridge_plans_atomic_and_vector_helpers() {
    const DbtHelperExecutionRequest atomic =
        plan_dbt_helper_execution_bridge(atomic_replay());
    const DbtHelperExecutionRequest vector =
        plan_dbt_helper_execution_bridge(vector_replay());
    const std::string line = format_dbt_helper_execution_request(vector);

    return expect(atomic.ok && atomic.kind == DbtHelperExecutionKind::AtomicMemory,
                  "atomic replay should become atomic execution request") &&
           expect(atomic.atomic_op == DbtAtomicHelperOp::Add && atomic.atomic_aq &&
                      atomic.reads_memory && atomic.writes_memory && atomic.writes_gpr,
                  "atomic execution request should preserve op and effects") &&
           expect(atomic.requires_commit_boundary && atomic.non_speculative &&
                      atomic.serializing && atomic.fallback_to_reference_on_trap,
                  "atomic execution request should preserve ordering and fallback") &&
           expect(vector.ok && vector.kind == DbtHelperExecutionKind::VectorAlu,
                  "vector replay should become vector execution request") &&
           expect(vector.vector_op == DbtVectorHelperOp::Dot && vector.rd == 5 &&
                      vector.vector_vs1 == 1 && vector.vector_vs2 == 2 &&
                      vector.vector_sew_bytes == 4 && vector.vector_vl == 8,
                  "vector execution request should preserve vector operands") &&
           expect(vector.writes_vector && vector.fallback_to_reference_on_trap,
                  "vector execution request should expose vector effects and fallback") &&
           expect(dbt_helper_execution_kind_name(DbtHelperExecutionKind::VectorAlu) ==
                      std::string("vector-alu"),
                  "helper execution kind name should be stable") &&
           expect(line.find("helper-exec: kind=vector-alu") != std::string::npos,
                  "formatter should expose vector helper kind") &&
           expect(line.find("executed-helper=false") != std::string::npos,
                  "formatter should expose non-execution flag");
}

bool test_helper_execution_bridge_executes_scalar_memory_load_opt_in() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kPc);
    cpu.core().write_gpr(5, 0xaaaaaaaaaaaaaaaaULL);
    ram.store(kData, 0xffffffffffffff80ULL, 1);

    const DbtHelperExecutionRequest request =
        plan_dbt_helper_execution_bridge(scalar_load_replay(kData, 5, 1, true));
    const DbtHelperExecutionResult result =
        execute_dbt_helper_request(cpu, bus, request);
    const std::string line = format_dbt_helper_execution_result(result);

    return expect(request.ok, "load helper request should be valid") &&
           expect(result.ok && result.executed_helper && result.mutated_cpu_state,
                  "opt-in load helper execution should run and mutate CPU state") &&
           expect(result.retired && !result.trap_taken && result.next_pc == kPc + 4,
                  "load helper should retire exactly at the helper commit boundary") &&
           expect(cpu.core().read_gpr(5) == 0xffffffffffffff80ULL,
                  "load helper should sign-extend and commit loaded value to rd") &&
           expect(cpu.core().pc() == kPc + 4 && cpu.core().instret() == 1,
                  "load helper should commit next PC and instret") &&
           expect(line.find("helper-exec-result: kind=scalar-memory-load") != std::string::npos,
                  "helper execution result formatter should expose stable prefix") &&
           expect(line.find("dry-run-only=false") != std::string::npos,
                  "executed helper result should clear dry-run-only flag");
}

bool test_helper_execution_bridge_executes_scalar_memory_store_opt_in() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kPc);

    const DbtHelperExecutionRequest request =
        plan_dbt_helper_execution_bridge(scalar_store_replay(kData + 8, 4, 0x11223344));
    const DbtHelperExecutionResult result =
        execute_dbt_helper_request(cpu, bus, request);

    return expect(request.ok, "store helper request should be valid") &&
           expect(result.ok && result.executed_helper && result.mutated_cpu_state,
                  "opt-in store helper execution should run and mutate machine state") &&
           expect(result.retired && result.platform_state_changed &&
                      result.next_pc == kPc + 4,
                  "store helper should retire and report platform boundary change") &&
           expect(ram.load(kData + 8, 4) == 0x11223344,
                  "store helper should commit store value through the bus") &&
           expect(cpu.core().pc() == kPc + 4 && cpu.core().instret() == 1,
                  "store helper should commit next PC and instret");
}

bool test_helper_execution_bridge_rejects_faulting_scalar_memory_load_without_commit() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kPc);
    cpu.core().write_gpr(5, 0x1234);

    const DbtHelperExecutionRequest request =
        plan_dbt_helper_execution_bridge(scalar_load_replay(0x1000, 5, 4, false));
    const DbtHelperExecutionResult result =
        execute_dbt_helper_request(cpu, bus, request);

    return expect(request.ok, "faulting load helper request should still be valid") &&
           expect(!result.ok && result.trap_taken && result.fallback_to_reference_on_trap,
                  "faulting load helper should report trap and reference fallback boundary") &&
           expect(result.executed_helper && !result.mutated_cpu_state && !result.retired,
                  "faulting load helper should execute but avoid helper state commit") &&
           expect(cpu.core().read_gpr(5) == 0x1234 && cpu.core().pc() == kPc,
                  "faulting load helper should preserve rd and pc for reference fallback");
}

}  // namespace

int main() {
    if (!test_helper_execution_bridge_rejects_invalid_replay()) {
        return 1;
    }
    if (!test_helper_execution_bridge_plans_scalar_helpers_without_execution()) {
        return 1;
    }
    if (!test_helper_execution_bridge_plans_atomic_and_vector_helpers()) {
        return 1;
    }
    if (!test_helper_execution_bridge_executes_scalar_memory_load_opt_in()) {
        return 1;
    }
    if (!test_helper_execution_bridge_executes_scalar_memory_store_opt_in()) {
        return 1;
    }
    if (!test_helper_execution_bridge_rejects_faulting_scalar_memory_load_without_commit()) {
        return 1;
    }
    std::puts("dbt_helper_execution_bridge_smoke: PASS");
    return 0;
}
