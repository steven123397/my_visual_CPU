#include "bus.h"

#include <cstring>
#include <exception>
#include <stdexcept>

#include "ram.h"

namespace {

bool ranges_overlap(const Device& lhs, const Device& rhs) {
    return lhs.base() < rhs.end() && rhs.base() < lhs.end();
}

void record_unmapped_access(DebugBusAccess& access,
                            bool write,
                            uint64_t addr,
                            uint64_t value,
                            int size,
                            const char* source,
                            const char* kind) {
    access.valid = true;
    access.success = false;
    access.write = write;
    access.mmio = false;
    access.source = source != nullptr ? source : "";
    access.kind = kind != nullptr ? kind : "";
    access.addr = addr;
    access.value = value;
    access.size = size;
    access.device = "<unmapped>";
    access.detail = "no device mapped for access";
}

DmaTransferResult make_dma_result(const DmaTransaction& transaction) {
    DmaTransferResult result;
    result.initiator = transaction.initiator != nullptr ? transaction.initiator : "<unknown>";
    result.addr = transaction.addr;
    result.requested_bytes = transaction.size;
    result.direction = transaction.direction;
    return result;
}

void fail_dma_result(
    DmaTransferResult& result,
    DmaFault fault,
    const PhysicalRegionInfo& region,
    const char* detail) {
    result.ok = false;
    result.fault = fault;
    result.region = region;
    result.detail = detail != nullptr ? detail : "";
}

bool validate_dma_request(const DmaTransaction& transaction, void* data, DmaTransferResult& result) {
    if (transaction.initiator == nullptr || transaction.initiator[0] == '\0') {
        fail_dma_result(result, DmaFault::InvalidArguments, {}, "missing DMA initiator");
        return false;
    }
    if (transaction.size == 0) {
        result.ok = true;
        result.fault = DmaFault::None;
        return false;
    }
    if (data == nullptr) {
        fail_dma_result(result, DmaFault::InvalidArguments, {}, "null DMA buffer");
        return false;
    }
    return true;
}

const char* observed_kind_for_device(const Device& device, const char* source, const char* kind) {
    if (source != nullptr && std::strcmp(source, "guest-data") == 0 && device.is_mmio()) {
        return "mmio-commit";
    }
    return kind;
}

}  // namespace

Bus::Bus(Ram& ram) {
    attach(ram);
}

void Bus::attach(Device& device) {
    for (Device* existing : devices_) {
        if (ranges_overlap(*existing, device)) {
            throw std::runtime_error("device address range overlap");
        }
    }
    devices_.push_back(&device);
}

Device* Bus::find_device(uint64_t addr, int size) {
    if (size <= 0) {
        return nullptr;
    }
    for (Device* device : devices_) {
        if (device->contains(addr, static_cast<uint64_t>(size))) {
            return device;
        }
    }
    return nullptr;
}

const Device* Bus::find_device(uint64_t addr, int size) const {
    if (size <= 0) {
        return nullptr;
    }
    for (const Device* device : devices_) {
        if (device->contains(addr, static_cast<uint64_t>(size))) {
            return device;
        }
    }
    return nullptr;
}

PhysicalRegionInfo Bus::describe_region(uint64_t addr, int size) const {
    const Device* device = find_device(addr, size);
    return device != nullptr ? device->region_info() : make_unmapped_region_info();
}

PhysicalSpanInfo Bus::describe_span(uint64_t addr, uint64_t bytes) const {
    PhysicalSpanInfo span;
    span.first_addr = addr;
    span.size = bytes;
    if (bytes == 0) {
        return span;
    }

    span.region = describe_region(addr, 1);
    if (span.region.kind == PhysicalRegionKind::Unmapped) {
        return span;
    }

    for (const Device* device : devices_) {
        if (device->contains(addr, bytes)) {
            span.ok = true;
            span.region = device->region_info();
            return span;
        }
    }
    return span;
}

bool Bus::try_load(uint64_t addr, int size, uint64_t& value) {
    return try_load_observed(addr, size, value, "internal", "bus-load");
}

bool Bus::try_store(uint64_t addr, uint64_t value, int size) {
    return try_store_observed(addr, value, size, "internal", "bus-store");
}

bool Bus::try_load_observed(uint64_t addr, int size, uint64_t& value, const char* source, const char* kind) {
    if (Device* device = find_device(addr, size)) {
        try {
            value = device->load(addr, size);
            record_access(*device, true, false, addr, value, size, "", source, kind);
            return true;
        } catch (const std::exception& ex) {
            record_access(*device, false, false, addr, 0, size, ex.what(), source, kind);
            value = 0;
            return false;
        }
    }
    record_unmapped(false, addr, 0, size, source, kind);
    value = 0;
    return false;
}

bool Bus::try_store_observed(uint64_t addr, uint64_t value, int size, const char* source, const char* kind) {
    if (Device* device = find_device(addr, size)) {
        try {
            device->store(addr, value, size);
            record_access(*device, true, true, addr, value, size, "", source, kind);
            return true;
        } catch (const std::exception& ex) {
            record_access(*device, false, true, addr, value, size, ex.what(), source, kind);
            return false;
        }
    }
    record_unmapped(true, addr, value, size, source, kind);
    return false;
}

DmaTransferResult Bus::dma_read(const DmaTransaction& transaction, void* data) {
    DmaTransferResult result = make_dma_result(transaction);
    result.direction = DmaDirection::Read;
    if (!validate_dma_request(transaction, data, result)) {
        return result;
    }

    const PhysicalSpanInfo span = describe_span(transaction.addr, transaction.size);
    if (!span.ok) {
        const DmaFault fault = span.region.kind == PhysicalRegionKind::Unmapped
                                   ? DmaFault::Unmapped
                                   : DmaFault::SpanCrossesRegionBoundary;
        fail_dma_result(result, fault, span.region, "DMA span is not fully mapped");
        return result;
    }
    if (span.region.has_side_effect) {
        fail_dma_result(result, DmaFault::SideEffectRegion, span.region, "DMA rejects side-effect region");
        return result;
    }
    if (!span.region.dma_visible) {
        fail_dma_result(result, DmaFault::RegionNotDmaVisible, span.region, "region is not DMA visible");
        return result;
    }
    if (transaction.burst && !span.region.supports_burst) {
        fail_dma_result(result, DmaFault::BurstNotSupported, span.region, "region does not support DMA burst");
        return result;
    }

    Device* device = find_device(transaction.addr, 1);
    if (device == nullptr) {
        fail_dma_result(result, DmaFault::Unmapped, span.region, "DMA start address is unmapped");
        return result;
    }

    auto* bytes = static_cast<unsigned char*>(data);
    for (size_t i = 0; i < transaction.size; ++i) {
        try {
            bytes[i] = static_cast<unsigned char>(device->load(transaction.addr + i, 1) & 0xFFU);
            ++result.transferred_bytes;
        } catch (const std::exception& ex) {
            fail_dma_result(result, DmaFault::DeviceFault, span.region, ex.what());
            return result;
        }
    }

    result.ok = true;
    result.fault = DmaFault::None;
    result.region = span.region;
    result.detail.clear();
    return result;
}

DmaTransferResult Bus::dma_write(const DmaTransaction& transaction, const void* data) {
    DmaTransferResult result = make_dma_result(transaction);
    result.direction = DmaDirection::Write;
    if (!validate_dma_request(transaction, const_cast<void*>(data), result)) {
        return result;
    }

    const PhysicalSpanInfo span = describe_span(transaction.addr, transaction.size);
    if (!span.ok) {
        const DmaFault fault = span.region.kind == PhysicalRegionKind::Unmapped
                                   ? DmaFault::Unmapped
                                   : DmaFault::SpanCrossesRegionBoundary;
        fail_dma_result(result, fault, span.region, "DMA span is not fully mapped");
        return result;
    }
    if (span.region.has_side_effect) {
        fail_dma_result(result, DmaFault::SideEffectRegion, span.region, "DMA rejects side-effect region");
        return result;
    }
    if (!span.region.dma_visible) {
        fail_dma_result(result, DmaFault::RegionNotDmaVisible, span.region, "region is not DMA visible");
        return result;
    }
    if (transaction.burst && !span.region.supports_burst) {
        fail_dma_result(result, DmaFault::BurstNotSupported, span.region, "region does not support DMA burst");
        return result;
    }

    Device* device = find_device(transaction.addr, 1);
    if (device == nullptr) {
        fail_dma_result(result, DmaFault::Unmapped, span.region, "DMA start address is unmapped");
        return result;
    }

    const auto* bytes = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < transaction.size; ++i) {
        try {
            device->store(transaction.addr + i, bytes[i], 1);
            ++result.transferred_bytes;
        } catch (const std::exception& ex) {
            fail_dma_result(result, DmaFault::DeviceFault, span.region, ex.what());
            return result;
        }
    }

    result.ok = true;
    result.fault = DmaFault::None;
    result.region = span.region;
    result.detail.clear();
    return result;
}

bool Bus::dma_load_bytes(uint64_t addr, void* data, size_t size, const char* initiator) {
    return dma_read(
               DmaTransaction{
                   .initiator = initiator,
                   .addr = addr,
                   .size = size,
                   .burst = size > 1,
                   .direction = DmaDirection::Read,
               },
               data)
        .ok;
}

bool Bus::dma_store_bytes(uint64_t addr, const void* data, size_t size, const char* initiator) {
    return dma_write(
               DmaTransaction{
                   .initiator = initiator,
                   .addr = addr,
                   .size = size,
                   .burst = size > 1,
                   .direction = DmaDirection::Write,
               },
               data)
        .ok;
}


PlatformEvents Bus::tick() {
    PlatformEvents events;
    for (Device* device : devices_) {
        events.merge(device->tick());
    }
    return events;
}

PlatformEvents Bus::peek_events() const {
    PlatformEvents events;
    for (const Device* device : devices_) {
        events.merge(device->peek_events());
    }
    return events;
}

const DebugBusAccess& Bus::last_access() const {
    return last_access_;
}

const DebugBusAccess& Bus::last_guest_access() const {
    return last_guest_access_;
}

void Bus::record_access(
    const Device& device,
    bool success,
    bool write,
    uint64_t addr,
    uint64_t value,
    int size,
    const char* detail,
    const char* source,
    const char* kind) {
    last_access_.valid = true;
    last_access_.success = success;
    last_access_.write = write;
    last_access_.mmio = device.is_mmio();
    last_access_.source = source != nullptr ? source : "";
    last_access_.kind = observed_kind_for_device(device, source, kind);
    last_access_.addr = addr;
    last_access_.value = value;
    last_access_.size = size;
    last_access_.device = device.debug_name();
    last_access_.detail = detail != nullptr ? detail : "";
    if (last_access_.source == "guest-data" || last_access_.kind == "mmio-commit") {
        last_guest_access_ = last_access_;
    }
}

void Bus::record_unmapped(
    bool write,
    uint64_t addr,
    uint64_t value,
    int size,
    const char* source,
    const char* kind) {
    record_unmapped_access(last_access_, write, addr, value, size, source, kind);
    if (last_access_.source == "guest-data" || last_access_.kind == "mmio-commit") {
        last_guest_access_ = last_access_;
    }
}
