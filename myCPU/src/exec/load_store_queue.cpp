#include "load_store_queue.h"

#include <algorithm>

namespace {

std::vector<LsqEntry>::iterator find_entry(std::vector<LsqEntry>& entries, LsqIndex index) {
    return std::find_if(entries.begin(), entries.end(), [&](const LsqEntry& entry) {
        return entry.index.value == index.value;
    });
}

std::vector<LsqEntry>::const_iterator find_entry(const std::vector<LsqEntry>& entries, LsqIndex index) {
    return std::find_if(entries.begin(), entries.end(), [&](const LsqEntry& entry) {
        return entry.index.value == index.value;
    });
}

}  // namespace

LsqIndex LoadStoreQueue::enqueue_load(const LsqLoadRequest& req) {
    const LsqIndex index{.value = next_index_++};
    entries_.push_back({
        .index = index,
        .kind = LsqEntryKind::Load,
        .sequence_id = req.sequence_id,
        .rd = req.rd,
        .size = req.size,
        .sign_extend = req.sign_extend,
        .mmio = req.mmio,
        .non_speculative = req.non_speculative,
    });
    return index;
}

LsqIndex LoadStoreQueue::enqueue_store(const LsqStoreRequest& req) {
    const LsqIndex index{.value = next_index_++};
    entries_.push_back({
        .index = index,
        .kind = LsqEntryKind::Store,
        .sequence_id = req.sequence_id,
        .size = req.size,
        .mmio = req.mmio,
        .non_speculative = req.non_speculative,
    });
    return index;
}

void LoadStoreQueue::mark_address_ready(LsqIndex index, uint64_t addr) {
    const auto it = find_entry(entries_, index);
    if (it == entries_.end()) {
        return;
    }
    it->address_ready = true;
    it->address = addr;
}

void LoadStoreQueue::mark_data_ready(LsqIndex index, uint64_t value) {
    const auto it = find_entry(entries_, index);
    if (it == entries_.end()) {
        return;
    }
    it->data_ready = true;
    it->data = value;
}

std::optional<LsqEntry> LoadStoreQueue::peek(LsqIndex index) const {
    const auto it = find_entry(entries_, index);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return *it;
}

std::optional<LsqEntry> LoadStoreQueue::retire_store(LsqIndex index) {
    const auto it = find_entry(entries_, index);
    if (it == entries_.end() || it->kind != LsqEntryKind::Store ||
        !it->address_ready || !it->data_ready) {
        return std::nullopt;
    }

    const LsqEntry entry = *it;
    entries_.erase(it);
    return entry;
}

void LoadStoreQueue::flush_younger_than(uint64_t sequence_id) {
    entries_.erase(std::remove_if(entries_.begin(),
                                  entries_.end(),
                                  [&](const LsqEntry& entry) {
                                      return entry.sequence_id > sequence_id;
                                  }),
                   entries_.end());
}

size_t LoadStoreQueue::size() const {
    return entries_.size();
}
