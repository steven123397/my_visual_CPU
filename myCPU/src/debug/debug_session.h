#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "debug_snapshot.h"
#include "../exec/dbt_jit_engine.h"
#include "../platform/address_map.h"
#include "../platform/machine.h"

class DebugSession {
public:
    struct UartOutputChunk {
        size_t offset{0};
        size_t next_offset{0};
        std::string text{};
    };

    struct TranslationPlanSnapshot {
        bool candidate{false};
        bool inlineable{false};
        uint64_t start_pc{0};
        uint64_t end_pc{0};
        uint64_t executions{0};
        uint64_t retired_instructions{0};
        uint64_t inlineable_instructions{0};
        uint64_t fallback_pc{0};
        std::string reason{};
        std::string boundary_kind{};
    };

    void load_elf(const std::string& path,
                  BackendKind backend_kind,
                  BlockTransport block_transport = BlockTransport::SimpleStorage,
                  const char* disk_image = nullptr,
                  bool disk_ready = true,
                  bool disk_magic_valid = true,
                  bool l1d_enabled = false);
    void load_binary(const std::string& path,
                     uint64_t addr,
                     BackendKind backend_kind,
                     BlockTransport block_transport = BlockTransport::SimpleStorage,
                     const char* disk_image = nullptr,
                     bool disk_ready = true,
                     bool disk_magic_valid = true,
                     bool l1d_enabled = false);
    void load_binary_payload(const std::string& path, uint64_t addr);
    void set_gpr(std::string_view reg_name, uint64_t value);
    void set_memory(uint64_t addr, uint64_t value, uint32_t size, bool virtual_address);
    void set_csr(std::string_view csr_name, uint64_t value);
    void break_at(uint64_t addr);
    void reset();
    void step_cycle();
    void step_commit();
    void run_until_uart_contains(std::string_view text, uint64_t max_steps);
    UartOutputChunk run_until_new_uart_contains(size_t offset,
                                                std::string_view text,
                                                uint64_t max_steps);
    void run_until_halt(uint64_t max_steps);
    void uart_input(std::string_view text);
    UartOutputChunk uart_output(size_t offset) const;
    void debug_step_raw();
    bool debug_halted() const;
    bool debug_try_load_guest_memory(uint64_t addr, int size, uint64_t& value);
    TranslationPlanSnapshot translation_plan();
    DbtJitDryRunSummary jit_dispatch();
    DebugSnapshot snapshot() const;
    size_t post_load_action_count() const;

private:
    enum class ImageKind : uint8_t {
        None,
        Elf,
        Binary,
    };

    struct PostLoadAction {
        enum class Kind : uint8_t {
            Payload,
            SetGpr,
        };

        Kind kind{Kind::Payload};
        std::string text{};
        uint64_t value{0};
    };

    struct LoadConfig {
        ImageKind image_kind{ImageKind::None};
        std::string image_path{};
        uint64_t binary_addr{MEM_BASE};
        BackendKind backend_kind{BackendKind::Pipeline};
        BlockTransport block_transport{BlockTransport::SimpleStorage};
        std::string disk_image{};
        bool disk_attached{false};
        bool disk_ready{true};
        bool disk_magic_valid{true};
        bool l1d_enabled{false};
        std::vector<PostLoadAction> post_load_actions{};
    };

    void recreate_machine();
    DebugSnapshot collect_snapshot() const;
    void append_event(const char* kind, const std::string& detail);
    void record_step_events(const DebugSnapshot& before, const DebugSnapshot& after);
    bool is_breakpoint_pc(uint64_t pc) const;
    bool record_breakpoint_hit(uint64_t pc);
    bool record_current_breakpoint_hit();
    void ensure_loaded() const;
    Machine& machine();
    const Machine& machine() const;

    std::unique_ptr<Machine> machine_{};
    LoadConfig config_{};
    std::vector<DebugEvent> events_{};
    std::vector<uint64_t> breakpoints_{};
};
