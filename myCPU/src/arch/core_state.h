#pragma once

#include <array>
#include <cstdint>

enum class PrivilegeMode : uint8_t {
    User = 0,
    Supervisor = 1,
    Machine = 3,
};

class CoreState {
public:
    void reset(uint64_t entry);

    uint64_t read_gpr(uint32_t idx) const;
    void write_gpr(uint32_t idx, uint64_t value);

    uint64_t pc() const;
    void set_pc(uint64_t value);

    uint64_t cycle() const;
    void advance_cycle(uint64_t delta = 1);

    bool halted() const;
    void set_halted(bool halted);

    PrivilegeMode privilege_mode() const;
    void set_privilege_mode(PrivilegeMode mode);

private:
    std::array<uint64_t, 32> gpr_{};
    uint64_t pc_{0};
    uint64_t cycle_{0};
    bool halted_{false};
    PrivilegeMode privilege_mode_{PrivilegeMode::Machine};
};
