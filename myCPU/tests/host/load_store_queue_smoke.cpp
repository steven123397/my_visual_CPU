#include <cstdio>

#include "../../src/exec/load_store_queue.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    Ram ram;
    Bus bus(ram);

    LoadStoreQueue lsq;
    const LsqIndex older_store = lsq.enqueue_store({
        .sequence_id = 1,
        .size = 4,
        .non_speculative = true,
    });
    const LsqLoadStatus unresolved_block = lsq.classify_load(2, 0x80001000, 4);
    if (!expect(unresolved_block.state == LsqLoadState::BlockedByUnresolvedStore &&
                    unresolved_block.load_sequence_id == 2 &&
                    unresolved_block.store_sequence_id == 1 &&
                    unresolved_block.blocks_issue(),
                "older unresolved store should report an explicit unresolved-store block")) {
        return 1;
    }

    lsq.mark_address_ready(older_store, 0x80001000);
    const LsqLoadStatus address_known_non_overlap = lsq.classify_load(2, 0x80002000, 4);
    if (!expect(address_known_non_overlap.state == LsqLoadState::None &&
                    !address_known_non_overlap.blocks_issue(),
                "address-known non-overlapping younger load should no longer be blocked by unresolved-store semantics")) {
        return 1;
    }
    const LsqLoadStatus address_known_overlap = lsq.classify_load(2, 0x80001000, 1);
    if (!expect(address_known_overlap.state == LsqLoadState::BlockedByOverlappingStore &&
                    address_known_overlap.store_sequence_id == 1 &&
                    address_known_overlap.blocks_issue(),
                "address-known overlapping younger load should report overlapping-store block even before store data is ready")) {
        return 1;
    }

    lsq.mark_data_ready(older_store, 0x11);
    const auto older_store_entry = lsq.peek(older_store);
    if (!expect(older_store_entry.has_value() && older_store_entry->address_ready &&
                    older_store_entry->data_ready && !older_store_entry->order_ready,
                "store entry should expose address/data readiness before it becomes order-ready")) {
        return 1;
    }
    const LsqLoadStatus non_overlap_status = lsq.classify_load(2, 0x80002000, 4);
    if (!expect(non_overlap_status.state == LsqLoadState::None && !non_overlap_status.blocks_issue(),
                "non-overlapping younger load should stop blocking once the older store address and data are known")) {
        return 1;
    }
    const LsqLoadStatus overlap_block = lsq.classify_load(2, 0x80001000, 1);
    if (!expect(overlap_block.state == LsqLoadState::BlockedByOverlappingStore &&
                    overlap_block.store_sequence_id == 1 &&
                    overlap_block.blocks_issue(),
                "overlapping younger load should report an explicit overlap block until the older store reaches the release point")) {
        return 1;
    }

    lsq.mark_order_ready(older_store);
    if (!expect(lsq.classify_load(2, 0x80001000, 1).state == LsqLoadState::None,
                "overlapping younger load should be released once the older store becomes order-ready")) {
        return 1;
    }

    const auto released_store_entry = lsq.peek(older_store);
    if (!expect(released_store_entry.has_value() && released_store_entry->order_ready,
                "LSQ should retain the store order-ready state for debugging and scheduling")) {
        return 1;
    }

    const auto retired_older_store = lsq.retire_entry(older_store);
    if (!expect(retired_older_store.has_value() && lsq.size() == 0,
                "retire_entry should remove an order-ready store entry from the queue")) {
        return 1;
    }

    const LsqIndex load = lsq.enqueue_load({
        .sequence_id = 1,
        .rd = 5,
        .size = 4,
    });
    const LsqIndex store = lsq.enqueue_store({
        .sequence_id = 2,
        .size = 8,
        .mmio = true,
        .non_speculative = true,
    });

    lsq.mark_address_ready(load, 0x1000);
    lsq.mark_order_ready(load);
    lsq.mark_data_ready(load, 0x12345678ULL);
    lsq.mark_address_ready(store, 0x2000);
    lsq.mark_data_ready(store, 0xfeedfaceULL);
    lsq.mark_order_ready(store);

    const auto load_entry = lsq.peek(load);
    if (!expect(load_entry.has_value() && load_entry->kind == LsqEntryKind::Load &&
                    load_entry->address_ready && load_entry->order_ready &&
                    load_entry->address == 0x1000 &&
                    load_entry->data_ready && load_entry->data == 0x12345678ULL,
                "LSQ should retain load readiness and loaded value")) {
        return 1;
    }

    const auto store_entry = lsq.peek(store);
    if (!expect(store_entry.has_value() && store_entry->kind == LsqEntryKind::Store &&
                    store_entry->address_ready && store_entry->data_ready &&
                    store_entry->data == 0xfeedfaceULL &&
                    store_entry->mmio && store_entry->non_speculative,
                "LSQ should retain store readiness and non-speculative flags")) {
        return 1;
    }

    const auto retired_load = lsq.retire_entry(load);
    if (!expect(retired_load.has_value() && retired_load->kind == LsqEntryKind::Load && lsq.size() == 1,
                "retire_entry should remove a ready load entry from the queue")) {
        return 1;
    }

    const auto retired_store = lsq.retire_entry(store);
    if (!expect(retired_store.has_value() && retired_store->sequence_id == 2 && lsq.size() == 0,
                "retire_entry should remove a ready store entry from the queue")) {
        return 1;
    }

    const LsqIndex preserved = lsq.enqueue_load({
        .sequence_id = 1,
        .rd = 6,
        .size = 4,
    });
    lsq.mark_address_ready(preserved, 0x3000);
    lsq.mark_order_ready(preserved);
    lsq.mark_data_ready(preserved, 0xa5a5a5a5ULL);
    lsq.enqueue_store({
        .sequence_id = 3,
        .size = 4,
    });
    lsq.flush_younger_than(1);
    if (!expect(lsq.size() == 1 && lsq.peek(preserved).has_value(),
                "flush_younger_than should drop younger LSQ entries while preserving older ones")) {
        return 1;
    }

    LoadStoreQueue replay_lsq;
    const LsqIndex replay_store = replay_lsq.enqueue_store({
        .sequence_id = 1,
        .size = 4,
    });
    const LsqIndex replay_load = replay_lsq.enqueue_load({
        .sequence_id = 2,
        .rd = 6,
        .size = 4,
    });
    replay_lsq.mark_address_ready(replay_load, 0x4000);
    replay_lsq.mark_data_ready(replay_load, 0xdeadbeefULL);
    replay_lsq.mark_order_ready(replay_load);

    if (!expect(replay_lsq.active_replay().state == LsqLoadState::None,
                "a younger load should not report replay before any late-overlap store arrives")) {
        return 1;
    }

    replay_lsq.mark_address_ready(replay_store, 0x4000);
    const LsqLoadStatus replay_required = replay_lsq.active_replay();
    if (!expect(replay_required.state == LsqLoadState::ReplayRequired &&
                    replay_required.load_sequence_id == 2 &&
                    replay_required.store_sequence_id == 1 &&
                    replay_required.replay_required(),
                "late overlap should mark the already-issued younger load as replay-required")) {
        return 1;
    }

    const LsqLoadStatus replay_lookup = replay_lsq.classify_load(2, 0x4000, 4);
    if (!expect(replay_lookup.state == LsqLoadState::ReplayRequired &&
                    replay_lookup.store_sequence_id == 1,
                "load classification should surface replay-required once the violating older store address appears")) {
        return 1;
    }

    replay_lsq.flush_younger_than(1);
    if (!expect(replay_lsq.active_replay().state == LsqLoadState::None,
                "flushing the violating younger load should also clear replay-required state")) {
        return 1;
    }

    LoadStoreQueue forwarding_lsq;
    const LsqIndex forwarding_store = forwarding_lsq.enqueue_store({
        .sequence_id = 1,
        .size = 4,
    });
    forwarding_lsq.mark_address_ready(forwarding_store, 0x80001000ULL);
    forwarding_lsq.mark_data_ready(forwarding_store, 0xaabbccddULL);
    forwarding_lsq.mark_order_ready(forwarding_store);
    const auto forwarded_word = forwarding_lsq.forwardable_load(bus, 2, 0x80001000ULL, 4);
    if (!expect(forwarded_word.has_value() && forwarded_word->value == 0xaabbccddULL &&
                    forwarded_word->store_sequence_id == 1,
                "full-cover older RAM store should provide a forwarding value for the younger load")) {
        return 1;
    }
    const auto forwarded_byte = forwarding_lsq.forwardable_load(bus, 2, 0x80001001ULL, 1);
    if (!expect(forwarded_byte.has_value() && forwarded_byte->value == 0xccULL,
                "forwarding helper should extract the requested byte range from a covering older store")) {
        return 1;
    }

    LoadStoreQueue overshadowed_lsq;
    const LsqIndex older_covering_store = overshadowed_lsq.enqueue_store({
        .sequence_id = 1,
        .size = 4,
    });
    overshadowed_lsq.mark_address_ready(older_covering_store, 0x80002000ULL);
    overshadowed_lsq.mark_data_ready(older_covering_store, 0x11223344ULL);
    overshadowed_lsq.mark_order_ready(older_covering_store);
    const LsqIndex newer_partial_store = overshadowed_lsq.enqueue_store({
        .sequence_id = 2,
        .size = 1,
    });
    overshadowed_lsq.mark_address_ready(newer_partial_store, 0x80002000ULL);
    overshadowed_lsq.mark_data_ready(newer_partial_store, 0x55ULL);
    overshadowed_lsq.mark_order_ready(newer_partial_store);
    if (!expect(!overshadowed_lsq.forwardable_load(bus, 3, 0x80002000ULL, 4).has_value(),
                "a nearer overlapping store that cannot fully cover the load must block fallback to an older store")) {
        return 1;
    }

    return 0;
}
