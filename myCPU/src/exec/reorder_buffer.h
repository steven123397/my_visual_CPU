#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

#include "../arch/vector_state.h"
#include "../isa/effects.h"
#include "load_store_queue.h"

struct RobIndex {
    uint64_t value{0};
};

struct RobAllocate {
    uint64_t sequence_id{0};
    uint64_t pc{0};
    uint32_t raw{0};
    uint8_t arch_rd{0};
    uint32_t phys_rd{0};
    uint32_t previous_phys_rd{0};
    LsqIndex lsq_index{};
};

struct RobReady {
    bool value_ready{false};
    uint64_t value{0};
    bool has_fault{false};
    uint64_t cause{0};
    uint64_t tval{0};
    bool redirect{false};
    uint64_t redirect_target{0};
    InsnEffects effects{};
    LsqIndex lsq_index{};
};

struct RobEntry {
    RobIndex index{};
    uint64_t sequence_id{0};
    uint64_t pc{0};
    uint32_t raw{0};
    uint8_t arch_rd{0};
    uint32_t phys_rd{0};
    uint32_t previous_phys_rd{0};
    LsqIndex lsq_index{};
    bool ready{false};
    bool value_ready{false};
    uint64_t value{0};
    bool has_fault{false};
    uint64_t cause{0};
    uint64_t tval{0};
    bool redirect{false};
    uint64_t redirect_target{0};
    InsnEffects effects{};
};

struct OlderVectorDependency {
    bool blocks{false};
    bool vs1_valid{false};
    VectorState::VectorReg vs1{};
    bool vs2_valid{false};
    VectorState::VectorReg vs2{};
};

class ReorderBuffer {
public:
    RobIndex allocate(const RobAllocate& entry);
    void mark_ready(RobIndex index, const RobReady& ready);
    std::optional<RobEntry> peek_head() const;
    bool has_older_vector_pending(uint64_t sequence_id) const;
    OlderVectorDependency inspect_older_vector_dependencies(uint64_t sequence_id,
                                                            uint8_t vs1,
                                                            uint8_t vs2) const;
    void commit_head();
    void clear();
    void flush_younger_than(uint64_t sequence_id);
    size_t size() const;

private:
    std::deque<RobEntry> entries_{};
    uint64_t next_index_{1};
};
