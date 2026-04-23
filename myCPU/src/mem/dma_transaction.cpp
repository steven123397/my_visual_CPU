#include "dma_transaction.h"

bool DmaTransferResult::complete() const {
    return ok && transferred_bytes == requested_bytes;
}

bool DmaTransferResult::partial() const {
    return transferred_bytes != 0 && transferred_bytes < requested_bytes;
}

const char* dma_fault_name(DmaFault fault) {
    switch (fault) {
    case DmaFault::None:
        return "none";
    case DmaFault::InvalidArguments:
        return "invalid_arguments";
    case DmaFault::Unmapped:
        return "unmapped";
    case DmaFault::SpanCrossesRegionBoundary:
        return "span_crosses_region_boundary";
    case DmaFault::RegionNotDmaVisible:
        return "region_not_dma_visible";
    case DmaFault::SideEffectRegion:
        return "side_effect_region";
    case DmaFault::BurstNotSupported:
        return "burst_not_supported";
    case DmaFault::DeviceFault:
        return "device_fault";
    }
    return "unknown";
}
