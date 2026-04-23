#include "ai_submission_queue.h"

#include <algorithm>

#include "../mem/bus.h"

namespace {

void put_u32(std::array<uint8_t, kAiSubmissionDescriptorBytes>& bytes, size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xffU);
    }
}

void put_u64(std::array<uint8_t, kAiSubmissionDescriptorBytes>& bytes, size_t offset, uint64_t value) {
    for (size_t i = 0; i < 8; ++i) {
        bytes[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xffU);
    }
}

void put_completion_u32(std::array<uint8_t, kAiCompletionEntryBytes>& bytes, size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xffU);
    }
}

void put_completion_u64(std::array<uint8_t, kAiCompletionEntryBytes>& bytes, size_t offset, uint64_t value) {
    for (size_t i = 0; i < 8; ++i) {
        bytes[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xffU);
    }
}

uint32_t get_u32(const uint8_t* bytes, size_t offset) {
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) {
        value |= static_cast<uint32_t>(bytes[offset + i]) << (8 * i);
    }
    return value;
}

uint64_t get_u64(const uint8_t* bytes, size_t offset) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(bytes[offset + i]) << (8 * i);
    }
    return value;
}

uint32_t bounded_depth(uint32_t head, uint32_t tail, uint32_t size) {
    if (size == 0 || tail < head) {
        return 0;
    }
    return std::min<uint32_t>(tail - head, size);
}

std::string dma_error_detail(const DmaTransferResult& result) {
    if (!result.detail.empty()) {
        return result.detail;
    }
    return "AI queue DMA failed";
}

}  // namespace

void encode_ai_submission_descriptor(
    const AiSubmissionDescriptor& descriptor,
    std::array<uint8_t, kAiSubmissionDescriptorBytes>& bytes) {
    bytes.fill(0);
    put_u64(bytes, 0, descriptor.token);
    put_u64(bytes, 8, descriptor.graph_package_addr);
    put_u32(bytes, 16, descriptor.graph_package_bytes);
    put_u32(bytes, 20, descriptor.flags);
    put_u64(bytes, 24, descriptor.input_table_addr);
    put_u64(bytes, 32, descriptor.output_table_addr);
    put_u32(bytes, 40, descriptor.source_tag);
}

bool decode_ai_submission_descriptor(
    const std::array<uint8_t, kAiSubmissionDescriptorBytes>& bytes,
    AiSubmissionDescriptor& descriptor) {
    descriptor.token = get_u64(bytes.data(), 0);
    descriptor.graph_package_addr = get_u64(bytes.data(), 8);
    descriptor.graph_package_bytes = get_u32(bytes.data(), 16);
    descriptor.flags = get_u32(bytes.data(), 20);
    descriptor.input_table_addr = get_u64(bytes.data(), 24);
    descriptor.output_table_addr = get_u64(bytes.data(), 32);
    descriptor.source_tag = get_u32(bytes.data(), 40);
    return true;
}

void encode_ai_completion_entry(
    const AiCompletionEntry& completion,
    std::array<uint8_t, kAiCompletionEntryBytes>& bytes) {
    bytes.fill(0);
    put_completion_u64(bytes, 0, completion.token);
    put_completion_u32(bytes, 8, completion.status);
    put_completion_u32(bytes, 12, completion.fault_code);
    put_completion_u64(bytes, 16, completion.retired_ops);
    put_completion_u64(bytes, 24, completion.bytes_moved);
    put_completion_u32(bytes, 32, completion.source_tag);
}

bool decode_ai_completion_entry(
    const std::array<uint8_t, kAiCompletionEntryBytes>& bytes,
    AiCompletionEntry& completion) {
    completion.token = get_u64(bytes.data(), 0);
    completion.status = get_u32(bytes.data(), 8);
    completion.fault_code = get_u32(bytes.data(), 12);
    completion.retired_ops = get_u64(bytes.data(), 16);
    completion.bytes_moved = get_u64(bytes.data(), 24);
    completion.source_tag = get_u32(bytes.data(), 32);
    return true;
}

void AiSubmissionQueue::reset() {
    submission_base_ = 0;
    completion_base_ = 0;
    submission_size_ = 0;
    completion_size_ = 0;
    submission_head_ = 0;
    submission_tail_ = 0;
    completion_head_ = 0;
    completion_tail_ = 0;
}

void AiSubmissionQueue::set_submission_base(uint64_t base) {
    submission_base_ = base;
}

void AiSubmissionQueue::set_completion_base(uint64_t base) {
    completion_base_ = base;
}

void AiSubmissionQueue::set_submission_size(uint32_t entries) {
    submission_size_ = entries <= kAiQueueMaxEntries ? entries : 0;
    submission_head_ = 0;
    submission_tail_ = 0;
}

void AiSubmissionQueue::set_completion_size(uint32_t entries) {
    completion_size_ = entries <= kAiQueueMaxEntries ? entries : 0;
    completion_head_ = 0;
    completion_tail_ = 0;
}

void AiSubmissionQueue::set_submission_head(uint32_t head) {
    submission_head_ = head;
}

void AiSubmissionQueue::set_submission_tail(uint32_t tail) {
    submission_tail_ = tail;
}

void AiSubmissionQueue::set_completion_head(uint32_t head) {
    completion_head_ = head;
}

void AiSubmissionQueue::set_completion_tail(uint32_t tail) {
    completion_tail_ = tail;
}

uint64_t AiSubmissionQueue::submission_base() const {
    return submission_base_;
}

uint64_t AiSubmissionQueue::completion_base() const {
    return completion_base_;
}

uint32_t AiSubmissionQueue::submission_size() const {
    return submission_size_;
}

uint32_t AiSubmissionQueue::completion_size() const {
    return completion_size_;
}

uint32_t AiSubmissionQueue::submission_head() const {
    return submission_head_;
}

uint32_t AiSubmissionQueue::submission_tail() const {
    return submission_tail_;
}

uint32_t AiSubmissionQueue::completion_head() const {
    return completion_head_;
}

uint32_t AiSubmissionQueue::completion_tail() const {
    return completion_tail_;
}

bool AiSubmissionQueue::submission_configured() const {
    return submission_base_ != 0 && submission_size_ != 0;
}

bool AiSubmissionQueue::completion_configured() const {
    return completion_base_ != 0 && completion_size_ != 0;
}

bool AiSubmissionQueue::configured() const {
    return submission_configured() && completion_configured();
}

uint32_t AiSubmissionQueue::pending_depth() const {
    return bounded_depth(submission_head_, submission_tail_, submission_size_);
}

uint32_t AiSubmissionQueue::completion_depth() const {
    return bounded_depth(completion_head_, completion_tail_, completion_size_);
}

bool AiSubmissionQueue::completion_has_space() const {
    return completion_configured() && completion_depth() < completion_size_;
}

bool AiSubmissionQueue::read_next_submission(Bus& bus, AiSubmissionDescriptor& descriptor, std::string& error) const {
    if (!submission_configured() || pending_depth() == 0) {
        error = "AI submission queue is not ready";
        return false;
    }

    std::array<uint8_t, kAiSubmissionDescriptorBytes> bytes{};
    DmaTransferResult result = bus.dma_read(
        DmaTransaction{
            .initiator = "ai-accelerator",
            .addr = submission_record_addr(),
            .size = bytes.size(),
            .burst = true,
            .direction = DmaDirection::Read,
        },
        bytes.data());
    if (!result.ok) {
        error = dma_error_detail(result);
        return false;
    }
    return decode_ai_submission_descriptor(bytes, descriptor);
}

bool AiSubmissionQueue::write_next_completion(Bus& bus, const AiCompletionEntry& completion, std::string& error) const {
    if (!completion_has_space()) {
        error = "AI completion queue is full or not ready";
        return false;
    }

    std::array<uint8_t, kAiCompletionEntryBytes> bytes{};
    encode_ai_completion_entry(completion, bytes);
    DmaTransferResult result = bus.dma_write(
        DmaTransaction{
            .initiator = "ai-accelerator",
            .addr = completion_record_addr(),
            .size = bytes.size(),
            .burst = true,
            .direction = DmaDirection::Write,
        },
        bytes.data());
    if (!result.ok) {
        error = dma_error_detail(result);
        return false;
    }
    return true;
}

void AiSubmissionQueue::advance_submission() {
    ++submission_head_;
}

void AiSubmissionQueue::advance_completion() {
    ++completion_tail_;
}

uint64_t AiSubmissionQueue::submission_record_addr() const {
    return submission_base_ +
           (static_cast<uint64_t>(submission_head_ % submission_size_) * kAiSubmissionDescriptorBytes);
}

uint64_t AiSubmissionQueue::completion_record_addr() const {
    return completion_base_ +
           (static_cast<uint64_t>(completion_tail_ % completion_size_) * kAiCompletionEntryBytes);
}
