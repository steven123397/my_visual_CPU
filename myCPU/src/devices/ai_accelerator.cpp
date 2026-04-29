#include "ai_accelerator.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#include "plic.h"
#include "../mem/bus.h"

namespace {

bool is_valid_mmio_width(int size) {
    return size == 4;
}

uint64_t combine_u32(uint64_t current, uint32_t value, bool high) {
    if (high) {
        return (current & UINT64_C(0xffffffff)) | (static_cast<uint64_t>(value) << 32);
    }
    return (current & (UINT64_C(0xffffffff) << 32)) | value;
}

bool add_u64_u32(uint64_t base, uint32_t offset, uint64_t& result) {
    result = base + static_cast<uint64_t>(offset);
    return result >= base;
}

uint64_t read_le_u64(const uint8_t* bytes) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(bytes[i]) << (8 * i);
    }
    return value;
}

uint32_t saturating_u32(size_t value) {
    const size_t max = static_cast<size_t>(std::numeric_limits<uint32_t>::max());
    return static_cast<uint32_t>(std::min(value, max));
}

constexpr uint32_t kRuntimeShapeTableAlignment = 4;

}  // namespace

AiAccelerator::AiAccelerator(Plic& plic,
                             uint32_t irq_source,
                             uint64_t base,
                             uint64_t size)
    : Device(base, size),
      plic_(plic),
      irq_source_(irq_source),
      dma_engine_(scratchpad_) {}

void AiAccelerator::bind_bus(Bus& bus) {
    bus_ = &bus;
}

uint64_t AiAccelerator::load(uint64_t addr, int size) {
    if (!is_valid_mmio_width(size)) {
        invalid_access(addr, size);
    }

    const AiDmaCounters& dma = dma_engine_.counters();
    const uint32_t offset = static_cast<uint32_t>(addr - base());
    switch (offset) {
    case AI_ACCEL_REG_MAGIC:
        return AI_ACCEL_MMIO_MAGIC;
    case AI_ACCEL_REG_VERSION:
        return AI_ACCEL_MMIO_VERSION;
    case AI_ACCEL_REG_CAPABILITY:
        return AI_ACCEL_CAPABILITIES;
    case AI_ACCEL_REG_STATUS:
        return status();
    case AI_ACCEL_REG_QUEUE_DEPTH:
        return queue_.pending_depth();
    case AI_ACCEL_REG_SUBMIT_QUEUE_BASE_LOW:
        return static_cast<uint32_t>(queue_.submission_base());
    case AI_ACCEL_REG_SUBMIT_QUEUE_BASE_HIGH:
        return static_cast<uint32_t>(queue_.submission_base() >> 32);
    case AI_ACCEL_REG_SUBMIT_QUEUE_SIZE:
        return queue_.submission_size();
    case AI_ACCEL_REG_SUBMIT_QUEUE_HEAD:
        return queue_.submission_head();
    case AI_ACCEL_REG_SUBMIT_QUEUE_TAIL:
        return queue_.submission_tail();
    case AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW:
        return static_cast<uint32_t>(queue_.completion_base());
    case AI_ACCEL_REG_COMPLETE_QUEUE_BASE_HIGH:
        return static_cast<uint32_t>(queue_.completion_base() >> 32);
    case AI_ACCEL_REG_COMPLETE_QUEUE_SIZE:
        return queue_.completion_size();
    case AI_ACCEL_REG_COMPLETE_QUEUE_HEAD:
        return queue_.completion_head();
    case AI_ACCEL_REG_COMPLETE_QUEUE_TAIL:
        return queue_.completion_tail();
    case AI_ACCEL_REG_DOORBELL:
        return 0;
    case AI_ACCEL_REG_IRQ_STATUS:
        return irq_status_;
    case AI_ACCEL_REG_IRQ_MASK:
        return irq_mask_;
    case AI_ACCEL_REG_LAST_FAULT:
        return last_fault_;
    case AI_ACCEL_REG_FAULT_DETAIL:
        return fault_detail_;
    case AI_ACCEL_REG_DOORBELL_COUNT_LOW:
        return counter_low(doorbell_count_);
    case AI_ACCEL_REG_DOORBELL_COUNT_HIGH:
        return counter_high(doorbell_count_);
    case AI_ACCEL_REG_COMPLETION_COUNT_LOW:
        return counter_low(completion_count_);
    case AI_ACCEL_REG_COMPLETION_COUNT_HIGH:
        return counter_high(completion_count_);
    case AI_ACCEL_REG_SUBMISSION_COUNT_LOW:
        return counter_low(submission_count_);
    case AI_ACCEL_REG_SUBMISSION_COUNT_HIGH:
        return counter_high(submission_count_);
    case AI_ACCEL_REG_FAULT_COUNT_LOW:
        return counter_low(fault_count_);
    case AI_ACCEL_REG_FAULT_COUNT_HIGH:
        return counter_high(fault_count_);
    case AI_ACCEL_REG_DEVICE_CYCLES_LOW:
        return counter_low(device_cycles_);
    case AI_ACCEL_REG_DEVICE_CYCLES_HIGH:
        return counter_high(device_cycles_);
    case AI_ACCEL_REG_DMA_CYCLES_LOW:
        return counter_low(dma.total_cycles);
    case AI_ACCEL_REG_DMA_CYCLES_HIGH:
        return counter_high(dma.total_cycles);
    case AI_ACCEL_REG_COMPUTE_CYCLES_LOW:
        return counter_low(compute_cycles_);
    case AI_ACCEL_REG_COMPUTE_CYCLES_HIGH:
        return counter_high(compute_cycles_);
    case AI_ACCEL_REG_STALL_CYCLES_LOW:
        return counter_low(stall_cycles_);
    case AI_ACCEL_REG_STALL_CYCLES_HIGH:
        return counter_high(stall_cycles_);
    case AI_ACCEL_REG_BUSY_CYCLES_LOW:
        return counter_low(busy_cycles());
    case AI_ACCEL_REG_BUSY_CYCLES_HIGH:
        return counter_high(busy_cycles());
    case AI_ACCEL_REG_QUEUE_CYCLES_LOW:
        return counter_low(queue_cycles_);
    case AI_ACCEL_REG_QUEUE_CYCLES_HIGH:
        return counter_high(queue_cycles_);
    case AI_ACCEL_REG_COMPLETION_CYCLES_LOW:
        return counter_low(completion_cycles_);
    case AI_ACCEL_REG_COMPLETION_CYCLES_HIGH:
        return counter_high(completion_cycles_);
    case AI_ACCEL_REG_EFFECTIVE_OPS_PER_CYCLE:
        return effective_ops_per_cycle();
    case AI_ACCEL_REG_UTILIZATION:
        return utilization();
    case AI_ACCEL_REG_DMA_LOAD_CYCLES_LOW:
        return counter_low(dma.load_cycles);
    case AI_ACCEL_REG_DMA_LOAD_CYCLES_HIGH:
        return counter_high(dma.load_cycles);
    case AI_ACCEL_REG_DMA_STORE_CYCLES_LOW:
        return counter_low(dma.store_cycles);
    case AI_ACCEL_REG_DMA_STORE_CYCLES_HIGH:
        return counter_high(dma.store_cycles);
    case AI_ACCEL_REG_DMA_LOAD_BYTES_LOW:
        return counter_low(dma.load_bytes);
    case AI_ACCEL_REG_DMA_LOAD_BYTES_HIGH:
        return counter_high(dma.load_bytes);
    case AI_ACCEL_REG_DMA_STORE_BYTES_LOW:
        return counter_low(dma.store_bytes);
    case AI_ACCEL_REG_DMA_STORE_BYTES_HIGH:
        return counter_high(dma.store_bytes);
    case AI_ACCEL_REG_DMA_SETUP_CYCLES:
        return dma_engine_.timing().setup_cycles;
    case AI_ACCEL_REG_DMA_BYTES_PER_CYCLE:
        return dma_engine_.timing().bytes_per_cycle;
    default:
        invalid_access(addr, size);
    }
}

void AiAccelerator::store(uint64_t addr, uint64_t value, int size) {
    if (!is_valid_mmio_width(size)) {
        invalid_access(addr, size);
    }

    const uint32_t offset = static_cast<uint32_t>(addr - base());
    const uint32_t value32 = static_cast<uint32_t>(value);
    switch (offset) {
    case AI_ACCEL_REG_CONTROL:
        if ((value32 & AI_ACCEL_CONTROL_RESET) != 0) {
            reset_device();
        }
        return;
    case AI_ACCEL_REG_SUBMIT_QUEUE_BASE_LOW:
        write_queue_base_low(true, value32);
        return;
    case AI_ACCEL_REG_SUBMIT_QUEUE_BASE_HIGH:
        write_queue_base_high(true, value32);
        return;
    case AI_ACCEL_REG_SUBMIT_QUEUE_SIZE:
        queue_.set_submission_size(value32);
        pending_submission_budget_ = 0;
        clear_active_submission();
        dma_engine_.reset();
        return;
    case AI_ACCEL_REG_SUBMIT_QUEUE_TAIL:
        queue_.set_submission_tail(value32);
        return;
    case AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW:
        write_queue_base_low(false, value32);
        return;
    case AI_ACCEL_REG_COMPLETE_QUEUE_BASE_HIGH:
        write_queue_base_high(false, value32);
        return;
    case AI_ACCEL_REG_COMPLETE_QUEUE_SIZE:
        queue_.set_completion_size(value32);
        pending_submission_budget_ = 0;
        clear_active_submission();
        dma_engine_.reset();
        return;
    case AI_ACCEL_REG_COMPLETE_QUEUE_HEAD:
        queue_.set_completion_head(value32);
        return;
    case AI_ACCEL_REG_DOORBELL:
        ring_doorbell(value32);
        return;
    case AI_ACCEL_REG_IRQ_MASK:
        irq_mask_ = value32 & AI_ACCEL_IRQ_ALL;
        update_interrupt_line();
        return;
    case AI_ACCEL_REG_IRQ_ACK:
        irq_status_ &= ~(value32 & AI_ACCEL_IRQ_ALL);
        update_interrupt_line();
        return;
    default:
        invalid_access(addr, size);
    }
}

PlatformEvents AiAccelerator::tick() {
    pump_queue();
    update_interrupt_line();
    return {};
}

uint64_t AiAccelerator::doorbell_count() const {
    return doorbell_count_;
}

uint64_t AiAccelerator::completion_count() const {
    return completion_count_;
}

uint32_t AiAccelerator::last_fault() const {
    return last_fault_;
}

const AiAcceleratorProfileSummary& AiAccelerator::profile_summary() const {
    return profile_summary_;
}

DebugAiAcceleratorSnapshot AiAccelerator::debug_snapshot() const {
    return DebugAiAcceleratorSnapshot{
        .present = true,
        .queue_depth = queue_.pending_depth(),
        .doorbell_count = doorbell_count_,
        .last_fault = last_fault_,
        .completion_count = completion_count_,
        .engine_busy = is_busy(),
        .scratchpad_occupancy_bytes =
            active_submission_valid_ ? active_submission_.package.scratchpad_budget_bytes : 0,
        .dma_load_bytes = dma_engine_.counters().load_bytes,
        .dma_store_bytes = dma_engine_.counters().store_bytes,
        .device_cycles = device_cycles_,
        .dma_cycles = dma_engine_.counters().total_cycles,
        .compute_cycles = compute_cycles_,
        .stall_cycles = stall_cycles_,
        .busy_cycles = busy_cycles(),
        .queue_cycles = queue_cycles_,
        .completion_cycles = completion_cycles_,
        .effective_ops_per_cycle = effective_ops_per_cycle(),
        .utilization = utilization(),
    };
}

bool AiAccelerator::is_busy() const {
    return dma_engine_.busy() || active_submission_valid_ || pending_submission_budget_ != 0;
}

uint32_t AiAccelerator::status() const {
    uint32_t value = AI_ACCEL_STATUS_READY;
    if (is_busy()) {
        value |= AI_ACCEL_STATUS_BUSY;
    }
    if (last_fault_ != AI_ACCEL_FAULT_NONE) {
        value |= AI_ACCEL_STATUS_FAULT;
    }
    if ((irq_status_ & irq_mask_) != 0) {
        value |= AI_ACCEL_STATUS_IRQ;
    }
    return value;
}

uint64_t AiAccelerator::busy_cycles() const {
    return device_cycles_ + queue_cycles_ + completion_cycles_;
}

uint32_t AiAccelerator::effective_ops_per_cycle() const {
    if (compute_cycles_ == 0) {
        return 0;
    }
    return static_cast<uint32_t>(retired_ops_ / compute_cycles_);
}

uint32_t AiAccelerator::utilization() const {
    const uint64_t busy = busy_cycles();
    if (busy == 0) {
        return 0;
    }
    return static_cast<uint32_t>((compute_cycles_ * 100U) / busy);
}

uint32_t AiAccelerator::counter_low(uint64_t value) const {
    return static_cast<uint32_t>(value);
}

uint32_t AiAccelerator::counter_high(uint64_t value) const {
    return static_cast<uint32_t>(value >> 32);
}

void AiAccelerator::write_queue_base_low(bool submission, uint32_t value) {
    if (submission) {
        queue_.set_submission_base(combine_u32(queue_.submission_base(), value, false));
        return;
    }
    queue_.set_completion_base(combine_u32(queue_.completion_base(), value, false));
}

void AiAccelerator::write_queue_base_high(bool submission, uint32_t value) {
    if (submission) {
        queue_.set_submission_base(combine_u32(queue_.submission_base(), value, true));
        return;
    }
    queue_.set_completion_base(combine_u32(queue_.completion_base(), value, true));
}

void AiAccelerator::ring_doorbell(uint32_t budget) {
    ++doorbell_count_;
    if (bus_ == nullptr) {
        set_fault(AI_ACCEL_FAULT_EXECUTION, 0);
        irq_status_ |= AI_ACCEL_IRQ_FAULT;
        update_interrupt_line();
        return;
    }
    if (!queue_.configured()) {
        set_fault(AI_ACCEL_FAULT_QUEUE_NOT_CONFIGURED, 0);
        irq_status_ |= AI_ACCEL_IRQ_FAULT;
        update_interrupt_line();
        return;
    }

    const uint32_t requested = budget == 0 ? queue_.pending_depth() : budget;
    const uint32_t count = std::min<uint32_t>(requested, queue_.pending_depth());
    if (count == 0) {
        update_interrupt_line();
        return;
    }
    pending_submission_budget_ = std::min<uint32_t>(kAiQueueMaxEntries, pending_submission_budget_ + count);
    pump_queue();
    update_interrupt_line();
}

void AiAccelerator::pump_queue() {
    if (bus_ == nullptr) {
        return;
    }

    while (true) {
        if (dma_engine_.busy()) {
            const AiDmaTickResult step = dma_engine_.tick(*bus_);
            ++device_cycles_;
            if (!step.completed) {
                return;
            }
            if (step.faulted) {
                const uint32_t detail = saturating_u32(step.dma_result.transferred_bytes);
                const bool completion_ok = complete_descriptor(
                    active_submission_.descriptor,
                    step.fault_code,
                    detail,
                    active_submission_.bytes_moved,
                    active_submission_.retired_ops);
                clear_active_submission();
                if (!completion_ok) {
                    return;
                }
                continue;
            }
            active_submission_.bytes_moved += step.bytes_moved;
            if (step.kind == AiDmaTransferKind::Load) {
                ++active_submission_.next_load_index;
            } else {
                ++active_submission_.next_store_index;
            }
        }

        if (!active_submission_valid_) {
            if (pending_submission_budget_ == 0) {
                return;
            }
            if (queue_.pending_depth() == 0) {
                pending_submission_budget_ = 0;
                return;
            }
            if (!begin_submission()) {
                if (pending_submission_budget_ == 0) {
                    return;
                }
                continue;
            }
        }

        uint32_t fault = AI_ACCEL_FAULT_NONE;
        uint32_t detail = 0;
        if (active_submission_valid_ && active_submission_.compute_started &&
            !active_submission_.compute_complete) {
            if (active_submission_.stall_cycles_remaining != 0) {
                --active_submission_.stall_cycles_remaining;
                ++stall_cycles_;
                ++device_cycles_;
                if (active_submission_.stall_cycles_remaining != 0) {
                    return;
                }
            }
            if (active_submission_.compute_cycles_remaining != 0) {
                --active_submission_.compute_cycles_remaining;
                ++compute_cycles_;
                ++device_cycles_;
                if (active_submission_.compute_cycles_remaining != 0) {
                    return;
                }
            }
            if (active_submission_.stall_cycles_remaining == 0 &&
                active_submission_.compute_cycles_remaining == 0) {
                active_submission_.compute_complete = true;
            }
        }
        if (start_next_transfer(fault, detail)) {
            return;
        }
        const bool completion_ok = complete_descriptor(
            active_submission_.descriptor,
            fault,
            detail,
            active_submission_.bytes_moved,
            active_submission_.retired_ops);
        clear_active_submission();
        if (!completion_ok) {
            return;
        }
    }
}

bool AiAccelerator::begin_submission() {
    if (pending_submission_budget_ == 0 || bus_ == nullptr) {
        return false;
    }
    if (!queue_.completion_has_space()) {
        set_fault(AI_ACCEL_FAULT_COMPLETION_QUEUE_FULL, 0);
        irq_status_ |= AI_ACCEL_IRQ_FAULT;
        pending_submission_budget_ = 0;
        return false;
    }

    --pending_submission_budget_;
    ++queue_cycles_;

    AiSubmissionDescriptor descriptor{};
    std::string error;
    if (!queue_.read_next_submission(*bus_, descriptor, error)) {
        complete_descriptor(AiSubmissionDescriptor{}, AI_ACCEL_FAULT_DMA, 0, 0, 0);
        return false;
    }

    const uint32_t fault = validate_descriptor(descriptor);
    if (fault != AI_ACCEL_FAULT_NONE) {
        complete_descriptor(descriptor, fault, descriptor.graph_package_bytes, 0, 0);
        return false;
    }

    uint32_t prepare_fault = AI_ACCEL_FAULT_NONE;
    uint32_t detail = 0;
    if (!prepare_active_submission(descriptor, prepare_fault, detail)) {
        complete_descriptor(descriptor, prepare_fault, detail, 0, 0);
        return false;
    }
    return true;
}

bool AiAccelerator::prepare_active_submission(const AiSubmissionDescriptor& descriptor,
                                              uint32_t& fault,
                                              uint32_t& detail) {
    fault = AI_ACCEL_FAULT_NONE;
    detail = 0;

    std::vector<uint8_t> bytes;
    std::string error;
    if (!read_graph_package_bytes(descriptor.graph_package_addr,
                                  descriptor.graph_package_bytes,
                                  bytes,
                                  error)) {
        fault = AI_ACCEL_FAULT_DMA;
        return false;
    }

    AiGraphPackage package{};
    if (!parse_ai_graph_package(bytes, package, error)) {
        fault = graph_package_fault(error);
        detail = descriptor.graph_package_bytes;
        return false;
    }

    if (package.shape_mode == AiShapeMode::Static) {
        if (descriptor.runtime_shape_table_offset != 0) {
            fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
            detail = descriptor.runtime_shape_table_offset;
            return false;
        }
    } else if (package.shape_mode == AiShapeMode::DynamicBounded) {
        if (descriptor.runtime_shape_table_offset == 0) {
            fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
            detail = 0;
            return false;
        }
        if ((descriptor.runtime_shape_table_offset % kRuntimeShapeTableAlignment) != 0) {
            fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
            detail = descriptor.runtime_shape_table_offset;
            return false;
        }
        const uint32_t runtime_shape_table_bytes = static_cast<uint32_t>(
            package.dynamic_tensors.size() * kAiRuntimeShapeEntryBytes);
        if (descriptor.runtime_shape_table_offset < descriptor.graph_package_bytes) {
            fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
            detail = descriptor.runtime_shape_table_offset;
            return false;
        }
        if (runtime_shape_table_bytes > AI_ACCEL_MAX_GRAPH_PACKAGE_BYTES ||
            descriptor.runtime_shape_table_offset >
                AI_ACCEL_MAX_GRAPH_PACKAGE_BYTES - runtime_shape_table_bytes) {
            fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
            detail = descriptor.runtime_shape_table_offset;
            return false;
        }

        uint64_t runtime_shape_table_addr = 0;
        if (!add_u64_u32(descriptor.graph_package_addr,
                         descriptor.runtime_shape_table_offset,
                         runtime_shape_table_addr)) {
            fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
            detail = descriptor.runtime_shape_table_offset;
            return false;
        }

        std::vector<uint8_t> runtime_shape_bytes{};
        if (!read_graph_package_bytes(runtime_shape_table_addr,
                                      runtime_shape_table_bytes,
                                      runtime_shape_bytes,
                                      error)) {
            fault = AI_ACCEL_FAULT_DMA;
            detail = descriptor.runtime_shape_table_offset;
            return false;
        }

        std::vector<AiRuntimeShapeEntry> runtime_shapes{};
        if (!parse_ai_runtime_shape_table(runtime_shape_bytes,
                                          package.dynamic_tensors.size(),
                                          runtime_shapes,
                                          error)) {
            fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
            detail = descriptor.runtime_shape_table_offset;
            return false;
        }
        if (!resolve_ai_runtime_shape_package(package, runtime_shapes, package, error)) {
            fault = graph_package_fault(error);
            detail = descriptor.runtime_shape_table_offset;
            return false;
        }
    } else {
        fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        detail = descriptor.runtime_shape_table_offset;
        return false;
    }

    scratchpad_.configure(package.scratchpad_budget_bytes);

    AiActiveSubmissionState next{};
    next.descriptor = descriptor;
    next.package = std::move(package);
    next.tensor_system_bases.assign(next.package.tensors.size(), 0);

    for (const AiMemoryPlanEntry& entry : next.package.memory_plan) {
        const AiTensorMetadata& tensor = next.package.tensors[entry.tensor_index];
        if (tensor.role == AiTensorRole::Intermediate) {
            continue;
        }

        uint64_t system_addr = 0;
        if (!resolve_tensor_base(descriptor, tensor, entry.tensor_index, system_addr, fault, detail)) {
            return false;
        }
        next.tensor_system_bases[entry.tensor_index] = system_addr;
        if (tensor.role == AiTensorRole::Output) {
            next.store_entries.push_back(entry);
        } else {
            next.load_entries.push_back(entry);
        }
    }

    active_submission_ = std::move(next);
    active_submission_valid_ = true;
    return true;
}

bool AiAccelerator::read_graph_package_bytes(uint64_t addr,
                                             uint32_t size,
                                             std::vector<uint8_t>& bytes,
                                             std::string& error) const {
    bytes.assign(size, 0);
    DmaTransferResult result = bus_->dma_read(
        DmaTransaction{
            .initiator = "ai-accelerator-graph",
            .addr = addr,
            .size = size,
            .burst = size > 1,
            .direction = DmaDirection::Read,
        },
        bytes.data());
    if (!result.ok) {
        error = result.detail.empty() ? "AI graph package DMA read failed" : result.detail;
        return false;
    }
    return true;
}

bool AiAccelerator::read_u64_dma(uint64_t addr, uint64_t& value, std::string& error) const {
    std::array<uint8_t, 8> bytes{};
    DmaTransferResult result = bus_->dma_read(
        DmaTransaction{
            .initiator = "ai-accelerator-table",
            .addr = addr,
            .size = bytes.size(),
            .burst = true,
            .direction = DmaDirection::Read,
        },
        bytes.data());
    if (!result.ok) {
        error = result.detail.empty() ? "AI address table DMA read failed" : result.detail;
        return false;
    }
    value = read_le_u64(bytes.data());
    return true;
}

bool AiAccelerator::resolve_tensor_base(const AiSubmissionDescriptor& descriptor,
                                        const AiTensorMetadata& tensor,
                                        uint16_t tensor_index,
                                        uint64_t& system_addr,
                                        uint32_t& fault,
                                        uint32_t& detail) const {
    uint64_t table_base = 0;
    switch (tensor.role) {
    case AiTensorRole::Input:
    case AiTensorRole::Weight:
    case AiTensorRole::Constant:
        table_base = descriptor.input_table_addr;
        break;
    case AiTensorRole::Output:
        table_base = descriptor.output_table_addr;
        break;
    case AiTensorRole::Intermediate:
    case AiTensorRole::Invalid:
        system_addr = 0;
        return true;
    }

    if (table_base == 0) {
        fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        detail = tensor_index;
        return false;
    }

    uint64_t table_entry_addr = 0;
    if (!add_u64_u32(table_base, static_cast<uint32_t>(tensor_index) * 8U, table_entry_addr)) {
        fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        detail = tensor_index;
        return false;
    }

    std::string error;
    if (!read_u64_dma(table_entry_addr, system_addr, error)) {
        fault = AI_ACCEL_FAULT_DMA;
        detail = tensor_index;
        return false;
    }
    if (system_addr == 0) {
        fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        detail = tensor_index;
        return false;
    }
    return true;
}

bool AiAccelerator::start_next_transfer(uint32_t& fault, uint32_t& detail) {
    fault = AI_ACCEL_FAULT_NONE;
    detail = 0;

    if (!active_submission_valid_) {
        fault = AI_ACCEL_FAULT_EXECUTION;
        return false;
    }
    if (dma_engine_.busy()) {
        return true;
    }

    if (active_submission_.next_load_index < active_submission_.load_entries.size()) {
        const AiMemoryPlanEntry& entry =
            active_submission_.load_entries[active_submission_.next_load_index];
        uint64_t system_addr = 0;
        if (!add_u64_u32(active_submission_.tensor_system_bases[entry.tensor_index],
                         entry.system_offset,
                         system_addr)) {
            fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
            detail = entry.tensor_index;
            return false;
        }

        std::string error;
        if (!dma_engine_.start(
                AiDmaRequest{
                    .kind = AiDmaTransferKind::Load,
                    .system_addr = system_addr,
                    .space = AiScratchpadSpace::Scratchpad,
                    .scratchpad_offset = entry.scratchpad_offset,
                    .size = entry.scratchpad_bytes,
                    .initiator = "ai-accelerator",
                },
                fault,
                error)) {
            detail = entry.tensor_index;
            return false;
        }
        return true;
    }

    if (!active_submission_.compute_started) {
        return start_compute(fault, detail);
    }

    if (!active_submission_.compute_complete) {
        return true;
    }

    if (active_submission_.next_store_index < active_submission_.store_entries.size()) {
        const AiMemoryPlanEntry& entry =
            active_submission_.store_entries[active_submission_.next_store_index];
        uint64_t system_addr = 0;
        if (!add_u64_u32(active_submission_.tensor_system_bases[entry.tensor_index],
                         entry.system_offset,
                         system_addr)) {
            fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
            detail = entry.tensor_index;
            return false;
        }

        std::string error;
        if (!dma_engine_.start(
                AiDmaRequest{
                    .kind = AiDmaTransferKind::Store,
                    .system_addr = system_addr,
                    .space = AiScratchpadSpace::Scratchpad,
                    .scratchpad_offset = entry.scratchpad_offset,
                    .size = entry.scratchpad_bytes,
                    .initiator = "ai-accelerator",
                },
                fault,
                error)) {
            detail = entry.tensor_index;
            return false;
        }
        return true;
    }

    return false;
}

bool AiAccelerator::start_compute(uint32_t& fault, uint32_t& detail) {
    fault = AI_ACCEL_FAULT_NONE;
    detail = 0;

    AiGraphExecutionResult result{};
    std::string error;
    if (!scheduler_.execute(active_submission_.package, result, error)) {
        fault = result.fault == AI_ACCEL_FAULT_NONE ? AI_ACCEL_FAULT_EXECUTION : result.fault;
        detail = result.fault_detail;
        return false;
    }

    active_submission_.compute_started = true;
    active_submission_.compute_complete = false;
    active_submission_.retired_ops = result.retired_ops;
    active_submission_.compute_cycles_remaining = result.compute_cycles;
    active_submission_.stall_cycles_remaining = result.stall_cycles;
    profile_summary_.tile_count = result.tile_count;
    profile_summary_.scratchpad_peak_bytes = result.scratchpad_peak_bytes;
    profile_summary_.op_summaries = result.op_summaries;
    if (active_submission_.compute_cycles_remaining == 0 &&
        active_submission_.stall_cycles_remaining == 0) {
        active_submission_.compute_complete = true;
    }
    return active_submission_.compute_complete ? false : true;
}

bool AiAccelerator::complete_descriptor(const AiSubmissionDescriptor& descriptor,
                                        uint32_t fault,
                                        uint32_t detail,
                                        uint64_t bytes_moved,
                                        uint64_t retired_ops) {
    if (!queue_.completion_has_space()) {
        set_fault(AI_ACCEL_FAULT_COMPLETION_QUEUE_FULL, 0);
        irq_status_ |= AI_ACCEL_IRQ_FAULT;
        pending_submission_budget_ = 0;
        return false;
    }
    if (!write_completion(descriptor, fault, bytes_moved, retired_ops)) {
        pending_submission_budget_ = 0;
        return false;
    }

    ++completion_cycles_;
    retired_ops_ += retired_ops;
    queue_.advance_submission();
    queue_.advance_completion();
    ++submission_count_;
    ++completion_count_;
    irq_status_ |= AI_ACCEL_IRQ_COMPLETION;
    if (fault != AI_ACCEL_FAULT_NONE) {
        set_fault(fault, detail);
        irq_status_ |= AI_ACCEL_IRQ_FAULT;
    }
    return true;
}

uint32_t AiAccelerator::validate_descriptor(const AiSubmissionDescriptor& descriptor) const {
    if (descriptor.graph_package_addr == 0 ||
        descriptor.graph_package_bytes == 0 ||
        descriptor.graph_package_bytes > AI_ACCEL_MAX_GRAPH_PACKAGE_BYTES) {
        return AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
    }
    if ((descriptor.flags & ~AI_ACCEL_SUBMISSION_FLAG_PROFILE) != 0) {
        return AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
    }
    if (bus_ == nullptr) {
        return AI_ACCEL_FAULT_EXECUTION;
    }
    return AI_ACCEL_FAULT_NONE;
}

uint32_t AiAccelerator::graph_package_fault(const std::string& error) const {
    if (error.find("scratchpad") != std::string::npos) {
        return AI_ACCEL_FAULT_SCRATCHPAD_OVERFLOW;
    }
    return AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
}

bool AiAccelerator::write_completion(const AiSubmissionDescriptor& descriptor,
                                     uint32_t fault,
                                     uint64_t bytes_moved,
                                     uint64_t retired_ops) {
    AiCompletionEntry completion{
        .token = descriptor.token,
        .status = fault == AI_ACCEL_FAULT_NONE ? AI_ACCEL_COMPLETION_STATUS_SUCCESS
                                               : AI_ACCEL_COMPLETION_STATUS_FAULT,
        .fault_code = fault,
        .retired_ops = retired_ops,
        .bytes_moved = bytes_moved,
        .source_tag = descriptor.source_tag,
    };

    std::string error;
    if (!queue_.write_next_completion(*bus_, completion, error)) {
        set_fault(AI_ACCEL_FAULT_DMA, 0);
        irq_status_ |= AI_ACCEL_IRQ_FAULT;
        return false;
    }
    return true;
}

void AiAccelerator::clear_active_submission() {
    active_submission_ = {};
    active_submission_valid_ = false;
}

void AiAccelerator::set_fault(uint32_t fault, uint32_t detail) {
    last_fault_ = fault;
    fault_detail_ = detail;
    ++fault_count_;
}

void AiAccelerator::clear_fault() {
    last_fault_ = AI_ACCEL_FAULT_NONE;
    fault_detail_ = 0;
}

void AiAccelerator::reset_device() {
    queue_.reset();
    scratchpad_.configure(0, 0, 0);
    scratchpad_.reset();
    dma_engine_.reset();
    clear_active_submission();
    pending_submission_budget_ = 0;
    irq_status_ = 0;
    irq_mask_ = AI_ACCEL_IRQ_ALL;
    clear_fault();
    doorbell_count_ = 0;
    submission_count_ = 0;
    completion_count_ = 0;
    fault_count_ = 0;
    device_cycles_ = 0;
    compute_cycles_ = 0;
    stall_cycles_ = 0;
    queue_cycles_ = 0;
    completion_cycles_ = 0;
    retired_ops_ = 0;
    profile_summary_ = {};
    update_interrupt_line();
}

void AiAccelerator::update_interrupt_line() {
    plic_.set_source_level(irq_source_, (irq_status_ & irq_mask_) != 0);
}
