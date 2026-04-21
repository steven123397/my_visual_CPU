#pragma once

#include <cstdint>
#include <string>

class Bus;
class VirtQueue;

constexpr uint32_t VIRTIO_MMIO_VENDOR_ID = 0x554d4551;
constexpr uint32_t VIRTIO_DEVICE_ID_BLOCK = 2;

class VirtioDevice {
public:
    virtual ~VirtioDevice() = default;

    virtual uint32_t device_id() const = 0;
    virtual uint32_t vendor_id() const;
    virtual uint32_t num_queues() const = 0;
    virtual uint16_t queue_size(uint32_t queue_index) const = 0;
    virtual uint32_t device_features(uint32_t word) const = 0;
    virtual void reset() = 0;
    virtual bool read_config(uint32_t offset, int size, uint64_t& value) const = 0;
    virtual bool write_config(uint32_t offset, uint64_t value, int size);
    virtual bool notify_queue(Bus& bus,
                              uint32_t queue_index,
                              VirtQueue& queue,
                              uint32_t& completed,
                              std::string& error) = 0;
};
