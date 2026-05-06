#pragma once

#include <array>
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

struct FpRegWrite {
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
    enum class Target : uint8_t {
        Integer,
        Float,
    };

    Kind kind{Kind::None};
    Target target{Target::Integer};
    uint64_t addr{0};
    uint64_t store_value{0};
    uint8_t rd{0};
    int size{0};
    bool sign_extend{false};
    bool commit_at_boundary{false};
    bool non_speculative{false};
};

struct AtomicRequest {
    enum class Kind : uint8_t {
        None,
        LoadReserved,
        StoreConditional,
        Swap,
        Add,
        Xor,
        And,
        Or,
        Min,
        Max,
        MinUnsigned,
        MaxUnsigned,
    };

    Kind kind{Kind::None};
    uint64_t addr{0};
    uint64_t store_value{0};
    uint8_t rd{0};
    int size{0};
    bool aq{false};
    bool rl{false};
    bool commit_at_boundary{true};
    bool non_speculative{true};
};

struct VectorRequest {
    enum class Kind : uint8_t {
        None,
        SetConfig,
        Load,
        Store,
        Add,
        Mul,
        Max,
        Dot,
    };

    Kind kind{Kind::None};
    uint8_t vd{0};
    uint8_t vs1{0};
    uint8_t vs2{0};
    uint64_t addr{0};
    uint8_t sew_bytes{0};
    uint8_t vl{0};
    bool result_valid{false};
    std::array<uint8_t, 16> result{};
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
    FpRegWrite fp_write{};
    CsrWrite csr_write{};
    MemoryRequest mem{};
    AtomicRequest atomic{};
    VectorRequest vector{};
    TrapRequest trap{};
    ControlEffect control{};
    bool floating_state_touched{false};
    bool retired{true};
};
