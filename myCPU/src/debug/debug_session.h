#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "debug_snapshot.h"
#include "../platform/address_map.h"
#include "../platform/machine.h"

class DebugSession {
public:
    void load_elf(const std::string& path,
                  BackendKind backend_kind,
                  const char* disk_image,
                  bool disk_ready = true,
                  bool disk_magic_valid = true);
    void load_binary(const std::string& path,
                     uint64_t addr,
                     BackendKind backend_kind,
                     const char* disk_image,
                     bool disk_ready = true,
                     bool disk_magic_valid = true);
    void reset();
    void step_cycle();
    void step_commit();
    DebugSnapshot snapshot() const;

private:
    enum class ImageKind : uint8_t {
        None,
        Elf,
        Binary,
    };

    struct LoadConfig {
        ImageKind image_kind{ImageKind::None};
        std::string image_path{};
        uint64_t binary_addr{MEM_BASE};
        BackendKind backend_kind{BackendKind::Pipeline};
        std::string disk_image{};
        bool disk_attached{false};
        bool disk_ready{true};
        bool disk_magic_valid{true};
    };

    void recreate_machine();
    DebugSnapshot collect_snapshot() const;
    void append_event(const char* kind, const std::string& detail);
    void record_step_events(const DebugSnapshot& before, const DebugSnapshot& after);
    void ensure_loaded() const;
    Machine& machine();
    const Machine& machine() const;

    std::unique_ptr<Machine> machine_{};
    LoadConfig config_{};
    std::vector<DebugEvent> events_{};
};
