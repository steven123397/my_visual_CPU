#include "ai_accelerator.h"

#include <algorithm>
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

}  // namespace

AiAccelerator::AiAccelerator(Plic& plic,
                             uint32_t irq_source,
                             uint64_t base,
                             uint64_t size)
    : Device(base, size), plic_(plic), irq_source_(irq_source) {}

void AiAccelerator::bind_bus(Bus& bus) {
    bus_ = &bus;
}

uint64_t AiAccelerator::load(uint64_t addr, int size) {
    if (!is_valid_mmio_width(size)) {
        invalid_access(addr, size);
    }

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

uint64_t AiAccelerator::doorbell_count() const {
    return doorbell_count_;
}

uint64_t AiAccelerator::completion_count() const {
    return completion_count_;
}

uint32_t AiAccelerator::last_fault() const {
    return last_fault_;
}

DebugAiAcceleratorSnapshot AiAccelerator::debug_snapshot() const {
    return DebugAiAcceleratorSnapshot{
        .present = true,
        .queue_depth = queue_.pending_depth(),
        .doorbell_count = doorbell_count_,
        .last_fault = last_fault_,
        .completion_count = completion_count_,
    };
}

uint32_t AiAccelerator::status() const {
    uint32_t value = AI_ACCEL_STATUS_READY;
    if (busy_) {
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
    busy_ = true;
    for (uint32_t i = 0; i < count; ++i) {
        process_one_submission();
    }
    busy_ = false;
    update_interrupt_line();
}

void AiAccelerator::process_one_submission() {
    AiSubmissionDescriptor descriptor{};
    std::string error;
    uint32_t fault = AI_ACCEL_FAULT_NONE;

    if (!queue_.completion_has_space()) {
        set_fault(AI_ACCEL_FAULT_COMPLETION_QUEUE_FULL, 0);
        irq_status_ |= AI_ACCEL_IRQ_FAULT;
        return;
    }
    if (!queue_.read_next_submission(*bus_, descriptor, error)) {
        fault = AI_ACCEL_FAULT_DMA;
        descriptor = AiSubmissionDescriptor{};
    } else {
        fault = validate_descriptor(descriptor);
    }

    if (!write_completion(descriptor, fault)) {
        return;
    }

    queue_.advance_submission();
    queue_.advance_completion();
    ++submission_count_;
    ++completion_count_;
    irq_status_ |= AI_ACCEL_IRQ_COMPLETION;
    if (fault != AI_ACCEL_FAULT_NONE) {
        set_fault(fault, descriptor.graph_package_bytes);
        irq_status_ |= AI_ACCEL_IRQ_FAULT;
    }
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

    const PhysicalSpanInfo span = bus_->describe_span(
        descriptor.graph_package_addr,
        descriptor.graph_package_bytes);
    if (!span.ok || !span.region.dma_visible || span.region.has_side_effect) {
        return AI_ACCEL_FAULT_DMA;
    }
    return AI_ACCEL_FAULT_NONE;
}

bool AiAccelerator::write_completion(const AiSubmissionDescriptor& descriptor, uint32_t fault) {
    AiCompletionEntry completion{
        .token = descriptor.token,
        .status = fault == AI_ACCEL_FAULT_NONE ? AI_ACCEL_COMPLETION_STATUS_SUCCESS
                                               : AI_ACCEL_COMPLETION_STATUS_FAULT,
        .fault_code = fault,
        .retired_ops = 0,
        .bytes_moved = 0,
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
    irq_status_ = 0;
    irq_mask_ = AI_ACCEL_IRQ_ALL;
    clear_fault();
    busy_ = false;
    doorbell_count_ = 0;
    submission_count_ = 0;
    completion_count_ = 0;
    fault_count_ = 0;
    update_interrupt_line();
}

void AiAccelerator::update_interrupt_line() {
    plic_.set_source_level(irq_source_, (irq_status_ & irq_mask_) != 0);
}
