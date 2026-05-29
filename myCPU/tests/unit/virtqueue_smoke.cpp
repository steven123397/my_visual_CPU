#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "../../src/devices/virtqueue.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../src/platform/address_map.h"

namespace {

struct PackedDesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

int fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

void write_bytes(Ram& ram, uint64_t addr, const void* data, size_t size) {
    ram.write_bytes(addr, data, size);
}

void write_u16(Ram& ram, uint64_t addr, uint16_t value) {
    write_bytes(ram, addr, &value, sizeof(value));
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    try {
        Ram ram;
        Bus bus(ram);
        VirtQueue queue(8);
        std::string error;

        const uint64_t desc_addr = MEM_BASE + 0x1000;
        const uint64_t avail_addr = MEM_BASE + 0x2000;
        const uint64_t used_addr = MEM_BASE + 0x3000;
        const uint64_t data_addr = MEM_BASE + 0x4000;
        const uint64_t status_addr = MEM_BASE + 0x5000;

        queue.set_size(8);
        queue.set_desc_addr(desc_addr);
        queue.set_avail_addr(avail_addr);
        queue.set_used_addr(used_addr);
        queue.set_ready(true);

        const PackedDesc desc0{data_addr, 16, VIRTQ_DESC_F_NEXT, 1};
        const PackedDesc desc1{status_addr, 1, VIRTQ_DESC_F_WRITE, 0};
        write_bytes(ram, desc_addr + 0 * sizeof(PackedDesc), &desc0, sizeof(desc0));
        write_bytes(ram, desc_addr + 1 * sizeof(PackedDesc), &desc1, sizeof(desc1));
        write_u16(ram, avail_addr + 2, 1);
        write_u16(ram, avail_addr + 4, 0);

        if (!expect(queue.ready(), "expected queue ready flag") ||
            !expect(queue.configured(), "expected queue to be configured") ||
            !expect(queue.has_pending(bus, error), "expected queue to report pending heads") ||
            !expect(error.empty(), "expected pending check to succeed without error")) {
            return 1;
        }

        VirtQueue::Chain chain;
        if (!expect(queue.pop_chain(bus, chain, error), "expected descriptor chain pop to succeed") ||
            !expect(error.empty(), "expected empty error after chain pop") ||
            !expect(chain.head_index == 0, "expected head index 0") ||
            !expect(chain.descriptors.size() == 2, "expected two descriptors in chain") ||
            !expect(chain.descriptors[0].addr == data_addr, "expected first descriptor payload address") ||
            !expect(chain.descriptors[0].len == 16, "expected first descriptor length") ||
            !expect(chain.descriptors[1].addr == status_addr, "expected second descriptor address") ||
            !expect(chain.descriptors[1].flags == VIRTQ_DESC_F_WRITE,
                    "expected second descriptor write-only flag")) {
            return 1;
        }

        if (!expect(queue.has_pending(bus, error), "expected queue to stay pending until commit") ||
            !expect(error.empty(), "expected no error before queue commit") ||
            !expect(queue.push_used(bus, chain.head_index, 17, error), "expected used ring push to succeed") ||
            !expect(error.empty(), "expected used ring push without error") ||
            !expect(queue.commit_chain(chain, error), "expected queue commit to succeed") ||
            !expect(error.empty(), "expected queue commit without error") ||
            !expect(!queue.has_pending(bus, error), "expected queue to be empty after commit") ||
            !expect(error.empty(), "expected no error after queue drained")) {
            return 1;
        }

        uint64_t used_idx = 0;
        uint64_t used_head = 0;
        uint64_t used_len = 0;
        if (!expect(bus.try_load(used_addr + 2, 2, used_idx) && used_idx == 1,
                    "expected used idx increment") ||
            !expect(bus.try_load(used_addr + 4, 4, used_head) && used_head == 0,
                    "expected used ring head id") ||
            !expect(bus.try_load(used_addr + 8, 4, used_len) && used_len == 17,
                    "expected used ring length")) {
            return 1;
        }

        write_bytes(ram, desc_addr + 0 * sizeof(PackedDesc), &desc0, sizeof(desc0));
        write_bytes(ram, desc_addr + 1 * sizeof(PackedDesc), &desc1, sizeof(desc1));
        write_u16(ram, avail_addr + 2, 2);
        write_u16(ram, avail_addr + 6, 0);

        if (!expect(queue.pop_chain(bus, chain, error), "expected second chain pop to succeed") ||
            !expect(error.empty(), "expected second chain pop without error")) {
            return 1;
        }
        queue.set_used_addr(MEM_BASE + MEM_SIZE);
        if (queue.push_used(bus, chain.head_index, 17, error)) {
            return fail("expected unmapped used ring writeback to fail");
        }
        if (!expect(queue.has_pending(bus, error),
                    "expected queue to keep avail entry pending after used writeback failure")) {
            return 1;
        }
        queue.set_used_addr(used_addr);
        if (!expect(queue.push_used(bus, chain.head_index, 17, error),
                    "expected used ring retry to succeed") ||
            !expect(queue.commit_chain(chain, error), "expected queue commit after retry to succeed")) {
            return 1;
        }
        if (!expect(bus.try_load(used_addr + 2, 2, used_idx) && used_idx == 2,
                    "expected used idx increment after retried writeback")) {
            return 1;
        }

        const PackedDesc loop_desc0{data_addr, 8, VIRTQ_DESC_F_NEXT, 1};
        const PackedDesc loop_desc1{status_addr, 1, VIRTQ_DESC_F_NEXT, 0};
        write_bytes(ram, desc_addr + 0 * sizeof(PackedDesc), &loop_desc0, sizeof(loop_desc0));
        write_bytes(ram, desc_addr + 1 * sizeof(PackedDesc), &loop_desc1, sizeof(loop_desc1));
        write_u16(ram, avail_addr + 2, 3);
        write_u16(ram, avail_addr + 4 + 4, 0);

        if (queue.pop_chain(bus, chain, error)) {
            return fail("expected looped descriptor chain to be rejected");
        }
        if (error.empty()) {
            return fail("expected loop rejection to report an error");
        }

        queue.set_avail_addr(UART_BASE);
        error.clear();
        if (queue.has_pending(bus, error)) {
            return fail("expected MMIO avail ring to be rejected");
        }
        if (error.empty()) {
            return fail("expected MMIO avail ring rejection to report an error");
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
