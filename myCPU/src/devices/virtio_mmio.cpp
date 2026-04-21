#include "virtio_mmio.h"

#include <stdexcept>

#include "plic.h"
#include "../mem/bus.h"

namespace {

bool is_valid_mmio_width(int size) {
    return size == 4;
}

bool is_valid_config_width(int size) {
    return size == 1 || size == 2 || size == 4 || size == 8;
}

std::vector<VirtQueue> make_queues(const VirtioDevice& device) {
    std::vector<VirtQueue> queues;
    queues.reserve(device.num_queues());
    for (uint32_t i = 0; i < device.num_queues(); ++i) {
        queues.emplace_back(device.queue_size(i));
    }
    return queues;
}

}  // namespace

VirtioMmio::VirtioMmio(Plic& plic,
                       uint32_t irq_source,
                       VirtioDevice& device,
                       uint64_t base,
                       uint64_t size)
    : Device(base, size),
      plic_(plic),
      irq_source_(irq_source),
      device_(device),
      queues_(make_queues(device)) {}

void VirtioMmio::bind_bus(Bus& bus) {
    bus_ = &bus;
}

uint64_t VirtioMmio::load(uint64_t addr, int size) {
    const uint32_t offset = static_cast<uint32_t>(addr - base());
    if (offset >= VIRTIO_MMIO_CONFIG_SPACE_OFFSET) {
        if (!is_valid_config_width(size)) {
            invalid_access(addr, size);
        }
        uint64_t value = 0;
        if (!device_.read_config(offset - VIRTIO_MMIO_CONFIG_SPACE_OFFSET, size, value)) {
            invalid_access(addr, size);
        }
        return value;
    }

    if (!is_valid_mmio_width(size)) {
        invalid_access(addr, size);
    }

    switch (offset) {
    case VIRTIO_MMIO_REG_MAGIC_VALUE:
        return VIRTIO_MMIO_MAGIC_VALUE;
    case VIRTIO_MMIO_REG_VERSION:
        return VIRTIO_MMIO_VERSION_VALUE;
    case VIRTIO_MMIO_REG_DEVICE_ID:
        return device_.device_id();
    case VIRTIO_MMIO_REG_VENDOR_ID:
        return device_.vendor_id();
    case VIRTIO_MMIO_REG_DEVICE_FEATURES:
        return read_device_features();
    case VIRTIO_MMIO_REG_DEVICE_FEATURES_SEL:
        return device_features_sel_;
    case VIRTIO_MMIO_REG_DRIVER_FEATURES:
        return read_driver_features();
    case VIRTIO_MMIO_REG_DRIVER_FEATURES_SEL:
        return driver_features_sel_;
    case VIRTIO_MMIO_REG_QUEUE_SEL:
        return queue_sel_;
    case VIRTIO_MMIO_REG_QUEUE_NUM_MAX: {
        const VirtQueue* queue = selected_queue();
        return queue != nullptr ? queue->max_size() : 0;
    }
    case VIRTIO_MMIO_REG_QUEUE_NUM: {
        const VirtQueue* queue = selected_queue();
        return queue != nullptr ? queue->size() : 0;
    }
    case VIRTIO_MMIO_REG_QUEUE_READY: {
        const VirtQueue* queue = selected_queue();
        return queue != nullptr && queue->ready() ? 1 : 0;
    }
    case VIRTIO_MMIO_REG_INTERRUPT_STATUS:
        return interrupt_status_;
    case VIRTIO_MMIO_REG_STATUS:
        return status_;
    case VIRTIO_MMIO_REG_QUEUE_DESC_LOW: {
        const VirtQueue* queue = selected_queue();
        return queue != nullptr ? static_cast<uint32_t>(queue->desc_addr()) : 0;
    }
    case VIRTIO_MMIO_REG_QUEUE_DESC_HIGH: {
        const VirtQueue* queue = selected_queue();
        return queue != nullptr ? static_cast<uint32_t>(queue->desc_addr() >> 32) : 0;
    }
    case VIRTIO_MMIO_REG_QUEUE_DRIVER_LOW: {
        const VirtQueue* queue = selected_queue();
        return queue != nullptr ? static_cast<uint32_t>(queue->avail_addr()) : 0;
    }
    case VIRTIO_MMIO_REG_QUEUE_DRIVER_HIGH: {
        const VirtQueue* queue = selected_queue();
        return queue != nullptr ? static_cast<uint32_t>(queue->avail_addr() >> 32) : 0;
    }
    case VIRTIO_MMIO_REG_QUEUE_DEVICE_LOW: {
        const VirtQueue* queue = selected_queue();
        return queue != nullptr ? static_cast<uint32_t>(queue->used_addr()) : 0;
    }
    case VIRTIO_MMIO_REG_QUEUE_DEVICE_HIGH: {
        const VirtQueue* queue = selected_queue();
        return queue != nullptr ? static_cast<uint32_t>(queue->used_addr() >> 32) : 0;
    }
    case VIRTIO_MMIO_REG_CONFIG_GENERATION:
        return 0;
    default:
        invalid_access(addr, size);
    }
}

void VirtioMmio::store(uint64_t addr, uint64_t value, int size) {
    const uint32_t offset = static_cast<uint32_t>(addr - base());
    if (offset >= VIRTIO_MMIO_CONFIG_SPACE_OFFSET) {
        if (!is_valid_config_width(size) || !device_.write_config(offset - VIRTIO_MMIO_CONFIG_SPACE_OFFSET, value, size)) {
            invalid_access(addr, size);
        }
        return;
    }

    if (!is_valid_mmio_width(size)) {
        invalid_access(addr, size);
    }

    switch (offset) {
    case VIRTIO_MMIO_REG_DEVICE_FEATURES_SEL:
        device_features_sel_ = static_cast<uint32_t>(value);
        return;
    case VIRTIO_MMIO_REG_DRIVER_FEATURES_SEL:
        driver_features_sel_ = static_cast<uint32_t>(value);
        return;
    case VIRTIO_MMIO_REG_DRIVER_FEATURES: {
        const uint32_t shift = (driver_features_sel_ & 1U) * 32U;
        driver_features_ &= ~(UINT64_C(0xffffffff) << shift);
        driver_features_ |= (static_cast<uint64_t>(static_cast<uint32_t>(value)) << shift);
        return;
    }
    case VIRTIO_MMIO_REG_QUEUE_SEL:
        queue_sel_ = static_cast<uint32_t>(value);
        return;
    case VIRTIO_MMIO_REG_QUEUE_NUM: {
        VirtQueue* queue = selected_queue();
        if (queue != nullptr) {
            queue->set_size(static_cast<uint16_t>(value));
        }
        return;
    }
    case VIRTIO_MMIO_REG_QUEUE_READY: {
        VirtQueue* queue = selected_queue();
        if (queue != nullptr) {
            queue->set_ready(value != 0);
        }
        return;
    }
    case VIRTIO_MMIO_REG_QUEUE_NOTIFY: {
        if (bus_ == nullptr) {
            throw std::runtime_error("virtio-mmio bus not bound");
        }
        const uint32_t queue_index = static_cast<uint32_t>(value);
        if (queue_index >= queues_.size()) {
            return;
        }
        std::string error;
        uint32_t completed = 0;
        if (!device_.notify_queue(*bus_, queue_index, queues_[queue_index], completed, error)) {
            throw std::runtime_error(error.empty() ? "virtio-mmio queue notify failed" : error);
        }
        if (completed != 0) {
            interrupt_status_ |= VIRTIO_MMIO_INTERRUPT_USED_BUFFER;
            update_interrupt_line();
        }
        return;
    }
    case VIRTIO_MMIO_REG_INTERRUPT_ACK:
        interrupt_status_ &= ~static_cast<uint32_t>(value);
        update_interrupt_line();
        return;
    case VIRTIO_MMIO_REG_STATUS:
        status_ = static_cast<uint32_t>(value) & 0xFFU;
        if (status_ == 0) {
            reset_transport();
        }
        return;
    case VIRTIO_MMIO_REG_QUEUE_DESC_LOW:
    case VIRTIO_MMIO_REG_QUEUE_DESC_HIGH:
    case VIRTIO_MMIO_REG_QUEUE_DRIVER_LOW:
    case VIRTIO_MMIO_REG_QUEUE_DRIVER_HIGH:
    case VIRTIO_MMIO_REG_QUEUE_DEVICE_LOW:
    case VIRTIO_MMIO_REG_QUEUE_DEVICE_HIGH: {
        VirtQueue* queue = selected_queue();
        if (queue != nullptr) {
            write_queue_address(*queue, offset, static_cast<uint32_t>(value));
        }
        return;
    }
    default:
        invalid_access(addr, size);
    }
}

VirtQueue* VirtioMmio::selected_queue() {
    return queue_sel_ < queues_.size() ? &queues_[queue_sel_] : nullptr;
}

const VirtQueue* VirtioMmio::selected_queue() const {
    return queue_sel_ < queues_.size() ? &queues_[queue_sel_] : nullptr;
}

uint32_t VirtioMmio::read_device_features() const {
    return device_.device_features(device_features_sel_);
}

uint32_t VirtioMmio::read_driver_features() const {
    const uint32_t shift = (driver_features_sel_ & 1U) * 32U;
    return static_cast<uint32_t>((driver_features_ >> shift) & UINT64_C(0xffffffff));
}

uint64_t VirtioMmio::read_queue_address(const VirtQueue& queue, uint32_t low_reg) const {
    switch (low_reg) {
    case VIRTIO_MMIO_REG_QUEUE_DESC_LOW:
        return queue.desc_addr();
    case VIRTIO_MMIO_REG_QUEUE_DRIVER_LOW:
        return queue.avail_addr();
    case VIRTIO_MMIO_REG_QUEUE_DEVICE_LOW:
        return queue.used_addr();
    default:
        return 0;
    }
}

void VirtioMmio::write_queue_address(VirtQueue& queue, uint32_t reg, uint32_t value) {
    uint64_t current = 0;
    bool high = false;
    switch (reg) {
    case VIRTIO_MMIO_REG_QUEUE_DESC_LOW:
    case VIRTIO_MMIO_REG_QUEUE_DESC_HIGH:
        current = queue.desc_addr();
        high = (reg == VIRTIO_MMIO_REG_QUEUE_DESC_HIGH);
        break;
    case VIRTIO_MMIO_REG_QUEUE_DRIVER_LOW:
    case VIRTIO_MMIO_REG_QUEUE_DRIVER_HIGH:
        current = queue.avail_addr();
        high = (reg == VIRTIO_MMIO_REG_QUEUE_DRIVER_HIGH);
        break;
    case VIRTIO_MMIO_REG_QUEUE_DEVICE_LOW:
    case VIRTIO_MMIO_REG_QUEUE_DEVICE_HIGH:
        current = queue.used_addr();
        high = (reg == VIRTIO_MMIO_REG_QUEUE_DEVICE_HIGH);
        break;
    default:
        return;
    }

    if (high) {
        current = (current & UINT64_C(0xffffffff)) | (static_cast<uint64_t>(value) << 32);
    } else {
        current = (current & (UINT64_C(0xffffffff) << 32)) | static_cast<uint64_t>(value);
    }

    switch (reg) {
    case VIRTIO_MMIO_REG_QUEUE_DESC_LOW:
    case VIRTIO_MMIO_REG_QUEUE_DESC_HIGH:
        queue.set_desc_addr(current);
        return;
    case VIRTIO_MMIO_REG_QUEUE_DRIVER_LOW:
    case VIRTIO_MMIO_REG_QUEUE_DRIVER_HIGH:
        queue.set_avail_addr(current);
        return;
    case VIRTIO_MMIO_REG_QUEUE_DEVICE_LOW:
    case VIRTIO_MMIO_REG_QUEUE_DEVICE_HIGH:
        queue.set_used_addr(current);
        return;
    default:
        return;
    }
}

void VirtioMmio::reset_transport() {
    device_features_sel_ = 0;
    driver_features_sel_ = 0;
    driver_features_ = 0;
    queue_sel_ = 0;
    interrupt_status_ = 0;
    for (VirtQueue& queue : queues_) {
        queue.reset();
    }
    device_.reset();
    update_interrupt_line();
}

void VirtioMmio::update_interrupt_line() {
    plic_.set_source_level(irq_source_, interrupt_status_ != 0);
}
