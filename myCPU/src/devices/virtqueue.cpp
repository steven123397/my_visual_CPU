#include "virtqueue.h"

#include <cstring>

#include "../mem/bus.h"

namespace {

constexpr uint64_t kDescriptorSize = 16;

}  // namespace

VirtQueue::VirtQueue(uint16_t max_size) : max_size_(max_size) {}

void VirtQueue::reset() {
    size_ = 0;
    desc_addr_ = 0;
    avail_addr_ = 0;
    used_addr_ = 0;
    ready_ = false;
    next_avail_idx_ = 0;
}

uint16_t VirtQueue::max_size() const {
    return max_size_;
}

uint16_t VirtQueue::size() const {
    return size_;
}

bool VirtQueue::set_size(uint16_t size) {
    if (size > max_size_) {
        return false;
    }
    size_ = size;
    next_avail_idx_ = 0;
    return true;
}

uint64_t VirtQueue::desc_addr() const {
    return desc_addr_;
}

uint64_t VirtQueue::avail_addr() const {
    return avail_addr_;
}

uint64_t VirtQueue::used_addr() const {
    return used_addr_;
}

void VirtQueue::set_desc_addr(uint64_t addr) {
    desc_addr_ = addr;
}

void VirtQueue::set_avail_addr(uint64_t addr) {
    avail_addr_ = addr;
}

void VirtQueue::set_used_addr(uint64_t addr) {
    used_addr_ = addr;
}

bool VirtQueue::ready() const {
    return ready_;
}

void VirtQueue::set_ready(bool ready) {
    ready_ = ready;
    if (!ready_) {
        next_avail_idx_ = 0;
    }
}

bool VirtQueue::configured() const {
    return ready_ && size_ != 0 && desc_addr_ != 0 && avail_addr_ != 0 && used_addr_ != 0;
}

bool VirtQueue::has_pending(Bus& bus, std::string& error) const {
    error.clear();
    if (!configured()) {
        return false;
    }

    uint16_t avail_idx = 0;
    if (!load_u16(bus, avail_addr_ + 2, avail_idx, error)) {
        return false;
    }
    return avail_idx != next_avail_idx_;
}

bool VirtQueue::pop_chain(Bus& bus, Chain& chain, std::string& error) {
    error.clear();
    chain = {};

    if (!configured()) {
        error = "virtqueue is not configured";
        return false;
    }

    uint16_t avail_idx = 0;
    if (!load_u16(bus, avail_addr_ + 2, avail_idx, error)) {
        return false;
    }
    if (avail_idx == next_avail_idx_) {
        return false;
    }

    uint16_t head_index = 0;
    const uint16_t chain_avail_idx = next_avail_idx_;
    const uint64_t ring_addr = avail_addr_ + 4 + 2 * static_cast<uint64_t>(chain_avail_idx % size_);
    if (!load_u16(bus, ring_addr, head_index, error)) {
        return false;
    }
    if (head_index >= size_) {
        error = "virtqueue avail head out of range";
        return false;
    }

    std::vector<bool> visited(size_, false);
    uint16_t current = head_index;
    while (true) {
        if (current >= size_) {
            error = "virtqueue descriptor next index out of range";
            return false;
        }
        if (visited[current]) {
            error = "virtqueue descriptor chain contains a loop";
            return false;
        }
        visited[current] = true;

        Descriptor descriptor;
        if (!load_descriptor(bus, current, descriptor, error)) {
            return false;
        }
        chain.descriptors.push_back(descriptor);

        if ((descriptor.flags & VIRTQ_DESC_F_NEXT) == 0) {
            break;
        }
        current = descriptor.next;
        if (chain.descriptors.size() > size_) {
            error = "virtqueue descriptor chain exceeds queue size";
            return false;
        }
    }

    chain.head_index = head_index;
    chain.avail_index = chain_avail_idx;
    return true;
}

bool VirtQueue::commit_chain(const Chain& chain, std::string& error) {
    error.clear();
    if (!configured()) {
        error = "virtqueue is not configured";
        return false;
    }

    if (chain.head_index >= size_) {
        error = "virtqueue commit head out of range";
        return false;
    }
    if (chain.avail_index != next_avail_idx_) {
        error = "virtqueue commit index mismatch";
        return false;
    }

    ++next_avail_idx_;
    return true;
}

bool VirtQueue::push_used(Bus& bus, uint16_t head_index, uint32_t len, std::string& error) {
    error.clear();
    if (!configured()) {
        error = "virtqueue is not configured";
        return false;
    }

    uint16_t used_idx = 0;
    if (!load_u16(bus, used_addr_ + 2, used_idx, error)) {
        return false;
    }

    const uint64_t elem_addr = used_addr_ + 4 + 8 * static_cast<uint64_t>(used_idx % size_);
    if (!store_u32(bus, elem_addr, head_index, error) ||
        !store_u32(bus, elem_addr + 4, len, error) ||
        !store_u16(bus, used_addr_ + 2, static_cast<uint16_t>(used_idx + 1), error)) {
        return false;
    }
    return true;
}

bool VirtQueue::load_u16(Bus& bus, uint64_t addr, uint16_t& value, std::string& error) const {
    value = 0;
    if (!bus.dma_load_bytes(addr, &value, sizeof(value))) {
        error = "virtqueue DMA load failed";
        return false;
    }
    return true;
}

bool VirtQueue::load_descriptor(Bus& bus, uint16_t index, Descriptor& descriptor, std::string& error) const {
    descriptor = {};
    const uint64_t addr = desc_addr_ + kDescriptorSize * static_cast<uint64_t>(index);
    if (!bus.dma_load_bytes(addr, &descriptor, sizeof(descriptor))) {
        error = "virtqueue descriptor DMA load failed";
        return false;
    }
    if ((descriptor.flags & VIRTQ_DESC_F_INDIRECT) != 0) {
        error = "virtqueue indirect descriptors are not supported yet";
        return false;
    }
    return true;
}

bool VirtQueue::store_u16(Bus& bus, uint64_t addr, uint16_t value, std::string& error) const {
    if (!bus.dma_store_bytes(addr, &value, sizeof(value))) {
        error = "virtqueue DMA store failed";
        return false;
    }
    return true;
}

bool VirtQueue::store_u32(Bus& bus, uint64_t addr, uint32_t value, std::string& error) const {
    if (!bus.dma_store_bytes(addr, &value, sizeof(value))) {
        error = "virtqueue DMA store failed";
        return false;
    }
    return true;
}
