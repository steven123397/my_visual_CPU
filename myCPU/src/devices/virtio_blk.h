#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "virtio_device.h"
#include "virtqueue.h"

constexpr uint32_t VIRTIO_BLK_T_IN = 0;
constexpr uint32_t VIRTIO_BLK_T_OUT = 1;
constexpr uint8_t VIRTIO_BLK_S_OK = 0;
constexpr uint8_t VIRTIO_BLK_S_IOERR = 1;
constexpr uint8_t VIRTIO_BLK_S_UNSUPP = 2;
constexpr uint32_t VIRTIO_BLK_SECTOR_SIZE = 512;

class VirtioBlk : public VirtioDevice {
public:
    void load_image(const char* path);
    void clear_image();
    bool attached() const;
    uint64_t capacity_sectors() const;

    uint32_t device_id() const override;
    uint32_t num_queues() const override;
    uint16_t queue_size(uint32_t queue_index) const override;
    uint32_t device_features(uint32_t word) const override;
    void reset() override;
    bool read_config(uint32_t offset, int size, uint64_t& value) const override;
    bool notify_queue(Bus& bus,
                      uint32_t queue_index,
                      VirtQueue& queue,
                      uint32_t& completed,
                      std::string& error) override;

private:
    struct RequestHeader {
        uint32_t type;
        uint32_t reserved;
        uint64_t sector;
    };

    bool process_chain(Bus& bus,
                       const VirtQueue::Chain& chain,
                       uint32_t& used_len,
                       std::string& error);
    bool write_status(Bus& bus, uint64_t addr, uint8_t status, std::string& error) const;

    std::vector<uint8_t> data_{};
    bool attached_{false};
};
