#include <cstdio>

#include "../../src/exec/branch_predictor.h"

namespace {

constexpr uint64_t kBranchPc = 0x80000020ULL;
constexpr uint64_t kBranchTarget = 0x80000040ULL;
constexpr uint64_t kAliasBranchPc = 0x80000120ULL;
constexpr uint64_t kAliasBranchTarget = 0x80000140ULL;
constexpr uint32_t kBeqTaken = 0x00000463U;
constexpr uint32_t kJalForward = 0x0080006fU;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    BranchPredictor predictor;

    const PredictorQueryResult cold_branch = predictor.query(kBranchPc, kBeqTaken);
    if (!expect(cold_branch.valid, "conditional branch query should return a valid predictor result")) {
        return 1;
    }
    if (!expect(!cold_branch.predicted_taken, "cold conditional branch should default to not-taken")) {
        return 1;
    }
    if (!expect(cold_branch.predicted_target == kBranchPc + 4, "cold conditional branch should fall through")) {
        return 1;
    }

    predictor.update({
        .pc = kBranchPc,
        .raw = kBeqTaken,
        .prediction = cold_branch,
        .taken = true,
        .target = kBranchTarget,
    });

    const PredictorQueryResult warm_branch = predictor.query(kBranchPc, kBeqTaken);
    if (!expect(warm_branch.predicted_taken, "trained conditional branch should predict taken")) {
        return 1;
    }
    if (!expect(warm_branch.predicted_target == kBranchTarget, "trained conditional branch should reuse learned target")) {
        return 1;
    }
    if (!expect(warm_branch.table_hit, "trained conditional branch should report a predictor table hit")) {
        return 1;
    }

    predictor.update({
        .pc = kBranchPc,
        .raw = kBeqTaken,
        .prediction = warm_branch,
        .taken = true,
        .target = kBranchTarget,
    });

    const PredictorQueryResult preserved_branch = predictor.query(kBranchPc, kBeqTaken);
    if (!expect(preserved_branch.predicted_taken, "preserved branch prediction should stay taken before alias eviction")) {
        return 1;
    }

    const PredictorQueryResult alias_branch = predictor.query(kAliasBranchPc, kBeqTaken);
    if (!expect(!alias_branch.predicted_taken, "cold alias branch should default to not-taken")) {
        return 1;
    }
    predictor.update({
        .pc = kAliasBranchPc,
        .raw = kBeqTaken,
        .prediction = alias_branch,
        .taken = true,
        .target = kAliasBranchTarget,
    });
    predictor.update({
        .pc = kBranchPc,
        .raw = kBeqTaken,
        .prediction = preserved_branch,
        .taken = true,
        .target = kBranchTarget,
    });

    const PredictorStats stats_after_alias = predictor.stats();
    if (!expect(stats_after_alias.total_predictions == 4, "predictor should count every resolved control-flow update once")) {
        return 1;
    }
    if (!expect(stats_after_alias.correct_predictions == 2, "alias eviction should not rewrite an older correct prediction into a miss")) {
        return 1;
    }
    if (!expect(stats_after_alias.mispredictions == 2, "cold-taken training should account for exactly two misses in this scenario")) {
        return 1;
    }

    const PredictorQueryResult static_jal = predictor.query(kBranchPc + 0x20, kJalForward);
    if (!expect(static_jal.predicted_taken, "jal should use static predict-taken in phase 3-a")) {
        return 1;
    }
    if (!expect(static_jal.predicted_target == (kBranchPc + 0x20) + 8, "jal should expose its pc-relative target")) {
        return 1;
    }

    predictor.reset();
    const PredictorStats cleared_stats = predictor.stats();
    if (!expect(cleared_stats.total_predictions == 0, "reset should clear total prediction count")) {
        return 1;
    }
    if (!expect(cleared_stats.correct_predictions == 0, "reset should clear correct prediction count")) {
        return 1;
    }
    if (!expect(cleared_stats.mispredictions == 0, "reset should clear misprediction count")) {
        return 1;
    }

    const PredictorQueryResult reset_branch = predictor.query(kBranchPc, kBeqTaken);
    if (!expect(!reset_branch.predicted_taken, "reset predictor should forget trained taken state")) {
        return 1;
    }
    const PredictorStats reset_stats = predictor.stats();
    if (!expect(reset_stats.total_predictions == 0,
                "unresolved query after reset should not count toward resolved branch statistics")) {
        return 1;
    }
    if (!expect(reset_stats.correct_predictions == 0, "query alone should not count as a resolved correct prediction")) {
        return 1;
    }
    if (!expect(reset_stats.mispredictions == 0, "query alone should not count as a misprediction")) {
        return 1;
    }

    return 0;
}
