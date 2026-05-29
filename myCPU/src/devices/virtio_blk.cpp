#include "virtio_blk.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include "virtqueue.h"
#include "../mem/bus.h"

namespace {

constexpr uint16_t kQueueSize = 8;
constexpr uint32_t kCapacityConfigBytes = 8;

bool is_valid_config_access(uint32_t offset, int size) {
    if (size != 1 && size != 2 && size != 4 && size != 8) {
        return false;
    }
    return offset + static_cast<uint32_t>(size) <= kCapacityConfigBytes;
}

}  // namespace

void VirtioBlk::load_image(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::string("failed to open virtio-blk image: ") + path);
    }

    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    if (size < 0) {
        throw std::runtime_error(std::string("failed to size virtio-blk image: ") + path);
    }

    const uint64_t byte_count = static_cast<uint64_t>(size);
    const uint64_t padded_size =
        ((byte_count + VIRTIO_BLK_SECTOR_SIZE - 1) / VIRTIO_BLK_SECTOR_SIZE) * VIRTIO_BLK_SECTOR_SIZE;
    data_.assign(static_cast<size_t>(padded_size), 0);
    file.seekg(0, std::ios::beg);
    if (size > 0 && !file.read(reinterpret_cast<char*>(data_.data()), size)) {
        throw std::runtime_error(std::string("short read while loading virtio-blk image: ") + path);
    }
    attached_ = true;
}

void VirtioBlk::clear_image() {
    data_.clear();
    attached_ = false;
}

bool VirtioBlk::attached() const {
    return attached_;
}

uint64_t VirtioBlk::capacity_sectors() const {
    return data_.size() / VIRTIO_BLK_SECTOR_SIZE;
}

uint32_t VirtioBlk::device_id() const {
    return VIRTIO_DEVICE_ID_BLOCK;
}

uint32_t VirtioBlk::num_queues() const {
    return 1;
}

uint16_t VirtioBlk::queue_size(uint32_t queue_index) const {
    return queue_index == 0 ? kQueueSize : 0;
}

uint32_t VirtioBlk::device_features(uint32_t word) const {
    if (word == VIRTIO_F_VERSION_1_WORD) {
        return VIRTIO_F_VERSION_1_MASK;
    }
    return 0;
}

void VirtioBlk::reset() {}

bool VirtioBlk::read_config(uint32_t offset, int size, uint64_t& value) const {
    value = 0;
    if (!is_valid_config_access(offset, size)) {
        return false;
    }

    const uint64_t capacity = capacity_sectors();
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&capacity);
    for (int i = 0; i < size; ++i) {
        value |= static_cast<uint64_t>(bytes[offset + static_cast<uint32_t>(i)]) << (8 * i);
    }
    return true;
}

bool VirtioBlk::notify_queue(Bus& bus,
                             uint32_t queue_index,
                             VirtQueue& queue,
                             uint32_t& completed,
                             std::string& error) {
    completed = 0;
    error.clear();
    if (queue_index != 0) {
        error = "virtio-blk only exposes queue 0";
        return false;
    }

    while (queue.has_pending(bus, error)) {
        VirtQueue::Chain chain;
        if (!queue.pop_chain(bus, chain, error)) {
            return false;
        }

        uint32_t used_len = 0;
        if (!process_chain(bus, chain, used_len, error)) {
            return false;
        }
        if (!queue.push_used(bus, chain.head_index, used_len, error)) {
            return false;
        }
        if (!queue.commit_chain(chain, error)) {
            return false;
        }
        ++completed;
    }
    return error.empty();
}

bool VirtioBlk::process_chain(Bus& bus,
                              const VirtQueue::Chain& chain,
                              uint32_t& used_len,
                              std::string& error) {
    used_len = 0;
    error.clear();

    if (chain.descriptors.size() < 2) {
        error = "virtio-blk request chain is too short";
        return false;
    }

    const VirtQueue::Descriptor& header_desc = chain.descriptors.front();
    const VirtQueue::Descriptor& status_desc = chain.descriptors.back();
    if ((header_desc.flags & VIRTQ_DESC_F_WRITE) != 0 || header_desc.len < sizeof(RequestHeader)) {
        error = "virtio-blk request header descriptor is invalid";
        return false;
    }
    if ((status_desc.flags & VIRTQ_DESC_F_WRITE) == 0 || status_desc.len < 1) {
        error = "virtio-blk status descriptor is invalid";
        return false;
    }

    RequestHeader header{};
    if (!bus.dma_load_bytes(header_desc.addr, &header, sizeof(header))) {
        error = "virtio-blk failed to DMA-load request header";
        return complete_with_status(bus,
                                    status_desc,
                                    VIRTIO_BLK_S_IOERR,
                                    used_len,
                                    error);
    }

    uint64_t total_data_len = 0;
    for (size_t i = 1; i + 1 < chain.descriptors.size(); ++i) {
        const VirtQueue::Descriptor& desc = chain.descriptors[i];
        const bool writable = (desc.flags & VIRTQ_DESC_F_WRITE) != 0;
        if (header.type == VIRTIO_BLK_T_IN && !writable) {
            error = "virtio-blk read request requires writable data descriptors";
            return false;
        }
        if (header.type == VIRTIO_BLK_T_OUT && writable) {
            error = "virtio-blk write request requires readable data descriptors";
            return false;
        }
        total_data_len += desc.len;
    }

    uint8_t status = VIRTIO_BLK_S_OK;
    uint64_t image_offset = header.sector * static_cast<uint64_t>(VIRTIO_BLK_SECTOR_SIZE);
    if (!attached_ || image_offset > data_.size() || total_data_len > data_.size() - image_offset) {
        status = VIRTIO_BLK_S_IOERR;
    } else if (header.type != VIRTIO_BLK_T_IN && header.type != VIRTIO_BLK_T_OUT) {
        status = VIRTIO_BLK_S_UNSUPP;
    } else if (header.type == VIRTIO_BLK_T_IN) {
        uint64_t copied = 0;
        for (size_t i = 1; i + 1 < chain.descriptors.size(); ++i) {
            const VirtQueue::Descriptor& desc = chain.descriptors[i];
            if (!bus.dma_store_bytes(desc.addr, data_.data() + image_offset + copied, desc.len)) {
                return complete_with_status(bus,
                                            status_desc,
                                            VIRTIO_BLK_S_IOERR,
                                            used_len,
                                            error);
            }
            copied += desc.len;
        }
        used_len = static_cast<uint32_t>(total_data_len);
    } else if (header.type == VIRTIO_BLK_T_OUT) {
        uint64_t copied = 0;
        for (size_t i = 1; i + 1 < chain.descriptors.size(); ++i) {
            const VirtQueue::Descriptor& desc = chain.descriptors[i];
            if (!bus.dma_load_bytes(desc.addr, data_.data() + image_offset + copied, desc.len)) {
                return complete_with_status(bus,
                                            status_desc,
                                            VIRTIO_BLK_S_IOERR,
                                            used_len,
                                            error);
            }
            copied += desc.len;
        }
    }

    return complete_with_status(bus, status_desc, status, used_len, error);
}

bool VirtioBlk::complete_with_status(Bus& bus,
                                     const VirtQueue::Descriptor& status_desc,
                                     uint8_t status,
                                     uint32_t& used_len,
                                     std::string& error) const {
    if (!write_status(bus, status_desc.addr, status, error)) {
        return false;
    }
    used_len += 1;
    return true;
}

bool VirtioBlk::write_status(Bus& bus, uint64_t addr, uint8_t status, std::string& error) const {
    if (!bus.dma_store_bytes(addr, &status, sizeof(status))) {
        error = "virtio-blk failed to DMA-store status byte";
        return false;
    }
    return true;
}
