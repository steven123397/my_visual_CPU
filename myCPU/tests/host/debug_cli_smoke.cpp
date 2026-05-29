#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "../../src/cpu.h"
#define private public
#include "../../src/debug/debug_session.h"
#undef private
#include "../../src/debug/debug_protocol.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kDebugProgramAddr = 0x80000078ULL;

constexpr std::array<uint32_t, 49> kSupervisorExternalProgram = {
    0x10000437U, 0x0c0004b7U, 0x00000297U, 0x09428293U, 0x10529073U, 0x00000297U, 0x07c28293U,
    0x30529073U, 0x00100293U, 0x0254a423U, 0x40000293U, 0x00002337U, 0x0803031bU, 0x00648333U,
    0x00532023U, 0x00201337U, 0x00648333U, 0x00032023U, 0x20000293U, 0x30329073U, 0x10429073U,
    0x000012b7U, 0x8022829bU, 0x30029073U, 0x00200293U, 0x005400a3U, 0x00000297U, 0x01028293U,
    0x34129073U, 0x30200073U, 0x0000006fU, 0x05000293U, 0x00540023U, 0x04500293U, 0x00540023U,
    0x00100073U, 0x05d00893U, 0x00000513U, 0x00000073U, 0x00201337U, 0x0043031bU, 0x00648333U,
    0x00032383U, 0x000400a3U, 0x00732023U, 0x00000297U, 0xfc828293U, 0x14129073U, 0x10200073U,
};

constexpr std::array<uint32_t, 43> kSupervisorTimerProgram = {
    0x10000437U, 0x020004b7U, 0x00000297U, 0x07c28293U, 0x10529073U, 0x00000297U, 0x06428293U,
    0x30529073U, 0x0000c2b7U, 0xff82829bU, 0x005482b3U, 0x0002b303U, 0x00430313U, 0x000043b7U,
    0x007483b3U, 0x0063b023U, 0x02000293U, 0x30329073U, 0x10429073U, 0x000012b7U, 0x8022829bU,
    0x30029073U, 0x00000297U, 0x01028293U, 0x34129073U, 0x30200073U, 0x0000006fU, 0x05400293U,
    0x00540023U, 0x00100073U, 0x05d00893U, 0x00000513U, 0x00000073U, 0xfff00293U, 0x00004337U,
    0x00648333U, 0x00533023U, 0x02000293U, 0x1442b073U, 0x00000297U, 0xfd028293U, 0x14129073U,
    0x10200073U,
};

constexpr std::array<uint32_t, 4> kPredictorProgram = {
    0x0080006fU,
    0x06300093U,
    0x05d00893U,
    0x00000073U,
};

constexpr std::array<uint32_t, 4> kMmioFaultProgram = {
    0x10000437U,
    0x04100493U,
    0x00942023U,
    0x00000073U,
};

constexpr std::array<uint32_t, 5> kStoreQueueProgram = {
    0x07a00293U,
    0x80001537U,
    0x005500a3U,
    0x05d00893U,
    0x00000073U,
};

constexpr std::array<uint32_t, 3> kJitDispatchLoopProgram = {
    0x00108093U,
    0x00208113U,
    0xfe000ce3U,
};

bool expect_contains(const std::string& haystack, const char* needle, const char* message) {
    if (haystack.find(needle) == std::string::npos) {
        std::fprintf(stderr, "%s\n", message);
        std::fprintf(stderr, "output was:\n%s\n", haystack.c_str());
        return false;
    }
    return true;
}

template <size_t N>
std::string write_temp_binary(const char* tag, const std::array<uint32_t, N>& words) {
    char path[] = "/tmp/debug_cli_smoke_XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0) {
        std::fprintf(stderr, "failed to create temp binary for %s\n", tag);
        std::exit(1);
    }

    for (uint32_t word : words) {
        const unsigned char bytes[4] = {
            static_cast<unsigned char>(word & 0xffU),
            static_cast<unsigned char>((word >> 8) & 0xffU),
            static_cast<unsigned char>((word >> 16) & 0xffU),
            static_cast<unsigned char>((word >> 24) & 0xffU),
        };
        if (::write(fd, bytes, sizeof(bytes)) != static_cast<ssize_t>(sizeof(bytes))) {
            std::fprintf(stderr, "failed to write temp binary for %s\n", tag);
            ::close(fd);
            ::unlink(path);
            std::exit(1);
        }
    }

    ::close(fd);
    return path;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

bool expect_line_with_fields(const std::vector<std::string>& lines,
                             const std::string& output,
                             std::initializer_list<const char*> needles,
                             const char* message) {
    for (const std::string& line : lines) {
        bool matched = true;
        for (const char* needle : needles) {
            if (line.find(needle) == std::string::npos) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return true;
        }
    }

    std::fprintf(stderr, "%s\n", message);
    std::fprintf(stderr, "output was:\n%s\n", output.c_str());
    return false;
}

std::string build_flat_load_command(const std::string& path, const char* backend) {
    std::ostringstream command;
    command << "{\"cmd\":\"load\",\"image\":\"" << path
            << "\",\"backend\":\"" << backend << "\",\"flat\":true,\"addr\":\"0x" << std::hex << kDebugProgramAddr
            << "\"}\n";
    return command.str();
}

std::string build_flat_load_command(const std::string& path) {
    return build_flat_load_command(path, "pipeline");
}

std::string repeat_command(const char* command, int count) {
    std::ostringstream script;
    for (int i = 0; i < count; ++i) {
        script << command << '\n';
    }
    return script.str();
}

std::string run_until_halt_command(std::uint64_t max_steps) {
    std::ostringstream script;
    script << "{\"cmd\":\"run_until_halt\",\"max_steps\":" << max_steps << "}\n";
    return script.str();
}

std::string run_cli_script(const std::string& script) {
    std::istringstream in(script);
    std::ostringstream out;
    std::ostringstream err;

    const int status = run_debug_cli(in, out, err);
    if (status != 0) {
        std::fprintf(stderr, "debug cli exited with status %d\n", status);
        std::fprintf(stderr, "stderr:\n%s\n", err.str().c_str());
        std::exit(1);
    }

    return out.str();
}

struct TempBinary {
    std::string path{};

    ~TempBinary() {
        if (!path.empty()) {
            ::unlink(path.c_str());
        }
    }
};

}  // namespace

int main() {
    const TempBinary external_binary{write_temp_binary("external", kSupervisorExternalProgram)};
    const TempBinary timer_binary{write_temp_binary("timer", kSupervisorTimerProgram)};
    const TempBinary predictor_binary{write_temp_binary("predictor", kPredictorProgram)};
    const TempBinary mmio_fault_binary{write_temp_binary("mmio_fault", kMmioFaultProgram)};
    const TempBinary store_queue_binary{write_temp_binary("store_queue", kStoreQueueProgram)};
    const TempBinary jit_dispatch_loop_binary{
        write_temp_binary("jit_dispatch_loop", kJitDispatchLoopProgram)};

    const std::string external_pending_output =
        run_cli_script(build_flat_load_command(external_binary.path) +
                       repeat_command("{\"cmd\":\"step_cycle\"}", 40) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> external_pending_lines = split_lines(external_pending_output);
    if (!expect_contains(external_pending_output, "\"cmd\":\"load\"", "load response should be emitted")) {
        return 1;
    }
    if (!expect_contains(external_pending_output, "\"type\":\"snapshot\"", "snapshot response should be emitted")) {
        return 1;
    }
    if (!expect_contains(external_pending_output, "\"backend\":\"pipeline\"", "snapshot should report pipeline backend")) {
        return 1;
    }
    if (!expect_contains(external_pending_output, "\"pipeline\"", "snapshot should include pipeline section")) {
        return 1;
    }
    if (!expect_contains(external_pending_output, "\"gpr\"", "snapshot should include register state")) {
        return 1;
    }
    if (!expect_contains(external_pending_output, "\"profile\":{", "snapshot should include execution profile state")) {
        return 1;
    }
    if (!expect_line_with_fields(
            external_pending_lines,
            external_pending_output,
            {
                "\"halted\":false",
                "\"privilege\":\"S\"",
                "\"ier\":2",
                "\"pending\":true",
                "\"supervisor_has_pending\":true",
                "\"scause\":\"0x8000000000000009\"",
            },
            "external interrupt pending snapshot should expose delegated supervisor interrupt state")) {
        return 1;
    }

    const std::string jit_dispatch_output =
        run_cli_script(build_flat_load_command(jit_dispatch_loop_binary.path, "functional") +
                       repeat_command("{\"cmd\":\"step_cycle\"}", 12) +
                       "{\"cmd\":\"jit_dispatch\"}\n{\"cmd\":\"quit\"}\n");
    if (!expect_contains(jit_dispatch_output,
                         "\"type\":\"jit_dispatch\"",
                         "jit dispatch response should be emitted")) {
        return 1;
    }
    if (!expect_contains(jit_dispatch_output,
                         "\"source\":\"hot-path-profile\"",
                         "jit dispatch response should expose hot-path source")) {
        return 1;
    }
    if (!expect_contains(jit_dispatch_output,
                         "\"action\":\"reference-fallback\"",
                         "jit dispatch response should expose reference fallback action")) {
        return 1;
    }
    if (!expect_contains(jit_dispatch_output,
                         "\"reject_kind\":\"control-flow\"",
                         "jit dispatch response should expose typed reject kind")) {
        return 1;
    }
    if (!expect_contains(jit_dispatch_output,
                         "\"host_code\":false",
                         "jit dispatch response should preserve no-host-code flag")) {
        return 1;
    }

    const std::string external_final_output =
        run_cli_script(build_flat_load_command(external_binary.path) +
                       repeat_command("{\"cmd\":\"step_commit\"}", 64) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> external_final_lines = split_lines(external_final_output);
    if (!expect_line_with_fields(
            external_final_lines,
            external_final_output,
            {
                "\"halted\":true",
                "\"privilege\":\"M\"",
                "\"trap_flush\":true",
                "\"recent_output\":\"PE\"",
                "\"ier\":0",
                "\"pending\":false",
                "\"claimed\":false",
                "\"supervisor_has_pending\":false",
                "\"scause\":\"0x8000000000000009\"",
                "\"kind\":\"trap\"",
                "\"kind\":\"flush\"",
                "\"kind\":\"halt\"",
            },
            "external interrupt final snapshot should expose handler completion and pipeline events")) {
        return 1;
    }
    if (!expect_contains(
            external_final_output,
            "\"total_traps\":2",
            "external interrupt final snapshot should lock total trap count in the execution profile")) {
        return 1;
    }
    if (!expect_contains(
            external_final_output,
            "\"traps\":[{\"pc\":\"0x800000f0\",\"raw\":\"0x0\",\"cause\":\"0x8000000000000009\",\"privilege\":\"S\",\"interrupt\":true,\"count\":1},{\"pc\":\"0x80000104\",\"raw\":\"0x100073\",\"cause\":\"0x3\",\"privilege\":\"M\",\"interrupt\":false,\"count\":1}]",
            "external interrupt final snapshot should lock the delegated supervisor external trap profile")) {
        return 1;
    }
    if (!expect_contains(
            external_final_output,
            "\"shadow_cache\":{\"line_size_bytes\":64,\"capacity_lines\":64,\"resident_lines\":0,\"line_accesses\":0,\"hits\":0,\"misses\":0,\"evictions\":0,\"bypasses\":9},\"memory_regions\":[{\"label\":\"plic\",\"kind\":\"mmio\",\"cacheable\":false,\"dma_visible\":false,\"has_side_effect\":true,\"supports_burst\":false,\"accesses\":5,\"reads\":1,\"writes\":4,\"faults\":0,\"bytes\":20,\"shadow_cache_line_accesses\":0,\"shadow_cache_hits\":0,\"shadow_cache_misses\":0,\"shadow_cache_evictions\":0,\"shadow_cache_bypasses\":5},{\"label\":\"uart\",\"kind\":\"mmio\",\"cacheable\":false,\"dma_visible\":false,\"has_side_effect\":true,\"supports_burst\":false,\"accesses\":4,\"reads\":0,\"writes\":4,\"faults\":0,\"bytes\":4,\"shadow_cache_line_accesses\":0,\"shadow_cache_hits\":0,\"shadow_cache_misses\":0,\"shadow_cache_evictions\":0,\"shadow_cache_bypasses\":4}]",
            "external interrupt final snapshot should lock representative MMIO memory-region profile counts")) {
        return 1;
    }

    const std::string timer_pending_output =
        run_cli_script(build_flat_load_command(timer_binary.path) +
                       repeat_command("{\"cmd\":\"step_cycle\"}", 35) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> timer_pending_lines = split_lines(timer_pending_output);
    if (!expect_line_with_fields(
            timer_pending_lines,
            timer_pending_output,
            {
                "\"halted\":false",
                "\"privilege\":\"M\"",
                "\"timer_interrupt_pending\":true",
                "\"mip\":\"0x20\"",
                "\"sip\":\"0x20\"",
            },
            "timer snapshot should expose pending CLINT state before delegated delivery")) {
        return 1;
    }

    const std::string timer_final_output =
        run_cli_script(build_flat_load_command(timer_binary.path) +
                       repeat_command("{\"cmd\":\"step_commit\"}", 64) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> timer_final_lines = split_lines(timer_final_output);
    if (!expect_line_with_fields(
            timer_final_lines,
            timer_final_output,
            {
                "\"halted\":true",
                "\"privilege\":\"M\"",
                "\"trap_flush\":true",
                "\"recent_output\":\"T\"",
                "\"timer_interrupt_pending\":false",
                "\"scause\":\"0x8000000000000005\"",
                "\"kind\":\"trap\"",
                "\"kind\":\"flush\"",
                "\"kind\":\"halt\"",
            },
            "timer final snapshot should expose delegated CLINT interrupt completion and pipeline events")) {
        return 1;
    }

    if (!expect_contains(timer_final_output, "\"cmd\":\"quit\"", "quit response should be emitted")) {
        return 1;
    }

    const std::string mmio_fault_output =
        run_cli_script(build_flat_load_command(mmio_fault_binary.path, "functional") +
                       repeat_command("{\"cmd\":\"step_cycle\"}", 3) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> mmio_fault_lines = split_lines(mmio_fault_output);
    if (!expect_line_with_fields(
            mmio_fault_lines,
            mmio_fault_output,
            {
                "\"backend\":\"functional\"",
                "\"mcause\":\"0x7\"",
                "\"mtval\":\"0x10000000\"",
                "\"valid\":true",
                "\"success\":false",
                "\"write\":true",
                "\"mmio\":true",
                "\"device\":\"uart\"",
                "\"addr\":\"0x10000000\"",
                "\"size\":4",
                "\"detail\":\"invalid MMIO access\"",
                "\"kind\":\"store\"",
            },
            "failed MMIO snapshot should expose bus failure detail through the debug CLI protocol")) {
        return 1;
    }

    const std::string terminal_io_output =
        run_cli_script(build_flat_load_command(external_binary.path) +
                       "{\"cmd\":\"run_until_new_uart_contains\","
                       "\"offset\":0,\"text\":\"PE\",\"max_steps\":4096}\n"
                       "{\"cmd\":\"uart_output\",\"offset\":2}\n"
                       "{\"cmd\":\"uart_input\",\"text\":\"abc\"}\n"
                       "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> terminal_io_lines = split_lines(terminal_io_output);
    if (!expect_line_with_fields(
            terminal_io_lines,
            terminal_io_output,
            {
                "\"type\":\"uart_output\"",
                "\"offset\":0",
                "\"next_offset\":2",
                "\"text\":\"PE\"",
            },
            "uart_output should expose the full UART log from offset 0")) {
        return 1;
    }
    if (!expect_line_with_fields(
            terminal_io_lines,
            terminal_io_output,
            {
                "\"type\":\"uart_output\"",
                "\"offset\":2",
                "\"next_offset\":2",
                "\"text\":\"\"",
            },
            "uart_output should return an empty increment once the offset catches up")) {
        return 1;
    }
    if (!expect_line_with_fields(
            terminal_io_lines,
            terminal_io_output,
            {
                "\"type\":\"ok\"",
                "\"cmd\":\"uart_input\"",
            },
            "uart_input should be accepted as a first-class debug CLI command")) {
        return 1;
    }

    const std::string predictor_output =
        run_cli_script(build_flat_load_command(predictor_binary.path) +
                       repeat_command("{\"cmd\":\"step_commit\"}", 16) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> predictor_lines = split_lines(predictor_output);
    const std::string predictor_unresolved_output =
        run_cli_script(build_flat_load_command(predictor_binary.path) +
                       repeat_command("{\"cmd\":\"step_cycle\"}", 1) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> predictor_unresolved_lines = split_lines(predictor_unresolved_output);
    if (!expect_line_with_fields(
            predictor_unresolved_lines,
            predictor_unresolved_output,
            {
                "\"halted\":false",
                "\"backend\":\"pipeline\"",
                "\"predictor\"",
                "\"total_predictions\":0",
                "\"correct_predictions\":0",
                "\"mispredictions\":0",
            },
            "predictor counters should stay at zero before any control-flow outcome is resolved")) {
        return 1;
    }
    if (!expect_line_with_fields(
            predictor_lines,
            predictor_output,
            {
                "\"halted\":true",
                "\"backend\":\"pipeline\"",
                "\"predictor\"",
                "\"mode\":\"bimodal-2bit\"",
                "\"last_prediction_valid\":true",
                "\"last_prediction_taken\":true",
                "\"last_prediction_correct\":true",
                "\"last_prediction_pc\":\"0x80000078\"",
                "\"last_prediction_target\":\"0x80000080\"",
                "\"total_predictions\":1",
                "\"correct_predictions\":1",
                "\"mispredictions\":0",
            },
            "predictor snapshot should expose branch prediction state and counters")) {
        return 1;
    }
    if (!expect_contains(predictor_output, "\"last_sequence_id\":", "predictor snapshot should expose last_sequence_id")) {
        return 1;
    }
    if (!expect_contains(predictor_output, "\"retire_trace\":[", "predictor snapshot should expose retire_trace")) {
        return 1;
    }
    if (!expect_contains(predictor_output, "\"sequence_id\":1", "predictor retire trace should include sequence ids")) {
        return 1;
    }
    if (!expect_contains(predictor_output, "\"hot_paths\":[", "predictor snapshot should expose hot-path profile state")) {
        return 1;
    }
    if (!expect_contains(predictor_output, "\"branches\":[", "predictor snapshot should expose branch profile state")) {
        return 1;
    }
    if (!expect_contains(
            predictor_output,
            "\"total_retirements\":3",
            "predictor snapshot should lock total retirements in the execution profile")) {
        return 1;
    }
    if (!expect_contains(
            predictor_output,
            "\"hot_paths\":[{\"start_pc\":\"0x80000080\",\"end_pc\":\"0x80000084\",\"executions\":1,\"retired_instructions\":2},{\"start_pc\":\"0x80000078\",\"end_pc\":\"0x80000078\",\"executions\":1,\"retired_instructions\":1}]",
            "predictor snapshot should lock representative hot-path profile counts")) {
        return 1;
    }
    if (!expect_contains(
            predictor_output,
            "\"branches\":[{\"pc\":\"0x80000078\",\"raw\":\"0x80006f\",\"executions\":1,\"redirects\":1}]",
            "predictor snapshot should lock representative branch profile counts")) {
        return 1;
    }
    if (!expect_contains(
            predictor_output,
            "\"syscalls\":[{\"pc\":\"0x80000084\",\"raw\":\"0x73\",\"count\":1}]",
            "predictor snapshot should lock representative syscall profile counts")) {
        return 1;
    }

    const std::string store_queue_output =
        run_cli_script(build_flat_load_command(store_queue_binary.path) +
                       repeat_command("{\"cmd\":\"step_cycle\"}", 6) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> store_queue_lines = split_lines(store_queue_output);
    if (!expect_line_with_fields(
            store_queue_lines,
            store_queue_output,
            {
                "\"backend\":\"pipeline\"",
                "\"ooo\"",
                "\"rob_depth\":3",
                "\"rob_head_sequence_id\":3",
                "\"lsq_depth\":1",
                "\"lsq_head_sequence_id\":3",
            },
            "pipeline snapshot should expose ROB/LSQ queue depth and head sequence")) {
        return 1;
    }
    if (!expect_contains(store_queue_output, "\"memory_regions\":[",
                         "pipeline snapshot should expose memory-region profile state")) {
        return 1;
    }
    if (!expect_contains(
            store_queue_output,
            "\"profile\":{\"total_retirements\":2,\"total_traps\":0,\"total_memory_observations\":0,\"hot_paths\":[{\"start_pc\":\"0x80000078\",\"end_pc\":\"0x8000007c\",\"executions\":1,\"retired_instructions\":2}],\"branches\":[],\"branch_targets\":[],\"syscalls\":[],\"traps\":[],\"shadow_cache\":{\"line_size_bytes\":64,\"capacity_lines\":64,\"resident_lines\":0,\"line_accesses\":0,\"hits\":0,\"misses\":0,\"evictions\":0,\"bypasses\":0},\"memory_regions\":[],\"pc_costs\":[{\"pc\":\"0x80000078\",\"raw\":\"0x7a00293\",\"retirements\":1,\"cycles\":4,\"memory_observations\":0,\"memory_reads\":0,\"memory_writes\":0,\"memory_faults\":0,\"memory_bytes\":0},{\"pc\":\"0x8000007c\",\"raw\":\"0x80001537\",\"retirements\":1,\"cycles\":1,\"memory_observations\":0,\"memory_reads\":0,\"memory_writes\":0,\"memory_faults\":0,\"memory_bytes\":0}]}",
            "pre-commit store queue snapshot should lock the zero-memory-observation profile shape")) {
        return 1;
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kDebugProgramAddr);

        PipelineBackend backend(cpu, bus);
        LoadStoreQueue& lsq = backend.testing_state().lsq();
        const LsqIndex older_store = lsq.enqueue_store({
            .sequence_id = 1,
            .size = 4,
        });
        const LsqIndex younger_load = lsq.enqueue_load({
            .sequence_id = 2,
            .rd = 6,
            .size = 4,
        });
        lsq.mark_address_ready(younger_load, 0x80000100ULL);
        lsq.mark_data_ready(younger_load, 0x11223344ULL);
        lsq.mark_order_ready(younger_load);
        lsq.mark_address_ready(older_store, 0x80000100ULL);

        DebugSnapshot snapshot{};
        snapshot.summary.pc = cpu.core().pc();
        snapshot.summary.privilege = cpu.core().privilege_mode();
        snapshot.summary.backend = backend.name();
        snapshot.pipeline = backend.debug_snapshot().pipeline;

        const std::string replay_snapshot_output = debug_snapshot_json(snapshot);
        if (!expect_contains(replay_snapshot_output,
                             "\"lsq_load_state\":\"replay_required\"",
                             "debug snapshot JSON should serialize LSQ replay-needed state")) {
            return 1;
        }
        if (!expect_contains(replay_snapshot_output,
                             "\"lsq_load_sequence_id\":2",
                             "debug snapshot JSON should serialize the replaying load sequence id")) {
            return 1;
        }
        if (!expect_contains(replay_snapshot_output,
                             "\"lsq_store_sequence_id\":1",
                             "debug snapshot JSON should serialize the conflicting older store sequence id")) {
            return 1;
        }

        constexpr uint32_t kReplayPhys = 33;
        backend.testing_state().phys_regs().write(kReplayPhys, 0x11223344ULL);
        const RobIndex replay_rob = backend.testing_state().rob().allocate({
            .sequence_id = 2,
            .pc = kDebugProgramAddr,
            .raw = 0x00150303U,
            .arch_rd = 6,
            .phys_rd = kReplayPhys,
            .previous_phys_rd = 6,
        });
        backend.testing_state().rob().mark_ready(replay_rob, {
            .value_ready = true,
            .value = 0x11223344ULL,
        });
        backend.testing_state().mem_wb.slot.valid = true;
        backend.testing_state().mem_wb.slot.sequence_id.value = 2;
        backend.testing_state().mem_wb.slot.pc = kDebugProgramAddr;
        backend.testing_state().mem_wb.slot.raw = 0x00150303U;
        backend.testing_state().mem_wb.slot.rd_phys = kReplayPhys;
        backend.testing_state().mem_wb.slot.rob_index = replay_rob;
        backend.testing_state().mem_wb.slot.lsq_index = younger_load;
        backend.testing_state().mem_wb.slot.effects.rd_write.enable = true;
        backend.testing_state().mem_wb.slot.effects.rd_write.rd = 6;
        backend.testing_state().mem_wb.slot.effects.rd_write.value = 0x11223344ULL;

        backend.step();
        snapshot.pipeline = backend.debug_snapshot().pipeline;
        const std::string replay_flush_output = debug_snapshot_json(snapshot);
        if (!expect_contains(replay_flush_output,
                             "\"replay_flush\":true",
                             "debug snapshot JSON should serialize replay flush once automatic replay fires")) {
            return 1;
        }
        if (!expect_contains(replay_flush_output,
                             "\"trap_flush\":false",
                             "automatic replay should not be reported as a trap flush")) {
            return 1;
        }
        if (!expect_contains(replay_flush_output,
                             "\"lsq_depth\":0",
                             "automatic replay should clear speculative LSQ state in the debug snapshot")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kDebugProgramAddr);
        cpu.core().write_gpr(10, 0x80000100ULL);

        PipelineBackend backend(cpu, bus);
        LoadStoreQueue& lsq = backend.testing_state().lsq();
        const LsqIndex older_store = lsq.enqueue_store({
            .sequence_id = 1,
            .size = 4,
        });
        lsq.mark_address_ready(older_store, 0x80000100ULL);

        backend.testing_state().if_id.slot.valid = true;
        backend.testing_state().if_id.slot.sequence_id.value = 2;
        backend.testing_state().if_id.slot.pc = kDebugProgramAddr;
        backend.testing_state().if_id.slot.raw = 0x00052083U;

        backend.step();

        DebugSnapshot snapshot{};
        snapshot.summary.pc = cpu.core().pc();
        snapshot.summary.privilege = cpu.core().privilege_mode();
        snapshot.summary.backend = backend.name();
        snapshot.pipeline = backend.debug_snapshot().pipeline;

        const std::string stalled_output = debug_snapshot_json(snapshot);
        if (!expect_contains(stalled_output,
                             "\"stall_reason\":\"blocked_by_overlapping_store\"",
                             "debug snapshot JSON should serialize the decode overlapping-store stall attribution")) {
            return 1;
        }
    }

    {
        DebugSession session;
        session.load_binary(predictor_binary.path,
                            kDebugProgramAddr,
                            BackendKind::Pipeline,
                            BlockTransport::SimpleStorage,
                            nullptr);
        const DebugSnapshot before = session.snapshot();
        DebugSnapshot after = before;
        after.pipeline.stalled = true;
        after.pipeline.stall_reason = "blocked_by_overlapping_store";
        session.record_step_events(before, after);
        const DebugSnapshot snapshot = session.snapshot();
        if (!expect_contains(
                debug_snapshot_json(snapshot),
                "\"events\":[",
                "debug session snapshot should serialize recorded events")) {
            return 1;
        }
        if (!expect_contains(
                debug_snapshot_json(snapshot),
                "\"detail\":\"pipeline stalled: blocked_by_overlapping_store\"",
                "debug session stall event should explain the concrete stall_reason instead of a generic load-use hazard")) {
            return 1;
        }
    }

    {
        DebugSession session;
        session.load_binary(predictor_binary.path,
                            kDebugProgramAddr,
                            BackendKind::Functional,
                            BlockTransport::SimpleStorage,
                            nullptr,
                            true,
                            true,
                            true);
        const AddressSpace::AccessResult loaded = session.machine().cpu().address_space().load_result(
            session.machine().bus(),
            kDebugProgramAddr,
            4);
        if (!loaded.ok) {
            std::fprintf(stderr, "debug session L1D setup load should succeed\n");
            return 1;
        }
        const DebugSnapshot populated = session.snapshot();
        if (!populated.l1_data_cache.enabled || populated.l1_data_cache.loads == 0 ||
            populated.l1_data_cache.misses == 0) {
            std::fprintf(stderr, "debug session L1D setup should expose populated counters\n");
            return 1;
        }
        session.reset();
        const DebugSnapshot reset_snapshot = session.snapshot();
        if (!reset_snapshot.l1_data_cache.enabled) {
            std::fprintf(stderr, "debug session reset should preserve opt-in L1D enabled state\n");
            return 1;
        }
        if (reset_snapshot.l1_data_cache.loads != 0 || reset_snapshot.l1_data_cache.stores != 0 ||
            reset_snapshot.l1_data_cache.hits != 0 || reset_snapshot.l1_data_cache.misses != 0 ||
            reset_snapshot.l1_data_cache.evictions != 0 || reset_snapshot.l1_data_cache.bypasses != 0 ||
            reset_snapshot.l1_data_cache.write_through_stores != 0) {
            std::fprintf(stderr, "debug session reset should clear L1D counters\n");
            return 1;
        }
    }

    {
        DebugSession session;
        session.load_binary(predictor_binary.path,
                            kDebugProgramAddr,
                            BackendKind::Functional,
                            BlockTransport::SimpleStorage,
                            nullptr);
        session.load_binary_payload(store_queue_binary.path, kDebugProgramAddr + 0x100);
        session.set_gpr("a1", 0x88000000ULL);
        const size_t post_load_actions_after_setup = session.post_load_action_count();
        session.reset();
        session.reset();

        if (session.post_load_action_count() != post_load_actions_after_setup) {
            std::fprintf(stderr, "debug session reset should not append duplicate post-load actions\n");
            return 1;
        }

        const DebugSnapshot snapshot = session.snapshot();
        if (snapshot.gpr[11] != 0x88000000ULL) {
            std::fprintf(stderr, "debug session reset should replay configured set_gpr actions\n");
            return 1;
        }
        uint64_t value = 0;
        if (!(session.machine().bus().try_load(kDebugProgramAddr + 0x100, 1, value) && value == 0x93)) {
            std::fprintf(stderr, "debug session reset should replay configured payload loads\n");
            return 1;
        }
    }

    {
        CPU cpu;
        cpu_init(cpu, kDebugProgramAddr);
        if (!cpu.core().vector().set_config(4, 3)) {
            std::fprintf(stderr, "vector config should accept sew=4 vl=3 for debug snapshot smoke\n");
            return 1;
        }

        VectorState::VectorReg relu{};
        relu.fill(0);
        relu[0] = 0x07;
        relu[4] = 0x00;
        relu[8] = 0x07;
        cpu.core().vector().write_reg(5, relu);

        DebugSnapshot snapshot{};
        snapshot.summary.pc = cpu.core().pc();
        snapshot.summary.privilege = cpu.core().privilege_mode();
        snapshot.summary.backend = "functional";
        snapshot.vector.sew_bytes = cpu.core().vector().sew_bytes();
        snapshot.vector.vl = cpu.core().vector().vl();
        snapshot.vector.registers[5] = cpu.core().vector().read_reg(5);

        const std::string vector_output = debug_snapshot_json(snapshot);
        if (!expect_contains(vector_output,
                             "\"vector\":{\"sew_bytes\":4,\"vl\":3,\"registers\":[",
                             "debug snapshot JSON should serialize vector config")) {
            return 1;
        }
        if (!expect_contains(vector_output,
                             "\"0x07000000000000000700000000000000\"",
                             "debug snapshot JSON should serialize vector register dumps")) {
            return 1;
        }
    }

    {
        const std::string ai_accel_output =
            run_cli_script("{\"cmd\":\"load\",\"image\":\"guest/ai_accel_demo.elf\",\"backend\":\"pipeline\"}\n" +
                           run_until_halt_command(12000000) + "{\"cmd\":\"snapshot\"}\n" +
                           "{\"cmd\":\"uart_output\",\"offset\":0}\n{\"cmd\":\"quit\"}\n");
        const std::vector<std::string> ai_accel_lines = split_lines(ai_accel_output);
        if (!expect_contains(ai_accel_output,
                             "\"text\":\"KMVAI\"",
                             "AI guest demo should surface KMVAI on success")) {
            return 1;
        }
        if (!expect_line_with_fields(
                ai_accel_lines,
                ai_accel_output,
                {
                    "\"type\":\"snapshot\"",
                    "\"halted\":true",
                    "\"recent_output\":\"KMVAI\"",
                    "\"ai_accelerator\":{\"present\":true",
                    "\"queue_depth\":0",
                    "\"doorbell_count\":1",
                    "\"last_fault\":0",
                    "\"completion_count\":1",
                    "\"engine_busy\":false",
                    "\"scratchpad_occupancy_bytes\":0",
                    "\"dma_load_bytes\":12",
                    "\"dma_store_bytes\":4",
                    "\"device_cycles\":8",
                    "\"dma_cycles\":6",
                    "\"compute_cycles\":1",
                    "\"stall_cycles\":1",
                    "\"busy_cycles\":10",
                    "\"queue_cycles\":1",
                    "\"completion_cycles\":1",
                    "\"effective_ops_per_cycle\":3",
                    "\"utilization\":10",
                },
                "AI guest demo snapshot should expose final AI accelerator debug counters")) {
            return 1;
        }
        if (!expect_contains(ai_accel_output,
                             "\"cmd\":\"quit\"",
                             "quit response should be emitted for AI guest demo")) {
            return 1;
        }
    }

    return 0;
}
