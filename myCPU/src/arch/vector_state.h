#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class VectorState {
public:
    static constexpr size_t kRegCount = 32;
    static constexpr size_t kRegBytes = 16;
    using VectorReg = std::array<uint8_t, kRegBytes>;

    void reset() {
        for (VectorReg& reg : regs_) {
            reg.fill(0);
        }
        sew_bytes_ = 1;
        vl_ = 0;
    }

    const VectorReg& read_reg(uint32_t idx) const {
        return regs_[static_cast<size_t>(idx & 0x1FU)];
    }

    void write_reg(uint32_t idx, const VectorReg& value) {
        regs_[static_cast<size_t>(idx & 0x1FU)] = value;
    }

    bool set_config(uint8_t sew_bytes, uint8_t vl) {
        if (!is_valid_config(sew_bytes, vl, false)) {
            return false;
        }
        sew_bytes_ = sew_bytes;
        vl_ = vl;
        return true;
    }

    uint8_t sew_bytes() const {
        return sew_bytes_;
    }

    uint8_t vl() const {
        return vl_;
    }

    static bool is_valid_sew_bytes(uint8_t sew_bytes) {
        return sew_bytes == 1 || sew_bytes == 2 || sew_bytes == 4 || sew_bytes == 8;
    }

    static uint8_t max_vl_for_sew(uint8_t sew_bytes) {
        if (!is_valid_sew_bytes(sew_bytes)) {
            return 0;
        }
        return static_cast<uint8_t>(kRegBytes / sew_bytes);
    }

    static bool is_valid_config(uint8_t sew_bytes, uint8_t vl, bool allow_zero_vl) {
        const uint8_t max_vl = max_vl_for_sew(sew_bytes);
        if (max_vl == 0) {
            return false;
        }
        if (allow_zero_vl && vl == 0) {
            return true;
        }
        return vl >= 1 && vl <= max_vl;
    }

private:
    std::array<VectorReg, kRegCount> regs_{};
    uint8_t sew_bytes_{1};
    uint8_t vl_{0};
};
