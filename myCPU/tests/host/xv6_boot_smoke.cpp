#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "../../include/platform_mmio.h"
#include "../../src/arch/csr_file.h"
#include "../../src/platform/machine.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool expect_file_exists(const std::filesystem::path& path, const char* message) {
    if (!std::filesystem::exists(path)) {
        std::fprintf(stderr, "%s: %s\n", message, path.string().c_str());
        return false;
    }
    return true;
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool expect_contains(const std::string& text, const char* needle, const char* message) {
    if (text.find(needle) == std::string::npos) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

const ExecutionMemoryRegionEntry* find_profile_region_kind(const ExecutionProfileSnapshot& profile,
                                                           const char* kind) {
    for (const ExecutionMemoryRegionEntry& entry : profile.memory_regions) {
        if (entry.kind == kind && entry.accesses != 0) {
            return &entry;
        }
    }
    return nullptr;
}

uint64_t read_symbol_address(const std::filesystem::path& path, const char* symbol) {
    std::ifstream in(path);
    std::string row;
    while (std::getline(in, row)) {
        std::istringstream fields(row);
        uint64_t address = 0;
        std::string name;
        if (fields >> std::hex >> address >> name && name == symbol) {
            return address;
        }
    }
    return 0;
}

struct BootBaseline {
    uint64_t main_pc;
    uint64_t memset_pc;
    uint64_t memory_observations;
    uint64_t ram_accesses;
    uint64_t ram_reads;
    uint64_t ram_writes;
    uint64_t ram_bytes;
    uint64_t shadow_hits;
};

// Keep exact checkpoints for the audited GCC 10.2 and Ubuntu GCC 13.2 builds.
constexpr BootBaseline kBootBaselines[] = {
    {0x80001348ULL, 0x800010dcULL, 1570, 1515, 621, 894, 8562, 1495},
    {0x800013d4ULL, 0x80001174ULL, 1558, 1503, 618, 885, 8508, 1483},
};

}  // namespace

int main() {
    const std::filesystem::path board_profile = "workloads/boards/mycpu_virt.mk";
    const std::filesystem::path workload_profile = "workloads/xv6/profile.mk";
    const std::filesystem::path upstream_makefile = "external/xv6-riscv/Makefile";
    const std::filesystem::path kernel_image = "external/xv6-riscv/kernel/kernel";
    const std::filesystem::path kernel_symbols = "external/xv6-riscv/kernel/kernel.sym";
    const std::filesystem::path fs_image = "external/xv6-riscv/fs.img";

    if (!expect_file_exists(board_profile, "xv6 smoke expects a reusable board profile") ||
        !expect_file_exists(workload_profile, "xv6 smoke expects a workload profile") ||
        !expect_file_exists(upstream_makefile, "xv6 smoke expects vendored xv6 sources") ||
        !expect_file_exists(kernel_image, "xv6 smoke expects a built kernel image") ||
        !expect_file_exists(kernel_symbols, "xv6 smoke expects the built kernel symbol table") ||
        !expect_file_exists(fs_image, "xv6 smoke expects a built filesystem image")) {
        return 1;
    }

    const uint64_t main_pc = read_symbol_address(kernel_symbols, "main");
    const uint64_t memset_pc = read_symbol_address(kernel_symbols, "memset") + 0x24;
    const BootBaseline* baseline = nullptr;
    for (const BootBaseline& candidate : kBootBaselines) {
        if (candidate.main_pc == main_pc && candidate.memset_pc == memset_pc) {
            baseline = &candidate;
            break;
        }
    }
    if (!expect(baseline != nullptr, "xv6 smoke has no audited baseline for this kernel layout")) {
        std::fprintf(stderr, "main=0x%llx memset+0x24=0x%llx\n",
                     static_cast<unsigned long long>(main_pc),
                     static_cast<unsigned long long>(memset_pc));
        return 1;
    }

    const std::string board_text = read_text_file(board_profile);
    const std::string workload_text = read_text_file(workload_profile);
    if (!expect_contains(board_text,
                         "BOARD_XV6_ARCH_MARCH := -march=rv64ima",
                         "board profile should pin xv6 to rv64ima for current myCPU bring-up") ||
        !expect_contains(board_text,
                         "BOARD_BLOCK_TRANSPORT := virtio-blk",
                         "board profile should record the current virtio-blk block transport") ||
        !expect_contains(board_text,
                         "BOARD_BLOCK_IRQ := 1",
                         "board profile should align xv6 with the virtio IRQ contract") ||
        !expect_contains(workload_text,
                         "XV6_UPSTREAM_COMMIT := 5474d4bf72fd95a6e5c735c2d7f208f58990ceab",
                         "workload profile should pin the audited xv6 upstream commit")) {
        return 1;
    }

    Machine machine;
    machine.set_block_transport(BlockTransport::VirtioBlk);
    machine.attach_storage_image(fs_image.string());
    machine.load_elf(kernel_image.string());

    if (!expect(machine.block_transport() == BlockTransport::VirtioBlk,
                "xv6 smoke should boot the virtio board profile with the virtio-blk transport") ||
        !expect(machine.virtio_blk().attached(), "xv6 smoke should attach the filesystem image") ||
        !expect(machine.virtio_blk().capacity_sectors() > 0,
                "xv6 smoke should expose a non-empty virtio disk image to the board")) {
        return 1;
    }

    for (int i = 0; i < 5000; ++i) {
        machine.step();
    }

    const CPU& cpu = machine.cpu();
    const CoreState& core = cpu.core();
    const ExecutionProfileSnapshot profile = machine.backend().debug_snapshot().profile;
    const ExecutionShadowCacheSnapshot& shadow = profile.shadow_cache;
    const ExecutionMemoryRegionEntry* ram_region = find_profile_region_kind(profile, "ram");
    if (!expect(core.cycle() == 5000, "xv6 smoke should stop after the planned bring-up probe window") ||
        !expect(core.instret() == 5000,
                "xv6 smoke should retire through the current post-banner bring-up checkpoint") ||
        !expect(core.pc() == baseline->memset_pc,
                "xv6 smoke should currently be in the kernel allocator warm-up memset loop") ||
        !expect(core.privilege_mode() == PrivilegeMode::Supervisor,
                "xv6 smoke should already be running in supervisor mode at the current checkpoint") ||
        !expect(cpu.csr().read(CSR_MCAUSE, core) == 0,
                "xv6 smoke should currently avoid machine-mode traps through the boot banner checkpoint") ||
        !expect(cpu.csr().read(CSR_MEPC, core) == baseline->main_pc,
                "xv6 smoke should retain the machine-mode handoff checkpoint in mepc") ||
        !expect(cpu.csr().read(CSR_MTVAL, core) == 0,
                "xv6 smoke should keep mtval clear through the boot banner checkpoint") ||
        !expect(cpu.csr().read(CSR_SCAUSE, core) == 0,
                "xv6 smoke should currently avoid supervisor traps through the boot banner checkpoint") ||
        !expect(cpu.csr().read(CSR_SEPC, core) == 0,
                "xv6 smoke should keep sepc clear through the boot banner checkpoint") ||
        !expect(cpu.csr().read(CSR_STVAL, core) == 0,
                "xv6 smoke should keep stval clear through the boot banner checkpoint") ||
        !expect(profile.total_retirements == 5000,
                "xv6 smoke should export the expected functional retire count") ||
        !expect(profile.total_traps == 0,
                "xv6 smoke should avoid trap observations at the current functional checkpoint") ||
        !expect(profile.total_memory_observations == baseline->memory_observations,
                "xv6 smoke should export the current functional memory observation baseline") ||
        !expect(ram_region != nullptr,
                "xv6 smoke should classify the current functional workload profile under RAM") ||
        !expect(ram_region->label == "ram",
                "xv6 smoke should keep RAM as the top functional memory region") ||
        !expect(ram_region->accesses == baseline->ram_accesses,
                "xv6 smoke should keep the current RAM access baseline") ||
        !expect(ram_region->reads == baseline->ram_reads,
                "xv6 smoke should keep the current RAM read baseline") ||
        !expect(ram_region->writes == baseline->ram_writes,
                "xv6 smoke should keep the current RAM write baseline") ||
        !expect(ram_region->faults == 0,
                "xv6 smoke should avoid RAM fault observations at the current checkpoint") ||
        !expect(ram_region->bytes == baseline->ram_bytes,
                "xv6 smoke should keep the current RAM byte baseline") ||
        !expect(shadow.line_size_bytes == 64,
                "xv6 smoke should export the functional shadow-cache line size") ||
        !expect(shadow.capacity_lines == 64,
                "xv6 smoke should export the functional shadow-cache capacity") ||
        !expect(shadow.resident_lines == 20,
                "xv6 smoke should keep the current functional shadow-cache residency") ||
        !expect(shadow.line_accesses == baseline->ram_accesses,
                "xv6 smoke should keep the current functional shadow-cache line-access baseline") ||
        !expect(shadow.hits == baseline->shadow_hits,
                "xv6 smoke should keep the current functional shadow-cache hit baseline") ||
        !expect(shadow.misses == 20,
                "xv6 smoke should keep the current functional shadow-cache miss baseline") ||
        !expect(shadow.evictions == 0,
                "xv6 smoke should avoid shadow-cache evictions at the current checkpoint") ||
        !expect(shadow.bypasses == 55,
                "xv6 smoke should keep the current functional shadow-cache bypass baseline") ||
        !expect(ram_region->shadow_cache_line_accesses == shadow.line_accesses,
                "xv6 smoke should keep RAM shadow-cache line accesses aligned with the global summary") ||
        !expect(ram_region->shadow_cache_hits == shadow.hits,
                "xv6 smoke should keep RAM shadow-cache hits aligned with the global summary") ||
        !expect(ram_region->shadow_cache_misses == shadow.misses,
                "xv6 smoke should keep RAM shadow-cache misses aligned with the global summary") ||
        !expect(ram_region->shadow_cache_evictions == shadow.evictions,
                "xv6 smoke should keep RAM shadow-cache evictions aligned with the global summary") ||
        !expect_contains(machine.uart().output(),
                         "xv6 kernel is booting",
                         "xv6 smoke should print the boot banner before the allocator warm-up loop")) {
        return 1;
    }

    return 0;
}
