#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../platform/machine.h"

class DebugSession {
public:
    void load_elf(const std::string& path, BackendKind backend_kind, const char* disk_image);
    void load_binary(const std::string& path, uint64_t addr, BackendKind backend_kind, const char* disk_image);
    void reset();
    void step_cycle();
    void step_commit();
    DebugSnapshot snapshot() const;

private:
    DebugSnapshot collect_snapshot() const;
    void append_event(const char* kind, const std::string& detail);
    void record_step_events(const DebugSnapshot& before, const DebugSnapshot& after);
    void ensure_loaded() const;

    Machine machine_{};
    std::vector<DebugEvent> events_{};
};
