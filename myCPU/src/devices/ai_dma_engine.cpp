#include "ai_dma_engine.h"

#include <vector>

#include "../mem/bus.h"

namespace {

bool is_ram_dma_target(const PhysicalSpanInfo& span) {
    return span.ok && span.region.kind == PhysicalRegionKind::Ram && span.region.dma_visible &&
           !span.region.has_side_effect;
}

DmaTransaction make_dma_transaction(const AiDmaRequest& request, DmaDirection direction) {
    return DmaTransaction{
        .initiator = request.initiator,
        .addr = request.system_addr,
        .size = request.size,
        .burst = request.size > 1,
        .direction = direction,
    };
}

}  // namespace

AiDmaEngine::AiDmaEngine(AiScratchpad& scratchpad, AiDmaTimingConfig timing)
    : scratchpad_(scratchpad),
      timing_{
          .setup_cycles = timing.setup_cycles,
          .bytes_per_cycle = timing.bytes_per_cycle == 0 ? 1U : timing.bytes_per_cycle,
      } {}

void AiDmaEngine::reset() {
    counters_ = {};
    active_ = {};
    active_valid_ = false;
}

bool AiDmaEngine::busy() const {
    return active_valid_;
}

const AiDmaTimingConfig& AiDmaEngine::timing() const {
    return timing_;
}

const AiDmaCounters& AiDmaEngine::counters() const {
    return counters_;
}

uint64_t AiDmaEngine::remaining_cycles() const {
    return active_valid_ ? active_.remaining_cycles : 0;
}

bool AiDmaEngine::start(const AiDmaRequest& request, uint32_t& fault_code, std::string& error) {
    fault_code = AI_ACCEL_FAULT_NONE;
    error.clear();
    if (active_valid_) {
        fault_code = AI_ACCEL_FAULT_EXECUTION;
        error = "AI DMA engine is already busy";
        return false;
    }
    if (request.initiator == nullptr || request.initiator[0] == '\0') {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        error = "AI DMA initiator is missing";
        return false;
    }
    if (request.size == 0) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        error = "AI DMA request size must be non-zero";
        return false;
    }
    if (!scratchpad_.contains(request.space, request.scratchpad_offset, request.size)) {
        fault_code = AI_ACCEL_FAULT_SCRATCHPAD_OVERFLOW;
        error = "AI DMA request exceeds scratchpad space";
        return false;
    }

    active_.request = request;
    active_.remaining_cycles = transfer_cycles(request.size);
    active_.buffer.assign(request.size, 0);
    if (request.kind == AiDmaTransferKind::Store &&
        !scratchpad_.read(request.space, request.scratchpad_offset, active_.buffer.data(), active_.buffer.size())) {
        active_ = {};
        fault_code = AI_ACCEL_FAULT_SCRATCHPAD_OVERFLOW;
        error = "AI DMA store could not read scratchpad bytes";
        return false;
    }

    active_valid_ = true;
    return true;
}

AiDmaTickResult AiDmaEngine::tick(Bus& bus) {
    AiDmaTickResult result{};
    if (!active_valid_) {
        return result;
    }

    result.kind = active_.request.kind;
    if (active_.request.kind == AiDmaTransferKind::Load) {
        ++counters_.load_cycles;
    } else {
        ++counters_.store_cycles;
    }
    ++counters_.total_cycles;

    if (active_.remaining_cycles > 0) {
        --active_.remaining_cycles;
    }
    if (active_.remaining_cycles != 0) {
        return result;
    }

    result.completed = true;
    const PhysicalSpanInfo span = bus.describe_span(active_.request.system_addr, active_.request.size);
    if (!is_ram_dma_target(span)) {
        result.faulted = true;
        result.fault_code = AI_ACCEL_FAULT_DMA;
        result.dma_result = DmaTransferResult{
            .ok = false,
            .fault = span.region.kind == PhysicalRegionKind::Unmapped
                         ? DmaFault::Unmapped
                         : DmaFault::RegionNotDmaVisible,
            .direction = active_.request.kind == AiDmaTransferKind::Load ? DmaDirection::Read
                                                                         : DmaDirection::Write,
            .initiator = active_.request.initiator,
            .addr = active_.request.system_addr,
            .requested_bytes = active_.request.size,
            .transferred_bytes = 0,
            .region = span.region,
            .detail = "AI DMA requires a DMA-visible RAM span",
        };
        active_ = {};
        active_valid_ = false;
        return result;
    }

    if (active_.request.kind == AiDmaTransferKind::Load) {
        result.dma_result = bus.dma_read(make_dma_transaction(active_.request, DmaDirection::Read),
                                         active_.buffer.data());
        if (!result.dma_result.ok ||
            !scratchpad_.write(active_.request.space,
                               active_.request.scratchpad_offset,
                               active_.buffer.data(),
                               active_.buffer.size())) {
            result.faulted = true;
            result.fault_code = AI_ACCEL_FAULT_DMA;
            if (result.dma_result.ok) {
                result.dma_result.ok = false;
                result.dma_result.fault = DmaFault::DeviceFault;
                result.dma_result.detail = "AI DMA could not commit the scratchpad load";
            }
        } else {
            result.bytes_moved = active_.buffer.size();
            counters_.load_bytes += result.bytes_moved;
            ++counters_.load_transfers;
        }
    } else {
        std::vector<uint8_t> backup(active_.buffer.size(), 0);
        DmaTransferResult backup_result = bus.dma_read(
            make_dma_transaction(active_.request, DmaDirection::Read),
            backup.data());
        if (!backup_result.ok) {
            result.faulted = true;
            result.fault_code = AI_ACCEL_FAULT_DMA;
            result.dma_result = backup_result;
        } else {
            result.dma_result = bus.dma_write(
                make_dma_transaction(active_.request, DmaDirection::Write),
                active_.buffer.data());
            if (!result.dma_result.ok) {
                bus.dma_write(make_dma_transaction(active_.request, DmaDirection::Write), backup.data());
                result.faulted = true;
                result.fault_code = AI_ACCEL_FAULT_DMA;
            } else {
                result.bytes_moved = active_.buffer.size();
                counters_.store_bytes += result.bytes_moved;
                ++counters_.store_transfers;
            }
        }
    }

    active_ = {};
    active_valid_ = false;
    return result;
}

uint64_t AiDmaEngine::transfer_cycles(uint32_t bytes) const {
    const uint64_t payload_cycles =
        (static_cast<uint64_t>(bytes) + timing_.bytes_per_cycle - 1) / timing_.bytes_per_cycle;
    return static_cast<uint64_t>(timing_.setup_cycles) + payload_cycles;
}
