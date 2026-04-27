#pragma once

#include <cstdint>

#include "../isa/effects.h"
#include "../isa/atomic_contract.h"

class CPU;
class Bus;

struct CommitBoundaryInput {
    uint64_t pc{0};
    uint64_t next_pc{0};
    InsnEffects effects{};
};

struct CommitBoundaryResult {
    bool retired{false};
    bool trap_taken{false};
    bool trap_flush{false};
    bool redirect{false};
    bool platform_state_changed{false};
    bool atomic_memory_observed{false};
    bool atomic_write_observed{false};
    bool atomic_paddr_valid{false};
    uint64_t atomic_paddr{0};
    uint64_t atomic_bytes{0};
    uint64_t next_pc{0};
};

CommitBoundaryResult apply_commit_boundary(CPU& cpu,
                                           Bus& bus,
                                           const CommitBoundaryInput& input);
