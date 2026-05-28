#include "reorder_buffer.h"

#include <algorithm>

#include "floating_ops.h"
#include "memory_ops.h"
#include "vector_ops.h"

extern "C" {
#include "../decode.h"
}

namespace {

bool is_vector_raw(uint32_t raw) {
    Insn insn{};
    decode(raw, &insn);
    insn.raw = raw;
    if (is_standard_fp_load(insn) || is_standard_fp_store(insn)) {
        return false;
    }
    return is_vector_opcode(insn.opcode);
}

bool writes_fp_destination(const Insn& insn) {
    return is_standard_fp_load(insn) || is_fmv_d_x(insn) || is_fmv_w_x(insn) || is_fmv_d(insn) || is_fneg_d(insn) ||
           is_fsgnj_d(insn) || is_fsgnjn_d(insn) || is_fsgnjx_d(insn) || is_fsgnj_s(insn) ||
           is_fsgnjn_s(insn) || is_fsgnjx_s(insn) || is_fadd_s(insn) || is_fsub_s(insn) || is_fmul_s(insn) || is_fdiv_s(insn) ||
           is_fadd_d(insn) || is_fsub_d(insn) || is_fmul_d(insn) ||
           is_fdiv_d(insn) || is_fmax_s(insn) || is_fmin_s(insn) || is_fmax_d(insn) || is_fmin_d(insn) ||
           is_fsqrt_s(insn) || is_fmadd_s(insn) || is_fmsub_s(insn) || is_fnmsub_s(insn) || is_fnmadd_s(insn) ||
           is_fmadd_d(insn) || is_fmsub_d(insn) || is_fnmsub_d(insn) || is_fnmadd_d(insn) || is_fsqrt_d(insn) || is_fcvt_d_w(insn) ||
           is_fcvt_d_wu(insn) || is_fcvt_d_l(insn) || is_fcvt_d_lu(insn) ||
           is_fcvt_s_w(insn) || is_fcvt_s_wu(insn) || is_fcvt_s_l(insn) || is_fcvt_s_lu(insn) || is_fcvt_d_s(insn) || is_fcvt_s_d(insn);
}

}  // namespace

RobIndex ReorderBuffer::allocate(const RobAllocate& entry) {
    const RobIndex index{.value = next_index_++};
    entries_.push_back({
        .index = index,
        .sequence_id = entry.sequence_id,
        .pc = entry.pc,
        .raw = entry.raw,
        .arch_rd = entry.arch_rd,
        .phys_rd = entry.phys_rd,
        .previous_phys_rd = entry.previous_phys_rd,
        .lsq_index = entry.lsq_index,
    });
    return index;
}

void ReorderBuffer::mark_ready(RobIndex index, const RobReady& ready) {
    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const RobEntry& entry) {
        return entry.index.value == index.value;
    });
    if (it == entries_.end()) {
        return;
    }

    it->ready = true;
    it->value_ready = ready.value_ready;
    it->value = ready.value;
    it->has_fault = ready.has_fault;
    it->cause = ready.cause;
    it->tval = ready.tval;
    it->redirect = ready.redirect;
    it->redirect_target = ready.redirect_target;
    it->effects = ready.effects;
    if (ready.lsq_index.value != 0) {
        it->lsq_index = ready.lsq_index;
    }
}

std::optional<RobEntry> ReorderBuffer::peek_head() const {
    if (entries_.empty()) {
        return std::nullopt;
    }
    return entries_.front();
}

bool ReorderBuffer::has_older_vector_pending(uint64_t sequence_id) const {
    return std::any_of(entries_.begin(),
                       entries_.end(),
                       [&](const RobEntry& entry) {
                           return entry.sequence_id < sequence_id &&
                                  is_vector_raw(entry.raw);
                       });
}

bool ReorderBuffer::has_older_fp_pending(uint64_t sequence_id,
                                         uint8_t rs1,
                                         uint8_t rs2,
                                         uint8_t rs3) const {
    for (const RobEntry& entry : entries_) {
        if (entry.sequence_id >= sequence_id) {
            continue;
        }

        Insn insn{};
        decode(entry.raw, &insn);
        insn.raw = entry.raw;
        if (!writes_fp_destination(insn)) {
            continue;
        }

        const uint8_t rd = insn.rd;
        if (rd == rs1 || rd == rs2 || rd == rs3) {
            return true;
        }
    }
    return false;
}

OlderVectorDependency ReorderBuffer::inspect_older_vector_dependencies(uint64_t sequence_id,
                                                                       uint8_t vs1,
                                                                       uint8_t vs2) const {
    OlderVectorDependency dependency;
    for (const RobEntry& entry : entries_) {
        if (entry.sequence_id >= sequence_id) {
            continue;
        }
        if (!is_vector_raw(entry.raw)) {
            continue;
        }

        Insn insn{};
        decode(entry.raw, &insn);
        insn.raw = entry.raw;
        if (is_serializing_vector_insn(insn)) {
            dependency.blocks = true;
            return dependency;
        }
        if (!is_non_memory_vector_alu_insn(insn)) {
            continue;
        }
        if (insn.rd != vs1 && insn.rd != vs2) {
            continue;
        }
        if (!entry.ready || !entry.effects.vector.result_valid) {
            dependency.blocks = true;
            return dependency;
        }
        if (insn.rd == vs1) {
            dependency.vs1_valid = true;
            dependency.vs1 = entry.effects.vector.result;
        }
        if (insn.rd == vs2) {
            dependency.vs2_valid = true;
            dependency.vs2 = entry.effects.vector.result;
        }
    }
    return dependency;
}

void ReorderBuffer::commit_head() {
    if (!entries_.empty() && entries_.front().ready) {
        entries_.pop_front();
    }
}

void ReorderBuffer::clear() {
    entries_.clear();
}

void ReorderBuffer::flush_younger_than(uint64_t sequence_id) {
    entries_.erase(std::remove_if(entries_.begin(),
                                  entries_.end(),
                                  [&](const RobEntry& entry) {
                                      return entry.sequence_id > sequence_id;
                                  }),
                   entries_.end());
}

size_t ReorderBuffer::size() const {
    return entries_.size();
}
