#pragma once

#include <cstdio>

#include "final_state.h"

namespace spike_differential {

struct CompareOptions {
    bool include_instret{true};
    bool include_trap_summary{true};
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
        return match_report();
    }
    if (expected.trap_summary.trapped != actual.trap_summary.trapped) {
        return mismatch_bool_report(MismatchKind::TrapTrapped,
                                    "trap_summary.trapped",
                                    scenario_name,
                                    expected.trap_summary.trapped,
                                    actual.trap_summary.trapped);
    }
    if (!expected.trap_summary.trapped) {
        return match_report();
    }
    if (expected.trap_summary.cause != actual.trap_summary.cause) {
        return mismatch_report(MismatchKind::TrapCause,
                               "trap_summary.cause",
                               scenario_name,
                               expected.trap_summary.cause,
                               actual.trap_summary.cause);
    }
    if (expected.trap_summary.tval != actual.trap_summary.tval) {
        return mismatch_report(MismatchKind::TrapTval,
                               "trap_summary.tval",
                               scenario_name,
                               expected.trap_summary.tval,
                               actual.trap_summary.tval);
    }
    if (expected.trap_summary.epc != actual.trap_summary.epc) {
        return mismatch_report(MismatchKind::TrapEpc,
                               "trap_summary.epc",
                               scenario_name,
                               expected.trap_summary.epc,
                               actual.trap_summary.epc);
    }
    if (expected.trap_summary.privilege_at_trap != actual.trap_summary.privilege_at_trap) {
        return mismatch_report(MismatchKind::TrapPrivilege,
                               "trap_summary.privilege_at_trap",
                               scenario_name,
                               static_cast<unsigned long long>(expected.trap_summary.privilege_at_trap),
                               static_cast<unsigned long long>(actual.trap_summary.privilege_at_trap));
    }
    return match_report();
}

}  // namespace spike_differential
