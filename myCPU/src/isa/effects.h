#pragma once

#include <cstdint>

struct TrapRequest {
    bool valid{false};
    uint64_t cause{0};
    uint64_t tval{0};
};

struct RegWrite {
    bool enable{false};
    uint8_t rd{0};
    uint64_t value{0};
};

struct CsrWrite {
    bool enable{false};
    uint32_t addr{0};
    uint64_t value{0};
};

struct MemoryRequest {
    enum class Kind : uint8_t {
        None,
        Load,
        Store,
    };

    Kind kind{Kind::None};
    uint64_t addr{0};
    uint64_t store_value{0};
    uint8_t rd{0};
    int size{0};
    bool sign_extend{false};
};

enum class TrapReturnKind : uint8_t {
    None,
    Mret,
    Sret,
};

struct ControlEffect {
    bool redirect_pc{false};
    uint64_t target_pc{0};
    bool halt{false};
    bool flush_tlb{false};
    TrapReturnKind trap_return{TrapReturnKind::None};
};

struct InsnEffects {
    RegWrite rd_write{};
    CsrWrite csr_write{};
    MemoryRequest mem{};
    TrapRequest trap{};
    ControlEffect control{};
    bool retired{true};
};
