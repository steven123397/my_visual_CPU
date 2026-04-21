#include "virtio_device.h"

uint32_t VirtioDevice::vendor_id() const {
    return VIRTIO_MMIO_VENDOR_ID;
}

bool VirtioDevice::write_config(uint32_t, uint64_t, int) {
    return false;
}
