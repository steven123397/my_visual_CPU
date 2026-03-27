#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../arch/core_state.h"

struct DebugStageSnapshot {
    bool valid{false};
    uint64_t pc{0};
    uint32_t raw{0};
    std::string text{};
};

struct PredictorDebugSnapshot {
    std::string mode{};
    bool last_prediction_valid{false};
    bool last_prediction_taken{false};
    bool last_prediction_correct{false};
    uint64_t last_prediction_pc{0};
    uint64_t last_prediction_target{0};
    bool last_mispredict_valid{false};
    uint64_t last_mispredict_pc{0};
    uint64_t last_mispredict_target{0};
    uint64_t total_predictions{0};
    uint64_t correct_predictions{0};
    uint64_t mispredictions{0};
};

struct PipelineDebugSnapshot {
    DebugStageSnapshot if_stage{};
    DebugStageSnapshot id_stage{};
    DebugStageSnapshot ex_stage{};
    DebugStageSnapshot mem_stage{};
    DebugStageSnapshot wb_stage{};
    bool stalled{false};
    bool redirected{false};
    uint64_t redirect_target{0};
    bool pending_fetch_fault{false};
    bool trap_flush{false};
    bool committed{false};
    bool empty{true};
    PredictorDebugSnapshot predictor{};
};

struct BackendDebugSnapshot {
    std::string backend_name{};
    PipelineDebugSnapshot pipeline{};
};

struct DebugBusAccess {
    bool valid{false};
    bool write{false};
    bool mmio{false};
    uint64_t addr{0};
    uint64_t value{0};
    int size{0};
    std::string device{};
};

struct DebugEvent {
    uint64_t cycle{0};
    std::string kind{};
    std::string detail{};
};

struct DebugCsrSnapshot {
    uint64_t mstatus{0};
    uint64_t sstatus{0};
    uint64_t mepc{0};
    uint64_t sepc{0};
    uint64_t mcause{0};
    uint64_t scause{0};
    uint64_t mtval{0};
    uint64_t stval{0};
    uint64_t mie{0};
    uint64_t mip{0};
    uint64_t sie{0};
    uint64_t sip{0};
    uint64_t mtvec{0};
    uint64_t stvec{0};
    uint64_t satp{0};
};

struct DebugUartSnapshot {
    uint8_t ier{0};
    bool thre_interrupt_asserted{false};
    size_t output_size{0};
    std::string recent_output{};
};

struct DebugClintSnapshot {
    uint64_t mtime{0};
    uint64_t mtimecmp{0};
    bool timer_interrupt_pending{false};
};

struct DebugPlicSnapshot {
    uint32_t priority{0};
    bool level{false};
    bool pending{false};
    bool claimed{false};
    uint32_t machine_enables{0};
    uint32_t supervisor_enables{0};
    uint32_t machine_threshold{0};
    uint32_t supervisor_threshold{0};
    bool machine_has_pending{false};
    bool supervisor_has_pending{false};
};

struct DebugStorageSnapshot {
    bool attached{false};
    uint64_t status{0};
    uint64_t capacity_blocks{0};
    uint64_t lba{0};
    uint64_t block_count{0};
    uint64_t error_code{0};
};

struct DebugDeviceSnapshot {
    DebugUartSnapshot uart{};
    DebugClintSnapshot clint{};
    DebugPlicSnapshot plic{};
    DebugStorageSnapshot storage{};
};

struct DebugSummarySnapshot {
    uint64_t cycle{0};
    uint64_t instret{0};
    uint64_t pc{0};
    bool halted{false};
    PrivilegeMode privilege{PrivilegeMode::Machine};
    std::string backend{};
};

struct DebugSnapshot {
    DebugSummarySnapshot summary{};
    PipelineDebugSnapshot pipeline{};
    std::array<uint64_t, 32> gpr{};
    DebugCsrSnapshot csrs{};
    DebugBusAccess bus{};
    DebugDeviceSnapshot devices{};
    std::vector<DebugEvent> events{};
};
