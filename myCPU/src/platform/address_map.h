#pragma once

#include "../../include/platform_mmio.h"

#ifdef __cplusplus
#include <cstdint>
inline constexpr uint64_t VIRTIO_MMIO_BASE = STORAGE_BASE;
inline constexpr uint64_t VIRTIO_MMIO_SIZE = 0x1000;
inline constexpr uint32_t VIRTIO_MMIO_PLIC_SOURCE = PLIC_SOURCE_VIRTIO_MMIO;
inline constexpr uint64_t AI_ACCEL_MMIO_BASE = AI_ACCEL_BASE;
inline constexpr uint64_t AI_ACCEL_MMIO_SIZE = AI_ACCEL_SIZE;
inline constexpr uint32_t AI_ACCEL_PLIC_SOURCE = PLIC_SOURCE_AI_ACCEL;
#endif
