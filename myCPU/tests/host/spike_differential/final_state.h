#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "shared_spec.h"

namespace spike_differential {

struct TrapSummary {
    bool trapped{false};
    uint64_t cause{0};
    uint64_t tval{0};
    uint64_t epc{0};
    PrivilegeMode privilege_at_trap{PrivilegeMode::Machine};
};

enum class MismatchKind : uint8_t {
    None,
    Halted,
    TimedOut,
    Pc,
    Instret,
    Privilege,
    Gpr,
    Csr,
    WatchedMemorySize,
    WatchedMemory,
    TrapTrapped,
    TrapCause,
    TrapTval,
    TrapEpc,
    TrapPrivilege,
};

struct DiffReport {
    bool matched{true};
    MismatchKind first_mismatch_kind{MismatchKind::None};
    std::string first_mismatch_field{};
    std::string message{};
};

struct FinalState {
    bool halted{false};
    bool timed_out{false};
    uint64_t pc{0};
    PrivilegeMode privilege{PrivilegeMode::Machine};
    std::array<uint64_t, 32> gprs{};
    std::array<uint64_t, kTrackedCsrs.size()> csrs{};
    std::vector<uint64_t> watched_memory{};
    TrapSummary trap_summary{};
    uint64_t instret{0};
    std::string exit_reason{};
};

}  // namespace spike_differential
