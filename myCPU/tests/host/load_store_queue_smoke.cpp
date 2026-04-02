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
    lsq.mark_data_ready(load, 0x12345678ULL);
    lsq.mark_address_ready(store, 0x2000);
    lsq.mark_data_ready(store, 0xfeedfaceULL);

    const auto load_entry = lsq.peek(load);
    if (!expect(load_entry.has_value() && load_entry->kind == LsqEntryKind::Load &&
                    load_entry->address_ready && load_entry->address == 0x1000 &&
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
