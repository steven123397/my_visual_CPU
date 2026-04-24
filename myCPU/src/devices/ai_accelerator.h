#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ai_dma_engine.h"
#include "ai_graph_package.h"
#include "ai_graph_scheduler.h"
#include "ai_submission_queue.h"
#include "device.h"
#include "../debug/debug_snapshot.h"
#include "../platform/address_map.h"

class Bus;
class Plic;

inline constexpr uint32_t AI_ACCEL_REG_MAGIC = 0x000;
inline constexpr uint32_t AI_ACCEL_REG_VERSION = 0x004;
inline constexpr uint32_t AI_ACCEL_REG_CAPABILITY = 0x008;
inline constexpr uint32_t AI_ACCEL_REG_STATUS = 0x00c;
inline constexpr uint32_t AI_ACCEL_REG_CONTROL = 0x010;
inline constexpr uint32_t AI_ACCEL_REG_QUEUE_DEPTH = 0x014;
inline constexpr uint32_t AI_ACCEL_REG_SUBMIT_QUEUE_BASE_LOW = 0x018;
inline constexpr uint32_t AI_ACCEL_REG_SUBMIT_QUEUE_BASE_HIGH = 0x01c;
inline constexpr uint32_t AI_ACCEL_REG_SUBMIT_QUEUE_SIZE = 0x020;
inline constexpr uint32_t AI_ACCEL_REG_SUBMIT_QUEUE_HEAD = 0x024;
inline constexpr uint32_t AI_ACCEL_REG_SUBMIT_QUEUE_TAIL = 0x028;
inline constexpr uint32_t AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW = 0x02c;
inline constexpr uint32_t AI_ACCEL_REG_COMPLETE_QUEUE_BASE_HIGH = 0x030;
inline constexpr uint32_t AI_ACCEL_REG_COMPLETE_QUEUE_SIZE = 0x034;
inline constexpr uint32_t AI_ACCEL_REG_COMPLETE_QUEUE_HEAD = 0x038;
inline constexpr uint32_t AI_ACCEL_REG_COMPLETE_QUEUE_TAIL = 0x03c;
inline constexpr uint32_t AI_ACCEL_REG_DOORBELL = 0x040;
inline constexpr uint32_t AI_ACCEL_REG_IRQ_STATUS = 0x044;
inline constexpr uint32_t AI_ACCEL_REG_IRQ_MASK = 0x048;
inline constexpr uint32_t AI_ACCEL_REG_IRQ_ACK = 0x04c;
inline constexpr uint32_t AI_ACCEL_REG_LAST_FAULT = 0x050;
inline constexpr uint32_t AI_ACCEL_REG_FAULT_DETAIL = 0x054;
inline constexpr uint32_t AI_ACCEL_REG_DOORBELL_COUNT_LOW = 0x058;
inline constexpr uint32_t AI_ACCEL_REG_DOORBELL_COUNT_HIGH = 0x05c;
inline constexpr uint32_t AI_ACCEL_REG_COMPLETION_COUNT_LOW = 0x060;
inline constexpr uint32_t AI_ACCEL_REG_COMPLETION_COUNT_HIGH = 0x064;
inline constexpr uint32_t AI_ACCEL_REG_SUBMISSION_COUNT_LOW = 0x068;
inline constexpr uint32_t AI_ACCEL_REG_SUBMISSION_COUNT_HIGH = 0x06c;
inline constexpr uint32_t AI_ACCEL_REG_FAULT_COUNT_LOW = 0x070;
inline constexpr uint32_t AI_ACCEL_REG_FAULT_COUNT_HIGH = 0x074;
inline constexpr uint32_t AI_ACCEL_REG_DEVICE_CYCLES_LOW = 0x078;
inline constexpr uint32_t AI_ACCEL_REG_DEVICE_CYCLES_HIGH = 0x07c;
inline constexpr uint32_t AI_ACCEL_REG_DMA_CYCLES_LOW = 0x080;
inline constexpr uint32_t AI_ACCEL_REG_DMA_CYCLES_HIGH = 0x084;
inline constexpr uint32_t AI_ACCEL_REG_DMA_LOAD_CYCLES_LOW = 0x088;
inline constexpr uint32_t AI_ACCEL_REG_DMA_LOAD_CYCLES_HIGH = 0x08c;
inline constexpr uint32_t AI_ACCEL_REG_DMA_STORE_CYCLES_LOW = 0x090;
inline constexpr uint32_t AI_ACCEL_REG_DMA_STORE_CYCLES_HIGH = 0x094;
inline constexpr uint32_t AI_ACCEL_REG_DMA_LOAD_BYTES_LOW = 0x098;
inline constexpr uint32_t AI_ACCEL_REG_DMA_LOAD_BYTES_HIGH = 0x09c;
inline constexpr uint32_t AI_ACCEL_REG_DMA_STORE_BYTES_LOW = 0x0a0;
inline constexpr uint32_t AI_ACCEL_REG_DMA_STORE_BYTES_HIGH = 0x0a4;
inline constexpr uint32_t AI_ACCEL_REG_DMA_SETUP_CYCLES = 0x0a8;
inline constexpr uint32_t AI_ACCEL_REG_DMA_BYTES_PER_CYCLE = 0x0ac;
inline constexpr uint32_t AI_ACCEL_REG_COMPUTE_CYCLES_LOW = 0x0b0;
inline constexpr uint32_t AI_ACCEL_REG_COMPUTE_CYCLES_HIGH = 0x0b4;
inline constexpr uint32_t AI_ACCEL_REG_STALL_CYCLES_LOW = 0x0b8;
inline constexpr uint32_t AI_ACCEL_REG_STALL_CYCLES_HIGH = 0x0bc;
inline constexpr uint32_t AI_ACCEL_REG_BUSY_CYCLES_LOW = 0x0c0;
inline constexpr uint32_t AI_ACCEL_REG_BUSY_CYCLES_HIGH = 0x0c4;
inline constexpr uint32_t AI_ACCEL_REG_QUEUE_CYCLES_LOW = 0x0c8;
inline constexpr uint32_t AI_ACCEL_REG_QUEUE_CYCLES_HIGH = 0x0cc;
inline constexpr uint32_t AI_ACCEL_REG_COMPLETION_CYCLES_LOW = 0x0d0;
inline constexpr uint32_t AI_ACCEL_REG_COMPLETION_CYCLES_HIGH = 0x0d4;
inline constexpr uint32_t AI_ACCEL_REG_EFFECTIVE_OPS_PER_CYCLE = 0x0d8;
inline constexpr uint32_t AI_ACCEL_REG_UTILIZATION = 0x0dc;

inline constexpr uint32_t AI_ACCEL_CONTROL_RESET = 0x1;

inline constexpr uint32_t AI_ACCEL_STATUS_READY = 0x1;
inline constexpr uint32_t AI_ACCEL_STATUS_BUSY = 0x2;
inline constexpr uint32_t AI_ACCEL_STATUS_FAULT = 0x4;
inline constexpr uint32_t AI_ACCEL_STATUS_IRQ = 0x8;

inline constexpr uint32_t AI_ACCEL_IRQ_COMPLETION = 0x1;
inline constexpr uint32_t AI_ACCEL_IRQ_FAULT = 0x2;
inline constexpr uint32_t AI_ACCEL_IRQ_ALL = AI_ACCEL_IRQ_COMPLETION | AI_ACCEL_IRQ_FAULT;

inline constexpr uint32_t AI_ACCEL_CAP_QUEUE = 0x1;
inline constexpr uint32_t AI_ACCEL_CAP_QUANTIZED = 0x2;
inline constexpr uint32_t AI_ACCEL_CAP_SEMI_PRECISION = 0x4;
inline constexpr uint32_t AI_ACCEL_CAP_STATIC_GRAPH = 0x8;
inline constexpr uint32_t AI_ACCEL_CAP_PROFILE = 0x10;
inline constexpr uint32_t AI_ACCEL_CAPABILITIES =
    AI_ACCEL_CAP_QUEUE | AI_ACCEL_CAP_QUANTIZED | AI_ACCEL_CAP_SEMI_PRECISION |
    AI_ACCEL_CAP_STATIC_GRAPH | AI_ACCEL_CAP_PROFILE;

inline constexpr uint32_t AI_ACCEL_MAX_GRAPH_PACKAGE_BYTES = 1024 * 1024;

using AiAcceleratorOpProfileSummary = AiOpProfileSummary;

struct AiAcceleratorProfileSummary {
    uint64_t tile_count{0};
    uint32_t scratchpad_peak_bytes{0};
    std::vector<AiAcceleratorOpProfileSummary> op_summaries{};
};

struct AiActiveSubmissionState {
    AiSubmissionDescriptor descriptor{};
    AiGraphPackage package{};
    std::vector<uint64_t> tensor_system_bases{};
    std::vector<AiMemoryPlanEntry> load_entries{};
    std::vector<AiMemoryPlanEntry> store_entries{};
    size_t next_load_index{0};
    size_t next_store_index{0};
    bool compute_started{false};
    bool compute_complete{false};
    uint64_t bytes_moved{0};
    uint64_t retired_ops{0};
    uint64_t compute_cycles_remaining{0};
    uint64_t stall_cycles_remaining{0};
};

class AiAccelerator : public Device {
public:
    AiAccelerator(Plic& plic,
                  uint32_t irq_source,
                  uint64_t base = AI_ACCEL_BASE,
                  uint64_t size = AI_ACCEL_SIZE);

    void bind_bus(Bus& bus);

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;
    PlatformEvents tick() override;
    const char* debug_name() const override {
        return "ai_accelerator";
    }

    uint64_t doorbell_count() const;
    uint64_t completion_count() const;
    uint32_t last_fault() const;
    DebugAiAcceleratorSnapshot debug_snapshot() const;
    const AiAcceleratorProfileSummary& profile_summary() const;

private:
    bool is_busy() const;
    uint32_t status() const;
    uint64_t busy_cycles() const;
    uint32_t effective_ops_per_cycle() const;
    uint32_t utilization() const;
    uint32_t counter_low(uint64_t value) const;
    uint32_t counter_high(uint64_t value) const;
    void write_queue_base_low(bool submission, uint32_t value);
    void write_queue_base_high(bool submission, uint32_t value);
    void ring_doorbell(uint32_t budget);
    void pump_queue();
    bool begin_submission();
    bool prepare_active_submission(const AiSubmissionDescriptor& descriptor,
                                   uint32_t& fault,
                                   uint32_t& detail);
    bool read_graph_package_bytes(uint64_t addr,
                                  uint32_t size,
                                  std::vector<uint8_t>& bytes,
                                  std::string& error) const;
    bool read_u64_dma(uint64_t addr, uint64_t& value, std::string& error) const;
    bool resolve_tensor_base(const AiSubmissionDescriptor& descriptor,
                             const AiTensorMetadata& tensor,
                             uint16_t tensor_index,
                             uint64_t& system_addr,
                             uint32_t& fault,
                             uint32_t& detail) const;
    bool start_compute(uint32_t& fault, uint32_t& detail);
    bool start_next_transfer(uint32_t& fault, uint32_t& detail);
    bool complete_descriptor(const AiSubmissionDescriptor& descriptor,
                             uint32_t fault,
                             uint32_t detail,
                             uint64_t bytes_moved,
                             uint64_t retired_ops);
    uint32_t validate_descriptor(const AiSubmissionDescriptor& descriptor) const;
    uint32_t graph_package_fault(const std::string& error) const;
    bool write_completion(const AiSubmissionDescriptor& descriptor,
                          uint32_t fault,
                          uint64_t bytes_moved,
                          uint64_t retired_ops);
    void clear_active_submission();
    void set_fault(uint32_t fault, uint32_t detail);
    void clear_fault();
    void reset_device();
    void update_interrupt_line();

    Plic& plic_;
    uint32_t irq_source_{0};
    Bus* bus_{nullptr};
    AiSubmissionQueue queue_{};
    AiScratchpad scratchpad_{};
    AiDmaEngine dma_engine_;
    AiGraphScheduler scheduler_{scratchpad_};
    uint32_t irq_status_{0};
    uint32_t irq_mask_{AI_ACCEL_IRQ_ALL};
    uint32_t last_fault_{AI_ACCEL_FAULT_NONE};
    uint32_t fault_detail_{0};
    uint32_t pending_submission_budget_{0};
    AiActiveSubmissionState active_submission_{};
    bool active_submission_valid_{false};
    uint64_t doorbell_count_{0};
    uint64_t submission_count_{0};
    uint64_t completion_count_{0};
    uint64_t fault_count_{0};
    uint64_t device_cycles_{0};
    uint64_t compute_cycles_{0};
    uint64_t stall_cycles_{0};
    uint64_t queue_cycles_{0};
    uint64_t completion_cycles_{0};
    uint64_t retired_ops_{0};
    AiAcceleratorProfileSummary profile_summary_{};
};
