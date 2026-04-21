#pragma once

#include "../../include/platform_mmio.h"

#ifdef __cplusplus
#include <cstdint>
inline constexpr uint64_t VIRTIO_MMIO_BASE = STORAGE_BASE;
inline constexpr uint64_t VIRTIO_MMIO_SIZE = 0x1000;
inline constexpr uint32_t VIRTIO_MMIO_PLIC_SOURCE = PLIC_SOURCE_UART_THRE;
#endif
