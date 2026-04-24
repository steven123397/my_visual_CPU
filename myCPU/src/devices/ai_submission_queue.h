#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

class Bus;

inline constexpr size_t kAiSubmissionDescriptorBytes = 48;
inline constexpr size_t kAiCompletionEntryBytes = 40;
inline constexpr uint32_t kAiQueueMaxEntries = 1024;

inline constexpr uint32_t AI_ACCEL_SUBMISSION_FLAG_PROFILE = 0x1;

inline constexpr uint32_t AI_ACCEL_COMPLETION_STATUS_SUCCESS = 0;
inline constexpr uint32_t AI_ACCEL_COMPLETION_STATUS_FAULT = 1;

inline constexpr uint32_t AI_ACCEL_FAULT_NONE = 0;
inline constexpr uint32_t AI_ACCEL_FAULT_INVALID_DESCRIPTOR = 1;
inline constexpr uint32_t AI_ACCEL_FAULT_UNSUPPORTED_DTYPE = 2;
inline constexpr uint32_t AI_ACCEL_FAULT_ILLEGAL_OP = 3;
inline constexpr uint32_t AI_ACCEL_FAULT_DMA = 4;
inline constexpr uint32_t AI_ACCEL_FAULT_SCRATCHPAD_OVERFLOW = 5;
inline constexpr uint32_t AI_ACCEL_FAULT_EXECUTION = 6;
inline constexpr uint32_t AI_ACCEL_FAULT_TIMEOUT = 7;
inline constexpr uint32_t AI_ACCEL_FAULT_QUEUE_NOT_CONFIGURED = 8;
inline constexpr uint32_t AI_ACCEL_FAULT_COMPLETION_QUEUE_FULL = 9;

struct AiSubmissionDescriptor {
    uint64_t token{0};
    uint64_t graph_package_addr{0};
    uint32_t graph_package_bytes{0};
    uint32_t flags{0};
    uint64_t input_table_addr{0};
    uint64_t output_table_addr{0};
    uint32_t source_tag{0};
    uint32_t runtime_shape_table_offset{0};
};

struct AiCompletionEntry {
    uint64_t token{0};
    uint32_t status{AI_ACCEL_COMPLETION_STATUS_SUCCESS};
    uint32_t fault_code{AI_ACCEL_FAULT_NONE};
    uint64_t retired_ops{0};
    uint64_t bytes_moved{0};
    uint32_t source_tag{0};
};

void encode_ai_submission_descriptor(
    const AiSubmissionDescriptor& descriptor,
    std::array<uint8_t, kAiSubmissionDescriptorBytes>& bytes);
bool decode_ai_submission_descriptor(
    const std::array<uint8_t, kAiSubmissionDescriptorBytes>& bytes,
    AiSubmissionDescriptor& descriptor);
void encode_ai_completion_entry(
    const AiCompletionEntry& completion,
    std::array<uint8_t, kAiCompletionEntryBytes>& bytes);
bool decode_ai_completion_entry(
    const std::array<uint8_t, kAiCompletionEntryBytes>& bytes,
    AiCompletionEntry& completion);

class AiSubmissionQueue {
public:
    void reset();

    void set_submission_base(uint64_t base);
    void set_completion_base(uint64_t base);
    void set_submission_size(uint32_t entries);
    void set_completion_size(uint32_t entries);
    void set_submission_head(uint32_t head);
    void set_submission_tail(uint32_t tail);
    void set_completion_head(uint32_t head);
    void set_completion_tail(uint32_t tail);

    uint64_t submission_base() const;
    uint64_t completion_base() const;
    uint32_t submission_size() const;
    uint32_t completion_size() const;
    uint32_t submission_head() const;
    uint32_t submission_tail() const;
    uint32_t completion_head() const;
    uint32_t completion_tail() const;

    bool submission_configured() const;
    bool completion_configured() const;
    bool configured() const;
    uint32_t pending_depth() const;
    uint32_t completion_depth() const;
    bool completion_has_space() const;

    bool read_next_submission(Bus& bus, AiSubmissionDescriptor& descriptor, std::string& error) const;
    bool write_next_completion(Bus& bus, const AiCompletionEntry& completion, std::string& error) const;
    void advance_submission();
    void advance_completion();

private:
    uint64_t submission_record_addr() const;
    uint64_t completion_record_addr() const;

    uint64_t submission_base_{0};
    uint64_t completion_base_{0};
    uint32_t submission_size_{0};
    uint32_t completion_size_{0};
    uint32_t submission_head_{0};
    uint32_t submission_tail_{0};
    uint32_t completion_head_{0};
    uint32_t completion_tail_{0};
};
