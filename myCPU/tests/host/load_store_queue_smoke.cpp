#include <cstdio>

#include "../../src/exec/load_store_queue.h"

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
    LoadStoreQueue lsq;
    const LsqIndex older_store = lsq.enqueue_store({
        .sequence_id = 1,
        .size = 4,
        .non_speculative = true,
    });
    if (!expect(lsq.has_blocking_older_store(2, 0x80001000, 4),
                "older unresolved store should block a younger load from issuing")) {
        return 1;
    }

    lsq.mark_address_ready(older_store, 0x80001000);
    if (!expect(lsq.has_blocking_older_store(2, 0x80002000, 4),
                "older store should keep blocking while its data is still unresolved")) {
        return 1;
    }

    lsq.mark_data_ready(older_store, 0x11);
    const auto older_store_entry = lsq.peek(older_store);
    if (!expect(older_store_entry.has_value() && older_store_entry->address_ready &&
                    older_store_entry->data_ready && !older_store_entry->order_ready,
                "store entry should expose address/data readiness before it becomes order-ready")) {
        return 1;
    }
    if (!expect(!lsq.has_blocking_older_store(2, 0x80002000, 4),
                "non-overlapping younger load should stop blocking once the older store address and data are known")) {
        return 1;
    }
    if (!expect(lsq.has_blocking_older_store(2, 0x80001000, 1),
                "overlapping younger load should keep waiting until the older store reaches the release point")) {
        return 1;
    }

    lsq.mark_order_ready(older_store);
    if (!expect(!lsq.has_blocking_older_store(2, 0x80001000, 1),
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

    return 0;
}
