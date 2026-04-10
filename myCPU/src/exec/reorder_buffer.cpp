#include "reorder_buffer.h"

#include <algorithm>

#include "vector_ops.h"

extern "C" {
#include "../decode.h"
}

namespace {

bool is_vector_raw(uint32_t raw) {
    const uint32_t opcode = raw & 0x7FU;
    return opcode == 0x57 || opcode == 0x07 || opcode == 0x27;
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
