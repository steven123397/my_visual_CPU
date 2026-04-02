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

bool ranges_overlap(uint64_t lhs_addr, int lhs_size, uint64_t rhs_addr, int rhs_size) {
    const uint64_t lhs_end = lhs_addr + static_cast<uint64_t>(lhs_size);
    const uint64_t rhs_end = rhs_addr + static_cast<uint64_t>(rhs_size);
    return lhs_addr < rhs_end && rhs_addr < lhs_end;
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

void LoadStoreQueue::mark_order_ready(LsqIndex index) {
    const auto it = find_entry(entries_, index);
    if (it == entries_.end()) {
        return;
    }
    it->order_ready = true;
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

std::optional<LsqEntry> LoadStoreQueue::peek_oldest() const {
    if (entries_.empty()) {
        return std::nullopt;
    }
    return entries_.front();
}

bool LoadStoreQueue::has_blocking_older_store(uint64_t sequence_id, uint64_t load_addr, int load_size) const {
    for (const LsqEntry& entry : entries_) {
        if (entry.kind != LsqEntryKind::Store || entry.sequence_id >= sequence_id) {
            continue;
        }
        if (!entry.address_ready || !entry.data_ready) {
            return true;
        }
        if (!entry.order_ready && ranges_overlap(entry.address, entry.size, load_addr, load_size)) {
            return true;
        }
    }
    return false;
}

std::optional<LsqEntry> LoadStoreQueue::retire_entry(LsqIndex index) {
    const auto it = find_entry(entries_, index);
    if (it == entries_.end() || !it->address_ready || !it->data_ready || !it->order_ready) {
        return std::nullopt;
    }

    const LsqEntry entry = *it;
    entries_.erase(it);
    return entry;
}

void LoadStoreQueue::clear() {
    entries_.clear();
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
