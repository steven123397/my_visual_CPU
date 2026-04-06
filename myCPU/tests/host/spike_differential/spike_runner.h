#pragma once

#include <array>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <elf.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "final_state.h"

namespace spike_differential {
namespace {

constexpr uint64_t kSpikeImageAlign = 0x1000ULL;
constexpr uint64_t kSpikeBootBase = kEntry + 0x1000ULL;
constexpr uint32_t kJalX0Zero = 0x0000006fU;
constexpr uint32_t kScratchReg = 31;
constexpr uint64_t kSpikeHostCommBytes = sizeof(uint64_t) * 2;
constexpr uint64_t kSstatusMask = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_SPP | MSTATUS_SUM |
                                  MSTATUS_MXR;

enum class SpikeCaptureMode : uint8_t {
    ControlledExit,
    StepBudget,
};

enum class SpikeCaptureRegion : uint8_t {
    Program,
    TrapProgram,
};

struct SpikeScenarioPlan {
    SpikeCaptureMode capture_mode{SpikeCaptureMode::StepBudget};
    SpikeCaptureRegion capture_region{SpikeCaptureRegion::Program};
    uint64_t capture_pc{0};
    uint64_t final_pc{0};
    bool halted{false};
    bool timed_out{false};
    const char* exit_reason{"step_budget_exhausted"};
};

struct SpikeRunnerOptions {
    std::string spike_path{"spike"};
    int timeout_ms{5000};
    std::vector<std::string> extra_args{};
};

struct SpikeProcessResult {
    bool launched{false};
    bool timed_out{false};
    int exec_errno{0};
    int exit_status{0};
    int term_signal{0};
    std::string output{};
};

enum class SpikeErrorKind : uint8_t {
    None,
    MissingSpike,
    LaunchFailure,
    ParseFailure,
    Timeout,
    UnsupportedScenario,
};

struct SpikeRunResult {
    bool ok{false};
    SpikeErrorKind error_kind{SpikeErrorKind::None};
    FinalState final_state{};
    std::string message{};
    std::string raw_output{};
};

struct ParsedSpikeOutput {
    std::vector<uint64_t> numeric_values{};
    bool has_privilege{false};
    PrivilegeMode privilege{PrivilegeMode::Machine};
};

struct ScopedTempPath {
    std::string path{};
    bool keep{false};

    ScopedTempPath() = default;
    ScopedTempPath(const ScopedTempPath&) = delete;
    ScopedTempPath& operator=(const ScopedTempPath&) = delete;

    ScopedTempPath(ScopedTempPath&& other) noexcept : path(std::move(other.path)) {
        other.path.clear();
    }

    ScopedTempPath& operator=(ScopedTempPath&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        reset();
        path = std::move(other.path);
        other.path.clear();
        return *this;
    }

    ~ScopedTempPath() {
        reset();
    }

    void reset() {
        if (!path.empty() && !keep) {
            ::unlink(path.c_str());
        }
        if (!path.empty()) {
            path.clear();
        }
    }
};

struct Elf64_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

inline uint64_t align_up(uint64_t value, uint64_t align) {
    return (value + align - 1) & ~(align - 1);
}

inline std::string trim(std::string_view text) {
    size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

inline std::string summarize_spike_output(std::string_view output) {
    const std::string trimmed = trim(output);
    if (trimmed.empty()) {
        return {};
    }
    constexpr size_t kMaxChars = 240;
    if (trimmed.size() <= kMaxChars) {
        return trimmed;
    }
    return trimmed.substr(0, kMaxChars) + "...";
}

inline std::string lowercase(std::string_view text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (char ch : text) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return lowered;
}

inline bool parse_u64(std::string_view text, uint64_t& value) {
    const std::string trimmed = trim(text);
    if (trimmed.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(trimmed.c_str(), &end, 0);
    if (errno != 0 || end == nullptr || *end != '\0') {
        return false;
    }
    value = static_cast<uint64_t>(parsed);
    return true;
}

inline bool parse_privilege_mode(std::string_view text, PrivilegeMode& mode) {
    const std::string lowered = lowercase(trim(text));
    if (lowered == "machine" || lowered == "m" || lowered == "3") {
        mode = PrivilegeMode::Machine;
        return true;
    }
    if (lowered == "supervisor" || lowered == "s" || lowered == "1") {
        mode = PrivilegeMode::Supervisor;
        return true;
    }
    if (lowered == "user" || lowered == "u" || lowered == "0") {
        mode = PrivilegeMode::User;
        return true;
    }
    return false;
}

inline std::string temp_path_error(const char* action, const char* tag) {
    char buffer[256];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "failed to %s temp file for %s: %s",
                  action,
                  tag,
                  std::strerror(errno));
    return buffer;
}

inline bool create_temp_path(const char* tag, ScopedTempPath& temp_path, std::string& error) {
    char path_template[64];
    std::snprintf(path_template, sizeof(path_template), "/tmp/%s_XXXXXX", tag);
    const int fd = ::mkstemp(path_template);
    if (fd < 0) {
        error = temp_path_error("create", tag);
        return false;
    }
    ::close(fd);
    temp_path.path = path_template;
    temp_path.keep = std::getenv("SPIKE_DIFF_KEEP_TEMPS") != nullptr;
    return true;
}

inline bool write_text_file(const ScopedTempPath& path,
                            std::string_view contents,
                            const char* tag,
                            std::string& error) {
    std::ofstream file(path.path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = temp_path_error("open", tag);
        return false;
    }
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.close();
    if (!file) {
        error = temp_path_error("write", tag);
        return false;
    }
    return true;
}

inline void close_pipe_pair(int pipe_fds[2]) {
    if (pipe_fds[0] >= 0) {
        ::close(pipe_fds[0]);
        pipe_fds[0] = -1;
    }
    if (pipe_fds[1] >= 0) {
        ::close(pipe_fds[1]);
        pipe_fds[1] = -1;
    }
}

inline uint64_t mask_low_bytes(uint64_t value, int size) {
    if (size >= 8) {
        return value;
    }
    return value & ((UINT64_C(1) << (size * 8)) - 1);
}

inline uint64_t spike_tracked_csr_value(const FinalState& state, uint32_t csr) {
    const size_t index = tracked_csr_index(csr);
    if (index >= state.csrs.size()) {
        return 0;
    }
    return state.csrs[index];
}

inline uint64_t scenario_initial_csr_value(const Scenario& scenario, uint32_t csr) {
    const size_t index = tracked_csr_index(csr);
    if (index >= scenario.initial_csrs.size()) {
        return 0;
    }
    const uint64_t value = scenario.initial_csrs[index];
    if (value != 0) {
        return value;
    }
    if (csr == CSR_MTVEC && !scenario.trap_program.empty()) {
        return kTrapVector;
    }
    return 0;
}

inline std::string hex_u64(uint64_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
    return buffer;
}

inline bool fits_signed_bits(int64_t value, int bits) {
    const int64_t min_value = -(INT64_C(1) << (bits - 1));
    const int64_t max_value = (INT64_C(1) << (bits - 1)) - 1;
    return value >= min_value && value <= max_value;
}

inline uint32_t encode_addi(uint32_t rd, uint32_t rs1, int32_t imm12) {
    return ((static_cast<uint32_t>(imm12) & 0xfffU) << 20) | (rs1 << 15) | (rd << 7) | 0x13U;
}

inline uint32_t encode_ld(uint32_t rd, uint32_t rs1, int32_t imm12) {
    return ((static_cast<uint32_t>(imm12) & 0xfffU) << 20) | (rs1 << 15) | (0x3U << 12) |
           (rd << 7) | 0x03U;
}

inline uint32_t encode_auipc(uint32_t rd, int32_t imm20) {
    return ((static_cast<uint32_t>(imm20) & 0xfffffU) << 12) | (rd << 7) | 0x17U;
}

inline uint32_t encode_csrw(uint32_t csr, uint32_t rs1) {
    return (csr << 20) | (rs1 << 15) | (0x1U << 12) | 0x73U;
}

inline uint32_t encode_jal(uint32_t rd, int32_t offset) {
    const uint32_t imm = static_cast<uint32_t>(offset);
    const uint32_t bit20 = (imm >> 20) & 0x1U;
    const uint32_t bits10_1 = (imm >> 1) & 0x3ffU;
    const uint32_t bit11 = (imm >> 11) & 0x1U;
    const uint32_t bits19_12 = (imm >> 12) & 0xffU;
    return (bit20 << 31) | (bits19_12 << 12) | (bit11 << 20) | (bits10_1 << 21) |
           (rd << 7) | 0x6fU;
}

inline void write_u32_le(std::vector<uint8_t>& image, size_t offset, uint32_t value) {
    image[offset + 0] = static_cast<uint8_t>(value & 0xffU);
    image[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffU);
    image[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xffU);
    image[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xffU);
}

inline void write_u64_le(std::vector<uint8_t>& image, size_t offset, uint64_t value) {
    for (int byte = 0; byte < 8; ++byte) {
        image[offset + static_cast<size_t>(byte)] =
            static_cast<uint8_t>((value >> (byte * 8)) & 0xffU);
    }
}

struct BootstrapLiteralRef {
    size_t auipc_index{0};
    uint32_t rd{0};
    size_t literal_index{0};
};

struct BootstrapBuilder {
    std::vector<uint32_t> instructions{};
    std::vector<uint64_t> literals{};
    std::vector<BootstrapLiteralRef> literal_refs{};

    void emit(uint32_t instruction) {
        instructions.push_back(instruction);
    }

    void emit_load_literal(uint32_t rd, uint64_t value) {
        const size_t auipc_index = instructions.size();
        instructions.push_back(0);
        instructions.push_back(0);
        const size_t literal_index = literals.size();
        literals.push_back(value);
        literal_refs.push_back(BootstrapLiteralRef{auipc_index, rd, literal_index});
    }

    bool finalize(uint64_t boot_base, std::string& error) {
        if ((instructions.size() & 1U) != 0) {
            instructions.push_back(encode_addi(0, 0, 0));
        }
        const uint64_t literal_base = boot_base + static_cast<uint64_t>(instructions.size() * 4);
        for (const BootstrapLiteralRef& ref : literal_refs) {
            const uint64_t literal_addr =
                literal_base + static_cast<uint64_t>(ref.literal_index * sizeof(uint64_t));
            const uint64_t pc = boot_base + static_cast<uint64_t>(ref.auipc_index * 4);
            const int64_t rel = static_cast<int64_t>(literal_addr) - static_cast<int64_t>(pc);
            const int64_t hi20 = (rel + 0x800) >> 12;
            const int64_t lo12 = rel - (hi20 << 12);
            if (!fits_signed_bits(hi20, 20) || !fits_signed_bits(lo12, 12)) {
                error = "bootstrap literal pool out of AUIPC/LD reach";
                return false;
            }
            instructions[ref.auipc_index] = encode_auipc(ref.rd, static_cast<int32_t>(hi20));
            instructions[ref.auipc_index + 1] = encode_ld(ref.rd, ref.rd, static_cast<int32_t>(lo12));
        }
        return true;
    }

    uint64_t end_address(uint64_t boot_base) const {
        return boot_base + static_cast<uint64_t>(instructions.size() * 4) +
               static_cast<uint64_t>(literals.size() * sizeof(uint64_t));
    }

    void append_to_image(std::vector<uint8_t>& image, uint64_t image_base, uint64_t boot_base) const {
        const size_t instruction_offset = static_cast<size_t>(boot_base - image_base);
        for (size_t i = 0; i < instructions.size(); ++i) {
            write_u32_le(image, instruction_offset + i * 4, instructions[i]);
        }
        const size_t literal_offset = instruction_offset + instructions.size() * 4;
        for (size_t i = 0; i < literals.size(); ++i) {
            write_u64_le(image,
                         literal_offset + i * sizeof(uint64_t),
                         literals[i]);
        }
    }
};

inline bool scenario_uses_only_supported_watches(const Scenario& scenario, std::string& reason) {
    for (const MemoryWatch& watch : scenario.watches) {
        if (watch.size != 1 && watch.size != 2 && watch.size != 4 && watch.size != 8) {
            reason = "spike differential v1 only supports watched memory sizes of 1, 2, 4, or 8 bytes";
            return false;
        }
        if ((watch.addr % static_cast<uint64_t>(watch.size)) != 0) {
            reason = "spike differential v1 requires watched memory addresses to be naturally aligned";
            return false;
        }
    }
    return true;
}

inline bool scenario_requires_spike_unsupported_setup(const Scenario& scenario,
                                                      std::string& reason) {
    if (scenario.fixture != Scenario::PlatformFixture::None) {
        reason = "platform fixtures are not supported by spike differential v1";
        return true;
    }
    if (static_cast<bool>(scenario.configure)) {
        reason = "configure hooks are not supported by spike differential v1";
        return true;
    }
    if (!scenario_uses_only_supported_watches(scenario, reason)) {
        return true;
    }
    if (scenario_initial_csr_value(scenario, CSR_MISA) != 0) {
        reason = "initial misa writes are not supported by spike differential v1";
        return true;
    }
    return false;
}

inline bool build_spike_scenario_plan(const Scenario& scenario,
                                      SpikeScenarioPlan& plan,
                                      std::string& error) {
    const auto ends_with_controlled_exit = [](const std::vector<uint32_t>& program) {
        return program.size() >= 2 && program[program.size() - 2] == kAddiA7Exit &&
               program.back() == kEcall;
    };
    if (scenario.program.empty()) {
        error = "scenario program is empty";
        return false;
    }
    if (ends_with_controlled_exit(scenario.program)) {
        plan.capture_mode = SpikeCaptureMode::ControlledExit;
        plan.capture_region = SpikeCaptureRegion::Program;
        plan.capture_pc = kEntry + static_cast<uint64_t>(scenario.program.size() * 4);
        plan.final_pc = plan.capture_pc;
        plan.halted = true;
        plan.timed_out = false;
        plan.exit_reason = "controlled_exit";
        return true;
    }
    if (ends_with_controlled_exit(scenario.trap_program)) {
        plan.capture_mode = SpikeCaptureMode::ControlledExit;
        plan.capture_region = SpikeCaptureRegion::TrapProgram;
        plan.capture_pc = kTrapVector + static_cast<uint64_t>(scenario.trap_program.size() * 4);
        plan.final_pc = plan.capture_pc;
        plan.halted = true;
        plan.timed_out = false;
        plan.exit_reason = "controlled_exit";
        return true;
    }
    plan.capture_mode = SpikeCaptureMode::StepBudget;
    plan.capture_region = SpikeCaptureRegion::Program;
    plan.capture_pc = 0;
    plan.final_pc = 0;
    plan.halted = false;
    plan.timed_out = true;
    plan.exit_reason = "step_budget_exhausted";
    return true;
}

inline void emit_csr_write(BootstrapBuilder& builder, uint32_t csr, uint64_t value) {
    builder.emit_load_literal(kScratchReg, value);
    builder.emit(encode_csrw(csr, kScratchReg));
}

inline bool build_bootstrap(const Scenario& scenario,
                            BootstrapBuilder& builder,
                            std::string& error) {
    static constexpr std::array<uint32_t, 13> kBootstrapCsrOrder{
        CSR_MEDELEG,
        CSR_MIDELEG,
        CSR_MTVEC,
        CSR_STVEC,
        CSR_MCOUNTEREN,
        CSR_SCOUNTEREN,
        CSR_MSCRATCH,
        CSR_SSCRATCH,
        CSR_SATP,
        CSR_MIE,
        CSR_SIE,
        CSR_MCAUSE,
        CSR_MTVAL,
    };

    uint64_t requested_mstatus = scenario_initial_csr_value(scenario, CSR_MSTATUS);
    const uint64_t requested_sstatus = scenario_initial_csr_value(scenario, CSR_SSTATUS);
    if (requested_sstatus != 0) {
        requested_mstatus = (requested_mstatus & ~kSstatusMask) | (requested_sstatus & kSstatusMask);
    }
    if (scenario.initial_privilege != PrivilegeMode::Machine) {
        requested_mstatus &= ~MSTATUS_MPP_MASK;
        if (scenario.initial_privilege == PrivilegeMode::Supervisor) {
            requested_mstatus |= (1ULL << MSTATUS_MPP_SHIFT);
        }
    }

    if (scenario.initial_privilege != PrivilegeMode::Machine &&
        scenario_initial_csr_value(scenario, CSR_MEPC) != 0 &&
        scenario_initial_csr_value(scenario, CSR_MEPC) != kEntry) {
        error = "initial mepc conflicts with non-machine bootstrap entry";
        return false;
    }

    for (uint32_t reg = 1; reg < kScratchReg; ++reg) {
        const uint64_t value = scenario.initial_gprs[reg];
        if (value == 0) {
            builder.emit(encode_addi(reg, 0, 0));
            continue;
        }
        builder.emit_load_literal(reg, value);
    }

    for (uint32_t csr : kBootstrapCsrOrder) {
        const uint64_t value = scenario_initial_csr_value(scenario, csr);
        if (value != 0) {
            emit_csr_write(builder, csr, value);
        }
    }

    if (scenario.initial_privilege != PrivilegeMode::Machine) {
        emit_csr_write(builder, CSR_MEPC, kEntry);
    } else if (scenario_initial_csr_value(scenario, CSR_MEPC) != 0) {
        emit_csr_write(builder, CSR_MEPC, scenario_initial_csr_value(scenario, CSR_MEPC));
    }

    if (requested_mstatus != 0 || scenario.initial_privilege != PrivilegeMode::Machine) {
        emit_csr_write(builder, CSR_MSTATUS, requested_mstatus);
    }

    if (scenario.initial_gprs[kScratchReg] == 0) {
        builder.emit(encode_addi(kScratchReg, 0, 0));
    } else {
        builder.emit_load_literal(kScratchReg, scenario.initial_gprs[kScratchReg]);
    }

    if (scenario.initial_privilege == PrivilegeMode::Machine) {
        const uint64_t pc = kSpikeBootBase + static_cast<uint64_t>(builder.instructions.size() * 4);
        const int64_t rel = static_cast<int64_t>(kEntry) - static_cast<int64_t>(pc);
        if (!fits_signed_bits(rel, 21) || (rel & 1) != 0) {
            error = "bootstrap jump to program is out of JAL reach";
            return false;
        }
        builder.emit(encode_jal(0, static_cast<int32_t>(rel)));
    } else {
        builder.emit(kMret);
    }

    return builder.finalize(kSpikeBootBase, error);
}

inline bool materialize_spike_image(const Scenario& scenario,
                                    const SpikeScenarioPlan& plan,
                                    std::vector<uint8_t>& image,
                                    uint64_t& memory_size,
                                    std::string& error) {
    BootstrapBuilder bootstrap;
    if (!build_bootstrap(scenario, bootstrap, error)) {
        return false;
    }

    uint64_t image_end = kEntry + static_cast<uint64_t>(scenario.program.size() * 4);
    if (plan.capture_mode == SpikeCaptureMode::ControlledExit) {
        image_end += 4;
    }
    if (!scenario.trap_program.empty()) {
        const uint64_t trap_end = kTrapVector + static_cast<uint64_t>(scenario.trap_program.size() * 4);
        if (trap_end > image_end) {
            image_end = trap_end;
        }
    }
    for (const MemoryInit& init : scenario.initial_memory) {
        if (init.size <= 0 || init.size > 8) {
            error = "unsupported initial_memory size in spike image materialization";
            return false;
        }
        if (init.addr < kEntry) {
            error = "initial_memory below kEntry is not supported by spike differential v1";
            return false;
        }
        const uint64_t end = init.addr + static_cast<uint64_t>(init.size);
        if (end > image_end) {
            image_end = end;
        }
    }
    const uint64_t bootstrap_end = bootstrap.end_address(kSpikeBootBase);
    if (bootstrap_end > image_end) {
        image_end = bootstrap_end;
    }
    image_end = align_up(image_end, sizeof(uint64_t)) + kSpikeHostCommBytes;
    if (image_end <= kEntry) {
        error = "scenario program is empty";
        return false;
    }

    image.assign(static_cast<size_t>(image_end - kEntry), 0);
    for (size_t i = 0; i < scenario.program.size(); ++i) {
        const uint64_t addr = kEntry + static_cast<uint64_t>(i * 4);
        uint32_t word = scenario.program[i];
        if (plan.capture_mode == SpikeCaptureMode::ControlledExit &&
            plan.capture_region == SpikeCaptureRegion::Program && i + 1 == scenario.program.size()) {
            word = 0x0040006fU;
        }
        write_u32_le(image, static_cast<size_t>(addr - kEntry), word);
    }
    if (plan.capture_mode == SpikeCaptureMode::ControlledExit) {
        write_u32_le(image, static_cast<size_t>(plan.capture_pc - kEntry), kJalX0Zero);
    }
    for (size_t i = 0; i < scenario.trap_program.size(); ++i) {
        const uint64_t addr = kTrapVector + static_cast<uint64_t>(i * 4);
        uint32_t word = scenario.trap_program[i];
        if (plan.capture_mode == SpikeCaptureMode::ControlledExit &&
            plan.capture_region == SpikeCaptureRegion::TrapProgram &&
            i + 1 == scenario.trap_program.size()) {
            word = 0x0040006fU;
        }
        write_u32_le(image, static_cast<size_t>(addr - kEntry), word);
    }
    for (const MemoryInit& init : scenario.initial_memory) {
        const size_t offset = static_cast<size_t>(init.addr - kEntry);
        for (int byte = 0; byte < init.size; ++byte) {
            image[offset + static_cast<size_t>(byte)] =
                static_cast<uint8_t>((init.value >> (8 * byte)) & 0xffU);
        }
    }
    bootstrap.append_to_image(image, kEntry, kSpikeBootBase);
    memory_size = align_up(static_cast<uint64_t>(image.size()), kSpikeImageAlign);
    return true;
}

inline bool write_spike_elf(const ScopedTempPath& path,
                            const std::vector<uint8_t>& image,
                            std::string& error) {
    constexpr uint64_t kSegmentOffset = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    constexpr uint32_t kSectionTypeSymtab = 2;
    constexpr uint32_t kSectionTypeStrtab = 3;
    constexpr uint8_t kSymbolBindGlobal = 1;
    constexpr uint8_t kSymbolTypeObject = 1;
    static constexpr char kSectionNameTable[] = "\0.strtab\0.symtab\0.shstrtab\0";
    static constexpr char kSymbolNameTable[] = "\0tohost\0fromhost\0";
    constexpr uint16_t kSectionCount = 4;
    constexpr uint16_t kStrtabSectionIndex = 1;
    constexpr uint16_t kSectionNameIndex = 3;
    const uint64_t symbol_name_offset = kSegmentOffset + image.size();
    const uint64_t symtab_offset = symbol_name_offset + sizeof(kSymbolNameTable) - 1;
    const uint64_t shstrtab_offset = symtab_offset + sizeof(Elf64_Sym) * 3;
    const uint64_t shoff = shstrtab_offset + sizeof(kSectionNameTable) - 1;
    const uint64_t tohost_addr = kEntry + image.size() - kSpikeHostCommBytes;
    const uint64_t fromhost_addr = tohost_addr + sizeof(uint64_t);

    Elf64_Ehdr ehdr{};
    ehdr.e_ident[EI_MAG0] = ELFMAG0;
    ehdr.e_ident[EI_MAG1] = ELFMAG1;
    ehdr.e_ident[EI_MAG2] = ELFMAG2;
    ehdr.e_ident[EI_MAG3] = ELFMAG3;
    ehdr.e_ident[EI_CLASS] = ELFCLASS64;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI] = ELFOSABI_NONE;
    ehdr.e_type = ET_EXEC;
    ehdr.e_machine = EM_RISCV;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = kSpikeBootBase;
    ehdr.e_phoff = sizeof(Elf64_Ehdr);
    ehdr.e_shoff = shoff;
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = 1;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = kSectionCount;
    ehdr.e_shstrndx = kSectionNameIndex;

    Elf64_Phdr phdr{};
    phdr.p_type = PT_LOAD;
    phdr.p_flags = PF_R | PF_W | PF_X;
    phdr.p_offset = kSegmentOffset;
    phdr.p_vaddr = kEntry;
    phdr.p_paddr = kEntry;
    phdr.p_filesz = image.size();
    phdr.p_memsz = image.size();
    phdr.p_align = kSpikeImageAlign;

    Elf64_Shdr null_shdr{};

    Elf64_Shdr strtab_shdr{};
    strtab_shdr.sh_name = 1;
    strtab_shdr.sh_type = kSectionTypeStrtab;
    strtab_shdr.sh_offset = symbol_name_offset;
    strtab_shdr.sh_size = sizeof(kSymbolNameTable) - 1;
    strtab_shdr.sh_addralign = 1;

    Elf64_Shdr symtab_shdr{};
    symtab_shdr.sh_name = 9;
    symtab_shdr.sh_type = kSectionTypeSymtab;
    symtab_shdr.sh_offset = symtab_offset;
    symtab_shdr.sh_size = sizeof(Elf64_Sym) * 3;
    symtab_shdr.sh_link = kStrtabSectionIndex;
    symtab_shdr.sh_info = 1;
    symtab_shdr.sh_addralign = alignof(Elf64_Sym);
    symtab_shdr.sh_entsize = sizeof(Elf64_Sym);

    Elf64_Shdr shstrtab_shdr{};
    shstrtab_shdr.sh_name = 17;
    shstrtab_shdr.sh_type = kSectionTypeStrtab;
    shstrtab_shdr.sh_offset = shstrtab_offset;
    shstrtab_shdr.sh_size = sizeof(kSectionNameTable) - 1;
    shstrtab_shdr.sh_addralign = 1;

    Elf64_Sym null_sym{};

    Elf64_Sym tohost_sym{};
    tohost_sym.st_name = 1;
    tohost_sym.st_info = static_cast<unsigned char>((kSymbolBindGlobal << 4) | kSymbolTypeObject);
    tohost_sym.st_shndx = SHN_ABS;
    tohost_sym.st_value = tohost_addr;
    tohost_sym.st_size = sizeof(uint64_t);

    Elf64_Sym fromhost_sym{};
    fromhost_sym.st_name = 8;
    fromhost_sym.st_info = static_cast<unsigned char>((kSymbolBindGlobal << 4) | kSymbolTypeObject);
    fromhost_sym.st_shndx = SHN_ABS;
    fromhost_sym.st_value = fromhost_addr;
    fromhost_sym.st_size = sizeof(uint64_t);

    std::ofstream file(path.path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = temp_path_error("open", "spike_elf");
        return false;
    }
    file.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
    file.write(reinterpret_cast<const char*>(&phdr), sizeof(phdr));
    file.write(reinterpret_cast<const char*>(image.data()),
               static_cast<std::streamsize>(image.size()));
    file.write(kSymbolNameTable, sizeof(kSymbolNameTable) - 1);
    file.write(reinterpret_cast<const char*>(&null_sym), sizeof(null_sym));
    file.write(reinterpret_cast<const char*>(&tohost_sym), sizeof(tohost_sym));
    file.write(reinterpret_cast<const char*>(&fromhost_sym), sizeof(fromhost_sym));
    file.write(kSectionNameTable, sizeof(kSectionNameTable) - 1);
    file.write(reinterpret_cast<const char*>(&null_shdr), sizeof(null_shdr));
    file.write(reinterpret_cast<const char*>(&strtab_shdr), sizeof(strtab_shdr));
    file.write(reinterpret_cast<const char*>(&symtab_shdr), sizeof(symtab_shdr));
    file.write(reinterpret_cast<const char*>(&shstrtab_shdr), sizeof(shstrtab_shdr));
    file.close();
    if (!file) {
        error = temp_path_error("write", "spike_elf");
        return false;
    }
    return true;
}

inline std::string build_spike_debug_script(const Scenario& scenario,
                                            const SpikeScenarioPlan& plan) {
    std::ostringstream script;
    if (plan.capture_mode == SpikeCaptureMode::ControlledExit) {
        script << "until pc 0 " << hex_u64(plan.capture_pc) << '\n';
    } else {
        script << "rs " << scenario.max_steps << '\n';
    }
    script << "pc 0\n";
    script << "priv 0\n";
    script << "reg 0 instret\n";
    for (size_t i = 0; i < 32; ++i) {
        script << "reg 0 " << i << '\n';
    }
    for (uint32_t csr : kTrackedCsrs) {
        script << "reg 0 " << csr_name(csr) << '\n';
    }
    for (const MemoryWatch& watch : scenario.watches) {
        script << "mem " << hex_u64(watch.addr) << '\n';
    }
    script << "quit\n";
    return script.str();
}

inline bool set_close_on_exec(int fd) {
    const int flags = ::fcntl(fd, F_GETFD);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
}

inline bool set_non_blocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

inline SpikeProcessResult run_spike_process(const SpikeRunnerOptions& options,
                                            uint64_t memory_size,
                                            const std::string& debug_script_path,
                                            const std::string& elf_path) {
    SpikeProcessResult result;
    int output_pipe[2] = {-1, -1};
    int exec_pipe[2] = {-1, -1};
    if (::pipe(output_pipe) != 0) {
        result.exec_errno = errno;
        return result;
    }
    if (::pipe(exec_pipe) != 0) {
        result.exec_errno = errno;
        close_pipe_pair(output_pipe);
        return result;
    }
    if (!set_close_on_exec(exec_pipe[1])) {
        result.exec_errno = errno;
        close_pipe_pair(output_pipe);
        close_pipe_pair(exec_pipe);
        return result;
    }
    if (!set_non_blocking(output_pipe[0])) {
        result.exec_errno = errno;
        close_pipe_pair(output_pipe);
        close_pipe_pair(exec_pipe);
        return result;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        result.exec_errno = errno;
        close_pipe_pair(output_pipe);
        close_pipe_pair(exec_pipe);
        return result;
    }

    if (pid == 0) {
        ::close(output_pipe[0]);
        ::close(exec_pipe[0]);
        ::dup2(output_pipe[1], STDOUT_FILENO);
        ::dup2(output_pipe[1], STDERR_FILENO);
        ::close(output_pipe[1]);

        std::vector<std::string> argv_storage;
        argv_storage.push_back(options.spike_path);
        argv_storage.push_back("--isa=rv64im_zicsr_zicntr");
        argv_storage.push_back("--pc=0x80001000");
        char memory_arg[64];
        std::snprintf(memory_arg,
                      sizeof(memory_arg),
                      "-m0x%llx:0x%llx",
                      static_cast<unsigned long long>(kEntry),
                      static_cast<unsigned long long>(memory_size));
        argv_storage.emplace_back(memory_arg);
        argv_storage.push_back("-d");
        argv_storage.push_back("--debug-cmd=" + debug_script_path);
        for (const std::string& arg : options.extra_args) {
            argv_storage.push_back(arg);
        }
        argv_storage.push_back(elf_path);

        std::vector<char*> argv;
        argv.reserve(argv_storage.size() + 1);
        for (std::string& arg : argv_storage) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);
        ::execvp(options.spike_path.c_str(), argv.data());

        const int exec_errno = errno;
        const ssize_t ignored = ::write(exec_pipe[1], &exec_errno, sizeof(exec_errno));
        (void)ignored;
        _exit(127);
    }

    ::close(output_pipe[1]);
    output_pipe[1] = -1;
    ::close(exec_pipe[1]);
    exec_pipe[1] = -1;

    const ssize_t exec_read = ::read(exec_pipe[0], &result.exec_errno, sizeof(result.exec_errno));
    ::close(exec_pipe[0]);
    exec_pipe[0] = -1;
    if (exec_read > 0) {
        int status = 0;
        (void)::waitpid(pid, &status, 0);
        close_pipe_pair(output_pipe);
        return result;
    }

    result.launched = true;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(options.timeout_ms);
    std::string output;
    bool child_exited = false;
    int status = 0;
    char buffer[4096];

    while (true) {
        while (true) {
            const ssize_t read_size = ::read(output_pipe[0], buffer, sizeof(buffer));
            if (read_size > 0) {
                output.append(buffer, static_cast<size_t>(read_size));
                continue;
            }
            if (read_size == 0) {
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            break;
        }

        const pid_t waited = ::waitpid(pid, &status, WNOHANG);
        if (waited == pid) {
            child_exited = true;
            break;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            result.timed_out = true;
            ::kill(pid, SIGKILL);
            (void)::waitpid(pid, &status, 0);
            child_exited = true;
            break;
        }

        struct pollfd poll_fd {
            output_pipe[0], POLLIN | POLLHUP, 0
        };
        (void)::poll(&poll_fd, 1, 25);
    }

    while (true) {
        const ssize_t read_size = ::read(output_pipe[0], buffer, sizeof(buffer));
        if (read_size > 0) {
            output.append(buffer, static_cast<size_t>(read_size));
            continue;
        }
        break;
    }
    close_pipe_pair(output_pipe);

    result.output = std::move(output);
    if (child_exited) {
        if (WIFEXITED(status)) {
            result.exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            result.term_signal = WTERMSIG(status);
        }
    }
    return result;
}

inline const char* spike_error_kind_name(SpikeErrorKind kind) {
    switch (kind) {
    case SpikeErrorKind::None:
        return "none";
    case SpikeErrorKind::MissingSpike:
        return "missing_spike";
    case SpikeErrorKind::LaunchFailure:
        return "launch_failure";
    case SpikeErrorKind::ParseFailure:
        return "parse_failure";
    case SpikeErrorKind::Timeout:
        return "timeout";
    case SpikeErrorKind::UnsupportedScenario:
        return "unsupported_scenario";
    }
    return "unknown";
}

inline bool starts_with(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

inline std::string_view strip_debug_prompt(std::string_view line) {
    while (!line.empty()) {
        const char ch = line.front();
        if (ch == ':' || ch == '>' || std::isspace(static_cast<unsigned char>(ch)) != 0) {
            line.remove_prefix(1);
            continue;
        }
        break;
    }
    return line;
}

inline bool line_is_debug_command_echo(std::string_view line) {
    line = strip_debug_prompt(line);
    if (line.empty()) {
        return true;
    }
    return starts_with(line, "until ") || starts_with(line, "untiln ") || starts_with(line, "reg ") ||
           starts_with(line, "pc ") || starts_with(line, "mem ") || starts_with(line, "rs ") ||
           starts_with(line, "run ") || starts_with(line, "priv ") || line == "quit" || line == "q";
}

inline bool extract_hex_token(std::string_view line, uint64_t& value) {
    for (size_t i = 0; i + 2 <= line.size(); ++i) {
        if (line[i] != '0' || (line[i + 1] != 'x' && line[i + 1] != 'X')) {
            continue;
        }
        size_t end = i + 2;
        while (end < line.size() && std::isxdigit(static_cast<unsigned char>(line[end])) != 0) {
            ++end;
        }
        if (end == i + 2) {
            continue;
        }
        return parse_u64(line.substr(i, end - i), value);
    }
    return false;
}

inline bool extract_spike_output(std::string_view output,
                                 ParsedSpikeOutput& parsed,
                                 std::string& error) {
    size_t line_start = 0;
    while (line_start < output.size()) {
        size_t line_end = output.find('\n', line_start);
        if (line_end == std::string_view::npos) {
            line_end = output.size();
        }
        const std::string raw_line = trim(output.substr(line_start, line_end - line_start));
        line_start = line_end + 1;
        if (raw_line.empty() || line_is_debug_command_echo(raw_line)) {
            continue;
        }

        PrivilegeMode privilege = PrivilegeMode::Machine;
        if (parse_privilege_mode(raw_line, privilege)) {
            if (parsed.has_privilege) {
                error = "duplicate privilege line in spike debug output";
                return false;
            }
            parsed.has_privilege = true;
            parsed.privilege = privilege;
            continue;
        }

        uint64_t numeric_value = 0;
        if (extract_hex_token(raw_line, numeric_value) || parse_u64(raw_line, numeric_value)) {
            parsed.numeric_values.push_back(numeric_value);
            continue;
        }

        error = "unexpected spike debug output line: " + raw_line;
        return false;
    }

    if (!parsed.has_privilege) {
        error = "missing privilege line in spike debug output";
        return false;
    }
    if (parsed.numeric_values.empty()) {
        error = "no numeric spike debug output captured";
        return false;
    }
    return true;
}

inline bool trap_state_present(const FinalState& state,
                               uint32_t cause_csr,
                               uint32_t epc_csr,
                               uint32_t tval_csr) {
    return spike_tracked_csr_value(state, cause_csr) != 0 ||
           spike_tracked_csr_value(state, epc_csr) != 0 ||
           spike_tracked_csr_value(state, tval_csr) != 0;
}

inline bool infer_supervisor_trap_summary(FinalState& state) {
    if (!trap_state_present(state, CSR_SCAUSE, CSR_SEPC, CSR_STVAL)) {
        return false;
    }
    state.trap_summary = {
        true,
        spike_tracked_csr_value(state, CSR_SCAUSE),
        spike_tracked_csr_value(state, CSR_STVAL),
        spike_tracked_csr_value(state, CSR_SEPC),
        PrivilegeMode::Supervisor,
    };
    return true;
}

inline bool infer_machine_trap_summary(FinalState& state) {
    if (!trap_state_present(state, CSR_MCAUSE, CSR_MEPC, CSR_MTVAL)) {
        return false;
    }
    state.trap_summary = {
        true,
        spike_tracked_csr_value(state, CSR_MCAUSE),
        spike_tracked_csr_value(state, CSR_MTVAL),
        spike_tracked_csr_value(state, CSR_MEPC),
        PrivilegeMode::Machine,
    };
    return true;
}

inline void infer_spike_trap_summary(FinalState& state) {
    const bool has_supervisor_trap = trap_state_present(state, CSR_SCAUSE, CSR_SEPC, CSR_STVAL);
    const bool has_machine_trap = trap_state_present(state, CSR_MCAUSE, CSR_MEPC, CSR_MTVAL);
    state.trap_summary = TrapSummary{};
    if (has_supervisor_trap == has_machine_trap) {
        return;
    }
    if (has_supervisor_trap) {
        (void)infer_supervisor_trap_summary(state);
        return;
    }
    (void)infer_machine_trap_summary(state);
}

inline bool parse_spike_final_state_output(const Scenario& scenario,
                                           const SpikeScenarioPlan& plan,
                                           const std::string& output,
                                           FinalState& state,
                                           std::string& error) {
    ParsedSpikeOutput parsed;
    if (!extract_spike_output(output, parsed, error)) {
        return false;
    }

    const size_t expected_value_count = 2 + state.gprs.size() + kTrackedCsrs.size() +
                                        scenario.watches.size();
    if (parsed.numeric_values.size() != expected_value_count) {
        std::ostringstream message;
        message << "unexpected field count: expected " << expected_value_count
                << " captured values, got " << parsed.numeric_values.size();
        error = message.str();
        return false;
    }

    size_t index = 0;
    state.pc = parsed.numeric_values[index++];
    state.privilege = parsed.privilege;
    state.instret = parsed.numeric_values[index++];
    state.halted = plan.halted;
    state.timed_out = plan.timed_out;
    state.exit_reason = plan.exit_reason;
    state.watched_memory.resize(scenario.watches.size(), 0);

    for (size_t i = 0; i < state.gprs.size(); ++i) {
        state.gprs[i] = parsed.numeric_values[index++];
    }
    for (size_t i = 0; i < kTrackedCsrs.size(); ++i) {
        state.csrs[i] = parsed.numeric_values[index++];
    }
    for (size_t i = 0; i < scenario.watches.size(); ++i) {
        state.watched_memory[i] =
            mask_low_bytes(parsed.numeric_values[index++], scenario.watches[i].size);
    }
    infer_spike_trap_summary(state);
    return true;
}

inline SpikeRunResult run_spike_final_state(const Scenario& scenario,
                                            const SpikeRunnerOptions& options = {}) {
    SpikeRunResult result;

    std::string unsupported_reason;
    if (scenario_requires_spike_unsupported_setup(scenario, unsupported_reason)) {
        result.error_kind = SpikeErrorKind::UnsupportedScenario;
        result.message = "[" + std::string(scenario.name ? scenario.name : "scenario") +
                         "] unsupported spike scenario: " + unsupported_reason;
        return result;
    }

    SpikeScenarioPlan plan;
    if (!build_spike_scenario_plan(scenario, plan, result.message)) {
        result.error_kind = SpikeErrorKind::LaunchFailure;
        result.message = "[" + std::string(scenario.name ? scenario.name : "scenario") +
                         "] failed to build spike scenario plan: " + result.message;
        return result;
    }

    std::vector<uint8_t> image;
    uint64_t memory_size = 0;
    if (!materialize_spike_image(scenario, plan, image, memory_size, result.message)) {
        result.error_kind = SpikeErrorKind::LaunchFailure;
        result.message = "[" + std::string(scenario.name ? scenario.name : "scenario") +
                         "] failed to materialize spike image: " + result.message;
        return result;
    }

    ScopedTempPath elf_path;
    if (!create_temp_path("spike_diff_elf", elf_path, result.message)) {
        result.error_kind = SpikeErrorKind::LaunchFailure;
        result.message = "[" + std::string(scenario.name ? scenario.name : "scenario") +
                         "] " + result.message;
        return result;
    }
    if (!write_spike_elf(elf_path, image, result.message)) {
        result.error_kind = SpikeErrorKind::LaunchFailure;
        result.message = "[" + std::string(scenario.name ? scenario.name : "scenario") +
                         "] failed to write spike elf: " + result.message;
        return result;
    }

    ScopedTempPath debug_script_path;
    if (!create_temp_path("spike_diff_cmd", debug_script_path, result.message)) {
        result.error_kind = SpikeErrorKind::LaunchFailure;
        result.message = "[" + std::string(scenario.name ? scenario.name : "scenario") +
                         "] " + result.message;
        return result;
    }
    if (!write_text_file(debug_script_path,
                         build_spike_debug_script(scenario, plan),
                         "spike_debug_script",
                         result.message)) {
        result.error_kind = SpikeErrorKind::LaunchFailure;
        result.message = "[" + std::string(scenario.name ? scenario.name : "scenario") +
                         "] failed to write spike debug script: " + result.message;
        return result;
    }

    const SpikeProcessResult process =
        run_spike_process(options, memory_size, debug_script_path.path, elf_path.path);
    result.raw_output = process.output;
    if (process.exec_errno != 0) {
        result.error_kind = process.exec_errno == ENOENT ? SpikeErrorKind::MissingSpike
                                                         : SpikeErrorKind::LaunchFailure;
        char buffer[256];
        std::snprintf(buffer,
                      sizeof(buffer),
                      "[%s] failed to launch spike executable '%s': %s",
                      scenario.name ? scenario.name : "scenario",
                      options.spike_path.c_str(),
                      std::strerror(process.exec_errno));
        result.message = buffer;
        return result;
    }
    if (!process.launched) {
        result.error_kind = SpikeErrorKind::LaunchFailure;
        result.message = "[" + std::string(scenario.name ? scenario.name : "scenario") +
                         "] failed to launch spike process";
        return result;
    }
    if (process.timed_out) {
        const std::string summarized_output = summarize_spike_output(process.output);
        result.error_kind = SpikeErrorKind::Timeout;
        result.message = "[" + std::string(scenario.name ? scenario.name : "scenario") +
                         "] spike timed out";
        if (!summarized_output.empty()) {
            result.message += " | raw_output=" + summarized_output;
        }
        return result;
    }
    if (process.exit_status != 0 || process.term_signal != 0) {
        std::ostringstream message;
        message << '[' << (scenario.name ? scenario.name : "scenario")
                << "] spike exited unexpectedly";
        if (process.exit_status != 0) {
            message << " with status " << process.exit_status;
        }
        if (process.term_signal != 0) {
            message << " from signal " << process.term_signal;
        }
        const std::string summarized_output = summarize_spike_output(process.output);
        if (!summarized_output.empty()) {
            message << ": " << summarized_output;
        }
        result.error_kind = SpikeErrorKind::LaunchFailure;
        result.message = message.str();
        return result;
    }

    std::string parse_error;
    if (!parse_spike_final_state_output(scenario, plan, process.output, result.final_state, parse_error)) {
        const std::string summarized_output = summarize_spike_output(process.output);
        result.error_kind = SpikeErrorKind::ParseFailure;
        result.message = "[" + std::string(scenario.name ? scenario.name : "scenario") +
                         "] failed to parse spike output: " + parse_error;
        if (!summarized_output.empty()) {
            result.message += " | raw_output=" + summarized_output;
        }
        return result;
    }

    result.ok = true;
    result.error_kind = SpikeErrorKind::None;
    return result;
}

}  // namespace
}  // namespace spike_differential
