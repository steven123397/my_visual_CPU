#pragma once

#include <cstdio>

#include "final_state.h"

namespace spike_differential {

struct CompareOptions {
    bool include_instret{true};
    bool include_trap_summary{true};
    bool include_first_trap_summary{false};
};

inline const char* mismatch_kind_name(MismatchKind kind) {
    switch (kind) {
    case MismatchKind::None:
        return "none";
    case MismatchKind::Halted:
        return "halted";
    case MismatchKind::TimedOut:
        return "timed_out";
    case MismatchKind::Pc:
        return "pc";
    case MismatchKind::Instret:
        return "instret";
    case MismatchKind::Privilege:
        return "privilege";
    case MismatchKind::Gpr:
        return "gpr";
    case MismatchKind::Csr:
        return "csr";
    case MismatchKind::WatchedMemorySize:
        return "watched_memory_size";
    case MismatchKind::WatchedMemory:
        return "watched_memory";
    case MismatchKind::TrapTrapped:
        return "trap_trapped";
    case MismatchKind::TrapCause:
        return "trap_cause";
    case MismatchKind::TrapTval:
        return "trap_tval";
    case MismatchKind::TrapEpc:
        return "trap_epc";
    case MismatchKind::TrapPrivilege:
        return "trap_privilege";
    case MismatchKind::FirstTrapTrapped:
        return "first_trap_trapped";
    case MismatchKind::FirstTrapCause:
        return "first_trap_cause";
    case MismatchKind::FirstTrapTval:
        return "first_trap_tval";
    case MismatchKind::FirstTrapEpc:
        return "first_trap_epc";
    case MismatchKind::FirstTrapPrivilege:
        return "first_trap_privilege";
    }
    return "unknown";
}

inline DiffReport match_report() {
    return DiffReport{};
}

inline DiffReport mismatch_report(MismatchKind kind,
                                  const char* field,
                                  const char* scenario_name,
                                  unsigned long long expected,
                                  unsigned long long actual) {
    DiffReport report;
    report.matched = false;
    report.first_mismatch_kind = kind;
    report.first_mismatch_field = field;
    char buffer[256];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "[%s] %s mismatch: expected=0x%llx actual=0x%llx",
                  scenario_name,
                  field,
                  expected,
                  actual);
    report.message = buffer;
    return report;
}

inline DiffReport mismatch_bool_report(MismatchKind kind,
                                       const char* field,
                                       const char* scenario_name,
                                       bool expected,
                                       bool actual) {
    return mismatch_report(kind,
                           field,
                           scenario_name,
                           expected ? 1ULL : 0ULL,
                           actual ? 1ULL : 0ULL);
}

inline DiffReport compare_trap_summary(const char* scenario_name,
                                       const TrapSummary& expected,
                                       const TrapSummary& actual,
                                       const char* field_prefix,
                                       MismatchKind trapped_kind,
                                       MismatchKind cause_kind,
                                       MismatchKind tval_kind,
                                       MismatchKind epc_kind,
                                       MismatchKind privilege_kind) {
    char field[64];
    std::snprintf(field, sizeof(field), "%s.trapped", field_prefix);
    if (expected.trapped != actual.trapped) {
        return mismatch_bool_report(trapped_kind,
                                    field,
                                    scenario_name,
                                    expected.trapped,
                                    actual.trapped);
    }
    if (!expected.trapped) {
        return match_report();
    }

    std::snprintf(field, sizeof(field), "%s.cause", field_prefix);
    if (expected.cause != actual.cause) {
        return mismatch_report(cause_kind, field, scenario_name, expected.cause, actual.cause);
    }
    std::snprintf(field, sizeof(field), "%s.tval", field_prefix);
    if (expected.tval != actual.tval) {
        return mismatch_report(tval_kind, field, scenario_name, expected.tval, actual.tval);
    }
    std::snprintf(field, sizeof(field), "%s.epc", field_prefix);
    if (expected.epc != actual.epc) {
        return mismatch_report(epc_kind, field, scenario_name, expected.epc, actual.epc);
    }
    std::snprintf(field, sizeof(field), "%s.privilege_at_trap", field_prefix);
    if (expected.privilege_at_trap != actual.privilege_at_trap) {
        return mismatch_report(privilege_kind,
                               field,
                               scenario_name,
                               static_cast<unsigned long long>(expected.privilege_at_trap),
                               static_cast<unsigned long long>(actual.privilege_at_trap));
    }
    return match_report();
}

inline uint64_t normalize_csr_for_compare(uint32_t csr, uint64_t value) {
    switch (csr) {
    case CSR_SSTATUS:
        return value & (MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_SPP | MSTATUS_SUM | MSTATUS_MXR);
    case CSR_MSTATUS:
        return value & (MSTATUS_SIE | MSTATUS_MIE | MSTATUS_SPIE | MSTATUS_MPIE | MSTATUS_SPP |
                        MSTATUS_MPRV | MSTATUS_SUM | MSTATUS_MXR | MSTATUS_MPP_MASK);
    default:
        return value;
    }
}

inline DiffReport compare_final_state(const char* scenario_name,
                                      const FinalState& expected,
                                      const FinalState& actual,
                                      const CompareOptions& options = {}) {
    if (expected.halted != actual.halted) {
        return mismatch_bool_report(MismatchKind::Halted,
                                    "halted",
                                    scenario_name,
                                    expected.halted,
                                    actual.halted);
    }
    if (expected.timed_out != actual.timed_out) {
        return mismatch_bool_report(MismatchKind::TimedOut,
                                    "timed_out",
                                    scenario_name,
                                    expected.timed_out,
                                    actual.timed_out);
    }
    if (expected.pc != actual.pc) {
        return mismatch_report(MismatchKind::Pc,
                               "pc",
                               scenario_name,
                               expected.pc,
                               actual.pc);
    }
    if (options.include_instret && expected.instret != actual.instret) {
        return mismatch_report(MismatchKind::Instret,
                               "instret",
                               scenario_name,
                               expected.instret,
                               actual.instret);
    }
    if (expected.privilege != actual.privilege) {
        return mismatch_report(MismatchKind::Privilege,
                               "privilege",
                               scenario_name,
                               static_cast<unsigned long long>(expected.privilege),
                               static_cast<unsigned long long>(actual.privilege));
    }
    for (size_t i = 0; i < expected.gprs.size(); ++i) {
        if (expected.gprs[i] != actual.gprs[i]) {
            char field[64];
            std::snprintf(field, sizeof(field), "gpr[x%zu]", i);
            return mismatch_report(MismatchKind::Gpr,
                                   field,
                                   scenario_name,
                                   expected.gprs[i],
                                   actual.gprs[i]);
        }
    }
    for (size_t i = 0; i < expected.csrs.size(); ++i) {
        const uint64_t expected_value = normalize_csr_for_compare(kTrackedCsrs[i], expected.csrs[i]);
        const uint64_t actual_value = normalize_csr_for_compare(kTrackedCsrs[i], actual.csrs[i]);
        if (expected_value != actual_value) {
            char field[64];
            std::snprintf(field, sizeof(field), "csr[%s]", csr_name(kTrackedCsrs[i]));
            return mismatch_report(MismatchKind::Csr,
                                   field,
                                   scenario_name,
                                   expected_value,
                                   actual_value);
        }
    }
    if (expected.watched_memory.size() != actual.watched_memory.size()) {
        return mismatch_report(MismatchKind::WatchedMemorySize,
                               "watched_memory.size",
                               scenario_name,
                               static_cast<unsigned long long>(expected.watched_memory.size()),
                               static_cast<unsigned long long>(actual.watched_memory.size()));
    }
    for (size_t i = 0; i < expected.watched_memory.size(); ++i) {
        if (expected.watched_memory[i] != actual.watched_memory[i]) {
            char field[64];
            std::snprintf(field, sizeof(field), "watched_memory[%zu]", i);
            return mismatch_report(MismatchKind::WatchedMemory,
                                   field,
                                   scenario_name,
                                   expected.watched_memory[i],
                                   actual.watched_memory[i]);
        }
    }
    if (!options.include_trap_summary) {
        if (!options.include_first_trap_summary) {
            return match_report();
        }
    } else {
        const DiffReport trap_report = compare_trap_summary(scenario_name,
                                                            expected.trap_summary,
                                                            actual.trap_summary,
                                                            "trap_summary",
                                                            MismatchKind::TrapTrapped,
                                                            MismatchKind::TrapCause,
                                                            MismatchKind::TrapTval,
                                                            MismatchKind::TrapEpc,
                                                            MismatchKind::TrapPrivilege);
        if (!trap_report.matched) {
            return trap_report;
        }
    }

    if (options.include_first_trap_summary) {
        return compare_trap_summary(scenario_name,
                                    expected.first_trap_summary,
                                    actual.first_trap_summary,
                                    "first_trap_summary",
                                    MismatchKind::FirstTrapTrapped,
                                    MismatchKind::FirstTrapCause,
                                    MismatchKind::FirstTrapTval,
                                    MismatchKind::FirstTrapEpc,
                                    MismatchKind::FirstTrapPrivilege);
    }
    return match_report();
}

}  // namespace spike_differential
