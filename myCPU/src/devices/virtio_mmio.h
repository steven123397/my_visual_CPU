#pragma once

#include <cstdint>
#include <vector>

#include "device.h"
#include "virtio_device.h"
#include "virtqueue.h"
#include "../platform/address_map.h"

class Bus;
class Plic;

constexpr uint32_t VIRTIO_MMIO_MAGIC_VALUE = 0x74726976;
constexpr uint32_t VIRTIO_MMIO_VERSION_VALUE = 2;
constexpr uint32_t VIRTIO_MMIO_INTERRUPT_USED_BUFFER = 0x1;
constexpr uint32_t VIRTIO_STATUS_ACKNOWLEDGE = 0x1;
constexpr uint32_t VIRTIO_STATUS_DRIVER = 0x2;
constexpr uint32_t VIRTIO_STATUS_DRIVER_OK = 0x4;
constexpr uint32_t VIRTIO_STATUS_FEATURES_OK = 0x8;
constexpr uint32_t VIRTIO_STATUS_FAILED = 0x80;

constexpr uint32_t VIRTIO_MMIO_REG_MAGIC_VALUE = 0x000;
constexpr uint32_t VIRTIO_MMIO_REG_VERSION = 0x004;
constexpr uint32_t VIRTIO_MMIO_REG_DEVICE_ID = 0x008;
constexpr uint32_t VIRTIO_MMIO_REG_VENDOR_ID = 0x00c;
constexpr uint32_t VIRTIO_MMIO_REG_DEVICE_FEATURES = 0x010;
constexpr uint32_t VIRTIO_MMIO_REG_DEVICE_FEATURES_SEL = 0x014;
constexpr uint32_t VIRTIO_MMIO_REG_DRIVER_FEATURES = 0x020;
constexpr uint32_t VIRTIO_MMIO_REG_DRIVER_FEATURES_SEL = 0x024;
constexpr uint32_t VIRTIO_MMIO_REG_QUEUE_SEL = 0x030;
constexpr uint32_t VIRTIO_MMIO_REG_QUEUE_NUM_MAX = 0x034;
constexpr uint32_t VIRTIO_MMIO_REG_QUEUE_NUM = 0x038;
constexpr uint32_t VIRTIO_MMIO_REG_QUEUE_READY = 0x044;
constexpr uint32_t VIRTIO_MMIO_REG_QUEUE_NOTIFY = 0x050;
constexpr uint32_t VIRTIO_MMIO_REG_INTERRUPT_STATUS = 0x060;
constexpr uint32_t VIRTIO_MMIO_REG_INTERRUPT_ACK = 0x064;
constexpr uint32_t VIRTIO_MMIO_REG_STATUS = 0x070;
constexpr uint32_t VIRTIO_MMIO_REG_QUEUE_DESC_LOW = 0x080;
constexpr uint32_t VIRTIO_MMIO_REG_QUEUE_DESC_HIGH = 0x084;
constexpr uint32_t VIRTIO_MMIO_REG_QUEUE_DRIVER_LOW = 0x090;
constexpr uint32_t VIRTIO_MMIO_REG_QUEUE_DRIVER_HIGH = 0x094;
constexpr uint32_t VIRTIO_MMIO_REG_QUEUE_DEVICE_LOW = 0x0a0;
constexpr uint32_t VIRTIO_MMIO_REG_QUEUE_DEVICE_HIGH = 0x0a4;
constexpr uint32_t VIRTIO_MMIO_REG_CONFIG_GENERATION = 0x0fc;
constexpr uint32_t VIRTIO_MMIO_CONFIG_SPACE_OFFSET = 0x100;

class VirtioMmio : public Device {
public:
    VirtioMmio(Plic& plic,
               uint32_t irq_source,
               VirtioDevice& device,
               uint64_t base = VIRTIO_MMIO_BASE,
               uint64_t size = VIRTIO_MMIO_SIZE);

    void bind_bus(Bus& bus);

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;
    const char* debug_name() const override {
        return "virtio_mmio";
    }

private:
    VirtQueue* selected_queue();
    const VirtQueue* selected_queue() const;
    uint32_t read_device_features() const;
    uint32_t read_driver_features() const;
    uint64_t read_queue_address(const VirtQueue& queue, uint32_t low_reg) const;
    void write_queue_address(VirtQueue& queue, uint32_t low_reg, uint32_t value);
    void reset_transport();
    void update_interrupt_line();

    Plic& plic_;
    uint32_t irq_source_{0};
    VirtioDevice& device_;
    Bus* bus_{nullptr};
    std::vector<VirtQueue> queues_{};
    uint32_t device_features_sel_{0};
    uint32_t driver_features_sel_{0};
    uint64_t driver_features_{0};
    uint32_t queue_sel_{0};
    uint32_t interrupt_status_{0};
    uint32_t status_{0};
};
