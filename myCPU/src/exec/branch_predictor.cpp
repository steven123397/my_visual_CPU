#include "branch_predictor.h"

#include <algorithm>

extern "C" {
#include "../decode.h"
}

namespace {

enum class ControlKind : uint8_t {
    None,
    Branch,
    Jal,
    Jalr,
};

ControlKind classify_control_kind(uint32_t raw) {
    Insn insn{};
    decode(raw, &insn);
    switch (insn.opcode) {
    case 0x63:
        return ControlKind::Branch;
    case 0x6F:
        return ControlKind::Jal;
    case 0x67:
        return ControlKind::Jalr;
    default:
        return ControlKind::None;
    }
}

uint64_t compute_target(uint64_t pc, uint32_t raw) {
    Insn insn{};
    decode(raw, &insn);
    return pc + static_cast<int64_t>(insn.imm);
}

bool predicts_taken(uint8_t counter) {
    return counter >= 2;
}

uint8_t saturating_increment(uint8_t counter) {
    return std::min<uint8_t>(3, static_cast<uint8_t>(counter + 1));
}

uint8_t saturating_decrement(uint8_t counter) {
    return counter == 0 ? 0 : static_cast<uint8_t>(counter - 1);
}

}  // namespace

size_t BranchPredictor::index_for(uint64_t pc) {
    return static_cast<size_t>((pc >> 2) & (kTableSize - 1));
}

PredictorQueryResult BranchPredictor::query(uint64_t pc, uint32_t raw) {
    const ControlKind kind = classify_control_kind(raw);
    PredictorQueryResult result;

    switch (kind) {
    case ControlKind::Branch: {
        ++stats_.total_predictions;
        result.valid = true;
        result.predicted_target = pc + 4;

        const Entry& entry = table_[index_for(pc)];
        if (entry.valid && entry.pc == pc) {
            result.table_hit = true;
            result.predicted_taken = predicts_taken(entry.counter);
            if (result.predicted_taken) {
                result.predicted_target = entry.target;
            }
        }
        return result;
    }
    case ControlKind::Jal:
        ++stats_.total_predictions;
        result.valid = true;
        result.predicted_taken = true;
        result.predicted_target = compute_target(pc, raw);
        return result;
    case ControlKind::Jalr:
    case ControlKind::None:
        return result;
    }

    return result;
}

void BranchPredictor::update(const PredictorUpdate& update) {
    const ControlKind kind = classify_control_kind(update.raw);

    switch (kind) {
    case ControlKind::Branch: {
        Entry& entry = table_[index_for(update.pc)];
        const bool table_hit = entry.valid && entry.pc == update.pc;
        const bool predicted_taken = table_hit && predicts_taken(entry.counter);
        const uint64_t predicted_target = predicted_taken ? entry.target : update.pc + 4;
        const bool correct =
            predicted_taken == update.taken && (!predicted_taken || predicted_target == update.target);
        if (correct) {
            ++stats_.correct_predictions;
        } else {
            ++stats_.mispredictions;
        }

        if (!table_hit) {
            entry = Entry{
                .valid = true,
                .pc = update.pc,
                .counter = 1,
                .target = update.target,
            };
        }

        if (update.taken) {
            entry.counter = saturating_increment(entry.counter);
            entry.target = update.target;
        } else {
            entry.counter = saturating_decrement(entry.counter);
        }
        return;
    }
    case ControlKind::Jal: {
        const uint64_t predicted_target = compute_target(update.pc, update.raw);
        const bool correct = update.taken && predicted_target == update.target;
        if (correct) {
            ++stats_.correct_predictions;
        } else {
            ++stats_.mispredictions;
        }
        return;
    }
    case ControlKind::Jalr:
    case ControlKind::None:
        return;
    }
}

void BranchPredictor::reset() {
    table_ = {};
    stats_ = {};
}

PredictorStats BranchPredictor::stats() const {
    return stats_;
}
