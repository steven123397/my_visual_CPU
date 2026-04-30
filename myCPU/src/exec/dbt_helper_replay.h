#pragma once

#include <cstdint>
#include <string>

#include "dbt_block_plan.h"
#include "dbt_ir.h"

enum class DbtHelperReplayKind : uint8_t {
    None,
    ScalarMemoryLoad,
    ScalarMemoryStore,
    CsrWrite,
    AtomicMemory,
    VectorConfig,
    VectorMemoryLoad,
    VectorMemoryStore,
    VectorAlu,
};

struct DbtHelperReplayPlan {
    bool ok{false};
    std::string reject_reason{};
    DbtHelperReplayKind kind{DbtHelperReplayKind::None};
    DbtHelperKind helper_kind{DbtHelperKind::None};
    uint64_t pc{0};
    uint32_t raw{0};
    uint8_t rd{0};
    uint64_t addr{0};
    uint8_t size{0};
    bool sign_extend{false};
    uint32_t csr_addr{0};
    uint64_t value{0};
    DbtAtomicHelperOp atomic_op{DbtAtomicHelperOp::None};
    bool atomic_aq{false};
    bool atomic_rl{false};
    DbtVectorHelperOp vector_op{DbtVectorHelperOp::None};
    uint8_t vector_vs1{0};
    uint8_t vector_vs2{0};
    uint8_t vector_sew_bytes{0};
    uint8_t vector_vl{0};
    bool reads_memory{false};
    bool writes_memory{false};
    bool writes_gpr{false};
    bool writes_csr{false};
    bool writes_vector{false};
    bool changes_vector_config{false};
    bool may_trap{false};
    bool may_change_platform_state{false};
    bool commit_at_boundary{false};
    bool non_speculative{false};
    bool serializing{false};
};

DbtHelperReplayPlan plan_dbt_helper_replay(const DbtTranslationUnit& unit);

const char* dbt_helper_replay_kind_name(DbtHelperReplayKind kind);
