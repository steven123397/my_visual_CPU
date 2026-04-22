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

}  // namespace

int main() {
    const std::filesystem::path board_profile = "workloads/boards/mycpu_virt.mk";
    const std::filesystem::path workload_profile = "workloads/xv6/profile.mk";
    const std::filesystem::path upstream_makefile = "external/xv6-riscv/Makefile";
    const std::filesystem::path kernel_image = "external/xv6-riscv/kernel/kernel";
    const std::filesystem::path fs_image = "external/xv6-riscv/fs.img";

    if (!expect_file_exists(board_profile, "xv6 smoke expects a reusable board profile") ||
        !expect_file_exists(workload_profile, "xv6 smoke expects a workload profile") ||
        !expect_file_exists(upstream_makefile, "xv6 smoke expects vendored xv6 sources") ||
        !expect_file_exists(kernel_image, "xv6 smoke expects a built kernel image") ||
        !expect_file_exists(fs_image, "xv6 smoke expects a built filesystem image")) {
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
    if (!expect(core.cycle() == 5000, "xv6 smoke should stop after the planned bring-up probe window") ||
        !expect(core.instret() == 5000,
                "xv6 smoke should retire through the current post-banner bring-up checkpoint") ||
        !expect(core.pc() == 0x800010dcULL,
                "xv6 smoke should currently be in the kernel allocator warm-up memset loop") ||
        !expect(core.privilege_mode() == PrivilegeMode::Supervisor,
                "xv6 smoke should already be running in supervisor mode at the current checkpoint") ||
        !expect(cpu.csr().read(CSR_MCAUSE, core) == 0,
                "xv6 smoke should currently avoid machine-mode traps through the boot banner checkpoint") ||
        !expect(cpu.csr().read(CSR_MEPC, core) == 0x80001348ULL,
                "xv6 smoke should retain the machine-mode handoff checkpoint in mepc") ||
        !expect(cpu.csr().read(CSR_MTVAL, core) == 0,
                "xv6 smoke should keep mtval clear through the boot banner checkpoint") ||
        !expect(cpu.csr().read(CSR_SCAUSE, core) == 0,
                "xv6 smoke should currently avoid supervisor traps through the boot banner checkpoint") ||
        !expect(cpu.csr().read(CSR_SEPC, core) == 0,
                "xv6 smoke should keep sepc clear through the boot banner checkpoint") ||
        !expect(cpu.csr().read(CSR_STVAL, core) == 0,
                "xv6 smoke should keep stval clear through the boot banner checkpoint") ||
        !expect_contains(machine.uart().output(),
                         "xv6 kernel is booting",
                         "xv6 smoke should print the boot banner before the allocator warm-up loop")) {
        return 1;
    }

    return 0;
}
