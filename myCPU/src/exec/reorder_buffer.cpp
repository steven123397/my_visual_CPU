#include "reorder_buffer.h"

#include <algorithm>

RobIndex ReorderBuffer::allocate(const RobAllocate& entry) {
    const RobIndex index{.value = next_index_++};
    entries_.push_back({
        .index = index,
        .sequence_id = entry.sequence_id,
        .pc = entry.pc,
        .raw = entry.raw,
        .arch_rd = entry.arch_rd,
        .phys_rd = entry.phys_rd,
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
}

std::optional<RobEntry> ReorderBuffer::peek_head() const {
    if (entries_.empty()) {
        return std::nullopt;
    }
    return entries_.front();
}

void ReorderBuffer::commit_head() {
    if (!entries_.empty() && entries_.front().ready) {
        entries_.pop_front();
    }
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
