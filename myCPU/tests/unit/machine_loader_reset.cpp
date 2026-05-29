#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>

#include <unistd.h>

#include "../../src/platform/machine.h"
#include "../../src/platform/address_map.h"
#include "../../src/isa/atomic_contract.h"

namespace {

constexpr uint64_t kBinaryAddr = MEM_BASE + 0x4000;
constexpr uint64_t kElfAddr = MEM_BASE + 0x8000;
constexpr uint32_t kElfMagic = 0x464C457F;
constexpr uint16_t kElfClass64 = 2;
constexpr uint8_t kElfDataLittleEndian = 1;
constexpr uint8_t kElfIdentVersionCurrent = 1;
constexpr uint16_t kElfTypeExec = 2;
constexpr uint16_t kElfMachineRiscv = 243;
constexpr uint32_t kElfVersionCurrent = 1;
constexpr uint32_t kProgramHeaderLoad = 1;
constexpr uint64_t kSegmentOffset = 0x100;

struct Elf64Ehdr {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

static_assert(sizeof(Elf64Ehdr) == 64, "unexpected ELF header size");
static_assert(sizeof(Elf64Phdr) == 56, "unexpected program header size");

std::string create_temp_path(const char* prefix) {
    char path_template[64];
    std::snprintf(path_template, sizeof(path_template), "/tmp/%s_XXXXXX", prefix);
    const int fd = mkstemp(path_template);
    if (fd < 0) {
        throw std::runtime_error("failed to create temp loader input");
    }
    close(fd);
    return path_template;
}

std::string write_temp_binary(const std::array<uint8_t, 4>& bytes, size_t count) {
    const std::string path = create_temp_path("mycpu_machine_bin");
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::remove(path.c_str());
        throw std::runtime_error("failed to open temp binary for writing");
    }
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(count));
    file.close();
    return path;
}

std::string write_temp_storage_image() {
    const std::string path = create_temp_path("mycpu_storage_img");
    const std::array<uint8_t, 4> bytes = {'S', 't', 'o', 'r'};
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::remove(path.c_str());
        throw std::runtime_error("failed to open temp storage image for writing");
    }
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    file.close();
    return path;
}

Elf64Ehdr make_ehdr(uint64_t entry) {
    Elf64Ehdr ehdr{};
    std::memcpy(ehdr.e_ident, &kElfMagic, sizeof(kElfMagic));
    ehdr.e_ident[4] = kElfClass64;
    ehdr.e_ident[5] = kElfDataLittleEndian;
    ehdr.e_ident[6] = kElfIdentVersionCurrent;
    ehdr.e_type = kElfTypeExec;
    ehdr.e_machine = kElfMachineRiscv;
    ehdr.e_version = kElfVersionCurrent;
    ehdr.e_entry = entry;
    ehdr.e_phoff = sizeof(Elf64Ehdr);
    ehdr.e_ehsize = sizeof(Elf64Ehdr);
    ehdr.e_phentsize = sizeof(Elf64Phdr);
    ehdr.e_phnum = 1;
    return ehdr;
}

std::string write_temp_elf(const std::array<uint8_t, 4>& bytes,
                           size_t count,
                           uint64_t load_addr) {
    const std::string path = create_temp_path("mycpu_machine_elf");
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::remove(path.c_str());
        throw std::runtime_error("failed to open temp ELF for writing");
    }

    const Elf64Ehdr ehdr = make_ehdr(load_addr);
    Elf64Phdr phdr{};
    phdr.p_type = kProgramHeaderLoad;
    phdr.p_offset = kSegmentOffset;
    phdr.p_vaddr = load_addr;
    phdr.p_paddr = load_addr;
    phdr.p_filesz = count;
    phdr.p_memsz = count;
    phdr.p_align = 0x1000;

    file.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
    file.write(reinterpret_cast<const char*>(&phdr), sizeof(phdr));
    file.seekp(static_cast<std::streamoff>(phdr.p_offset), std::ios::beg);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(count));
    file.close();
    return path;
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool expect_core_reset(const Machine& machine, uint64_t expected_pc) {
    const CoreState& core = machine.cpu().core();
    return expect(machine.loaded(), "machine should report loaded after image load") &&
           expect(core.pc() == expected_pc, "image reload should reset pc to the new entry") &&
           expect(core.read_gpr(1) == 0, "image reload should clear architectural registers") &&
           expect(core.cycle() == 0, "image reload should reset cycle count") &&
           expect(core.instret() == 0, "image reload should reset instret") &&
           expect(!core.halted(), "image reload should clear halted state") &&
           expect(core.privilege_mode() == PrivilegeMode::Machine,
                  "image reload should restore machine privilege");
}

bool expect_loaded_bytes(Machine& machine,
                         uint64_t addr,
                         uint8_t first,
                         uint8_t second) {
    uint64_t value = 0;
    return expect(machine.bus().try_load(addr, 1, value) && value == first,
                  "reloaded image should expose the first byte at the load address") &&
           expect(machine.bus().try_load(addr + 1, 1, value) && value == second,
                  "reloaded image should expose the second byte at the load address") &&
           expect(machine.bus().try_load(addr + 2, 1, value) && value == 0,
                  "reloaded image should clear stale third-byte data from the previous image") &&
           expect(machine.bus().try_load(addr + 3, 1, value) && value == 0,
                  "reloaded image should clear stale fourth-byte data from the previous image");
}

bool expect_storage_error_cleared(const Machine& machine) {
    return expect((machine.storage().status() & STORAGE_STATUS_ERROR) == 0,
                  "image reload should clear sticky storage error status") &&
           expect(machine.storage().error_code() == STORAGE_ERR_NONE,
                  "image reload should clear sticky storage error code");
}

bool expect_payload_load_preserves_state(Machine& machine,
                                         uint64_t expected_pc,
                                         uint64_t expected_cycle,
                                         uint64_t expected_instret,
                                         uint64_t payload_addr,
                                         uint8_t first,
                                         uint8_t second) {
    uint64_t value = 0;
    return expect(machine.cpu().core().pc() == expected_pc,
                  "payload load should not reset the current pc") &&
           expect(machine.cpu().core().cycle() == expected_cycle,
                  "payload load should preserve cycle count") &&
           expect(machine.cpu().core().instret() == expected_instret,
                  "payload load should preserve instret") &&
           expect(machine.bus().try_load(payload_addr, 1, value) && value == first,
                  "payload load should expose the first byte at the requested address") &&
           expect(machine.bus().try_load(payload_addr + 1, 1, value) && value == second,
                  "payload load should expose the second byte at the requested address");
}

bool expect_gpr_seed_preserves_state(Machine& machine,
                                     uint64_t expected_pc,
                                     uint64_t expected_cycle,
                                     uint64_t expected_instret,
                                     uint32_t reg,
                                     uint64_t expected_value) {
    return expect(machine.cpu().core().pc() == expected_pc,
                  "set_gpr should not reset the current pc") &&
           expect(machine.cpu().core().cycle() == expected_cycle,
                  "set_gpr should preserve cycle count") &&
           expect(machine.cpu().core().instret() == expected_instret,
                  "set_gpr should preserve instret") &&
           expect(machine.cpu().core().read_gpr(reg) == expected_value,
                  "set_gpr should update the requested architectural register");
}

bool expect_payload_load_invalidates_enabled_l1d(Machine& machine,
                                                 const std::string& payload_path,
                                                 uint64_t addr,
                                                 uint16_t expected_value) {
    machine.set_l1_data_cache_enabled(true);

    const AddressSpace::AccessResult first_load =
        machine.cpu().address_space().load_result(machine.bus(), addr, 2);
    if (!expect(first_load.ok, "initial L1D data load should succeed") ||
        !expect(first_load.value == 0xBBAA,
                "initial L1D data load should observe the original binary bytes") ||
        !expect(machine.cpu().l1_data_cache().stats().misses == 1,
                "initial L1D data load should populate one cache line")) {
        return false;
    }

    machine.load_binary_payload(payload_path, addr);

    const AddressSpace::AccessResult second_load =
        machine.cpu().address_space().load_result(machine.bus(), addr, 2);
    return expect(second_load.ok, "post-payload L1D data load should succeed") &&
           expect(second_load.value == expected_value,
                  "post-payload L1D data load should observe payload bytes") &&
           expect(machine.cpu().l1_data_cache().stats().misses == 2,
                  "payload overwrite should invalidate the cached line before the next load");
}

bool expect_stale_store_conditional_fails(Machine& machine,
                                          uint64_t addr,
                                          uint32_t attempted_value,
                                          uint16_t expected_memory,
                                          const char* context) {
    const AtomicApplyResult sc_result =
        apply_atomic_request(machine.cpu(),
                             machine.bus(),
                             AtomicRequest{
                                 .kind = AtomicRequest::Kind::StoreConditional,
                                 .addr = addr,
                                 .store_value = attempted_value,
                                 .rd = 5,
                                 .size = 4,
                             });
    uint64_t memory = 0;
    return expect(sc_result.ok, context) &&
           expect(sc_result.rd_write.enable && sc_result.rd_write.value == 1,
                  "store-conditional should fail after reload clears the old reservation") &&
           expect(machine.bus().try_load(addr, 2, memory) && memory == expected_memory,
                  "failed store-conditional should not overwrite the reloaded bytes");
}

bool trigger_no_media_storage_error(Machine& machine) {
    machine.storage().clear_image();
    return expect(machine.bus().try_store(STORAGE_BASE + STORAGE_REG_COMMAND,
                                          STORAGE_CMD_READ,
                                          8),
                  "storage error setup should issue a read command after detaching media") &&
           expect((machine.storage().status() & STORAGE_STATUS_ERROR) != 0,
                  "storage error setup should raise sticky error status before reload") &&
           expect(machine.storage().error_code() == STORAGE_ERR_NO_MEDIA,
                  "storage error setup should raise no-media before reload");
}

}  // namespace

int main() {
    try {
        const std::array<uint8_t, 4> first_bytes = {0xAA, 0xBB, 0xCC, 0xDD};
        const std::array<uint8_t, 4> second_bytes = {0x11, 0x22, 0x00, 0x00};

        const std::string first_binary = write_temp_binary(first_bytes, 4);
        const std::string second_binary = write_temp_binary(second_bytes, 2);
        const std::string first_elf = write_temp_elf(first_bytes, 4, kElfAddr);
        const std::string second_elf = write_temp_elf(second_bytes, 2, kElfAddr);
        const std::string storage_image = write_temp_storage_image();

        {
            Machine machine;
            machine.attach_storage_image(storage_image);
            machine.load_binary(first_binary, kBinaryAddr);
            machine.cpu().core().write_gpr(1, 0xDEADBEEF);
            machine.cpu().core().set_cycle(17);
            machine.cpu().core().set_instret(9);
            machine.cpu().core().set_halted(true);
            machine.cpu().core().set_privilege_mode(PrivilegeMode::Supervisor);
            if (!trigger_no_media_storage_error(machine)) {
                std::remove(first_binary.c_str());
                std::remove(second_binary.c_str());
                std::remove(first_elf.c_str());
                std::remove(second_elf.c_str());
                std::remove(storage_image.c_str());
                return 1;
            }

            machine.load_binary(second_binary, kBinaryAddr);
            if (!expect_core_reset(machine, kBinaryAddr) ||
                !expect_loaded_bytes(machine, kBinaryAddr, second_bytes[0], second_bytes[1]) ||
                !expect_storage_error_cleared(machine)) {
                std::remove(first_binary.c_str());
                std::remove(second_binary.c_str());
                std::remove(first_elf.c_str());
                std::remove(second_elf.c_str());
                std::remove(storage_image.c_str());
                return 1;
            }
        }

        {
            Machine machine;
            machine.attach_storage_image(storage_image);
            machine.load_elf(first_elf);
            machine.cpu().core().write_gpr(1, 0x1234);
            machine.cpu().core().set_cycle(21);
            machine.cpu().core().set_instret(5);
            machine.cpu().core().set_halted(true);
            machine.cpu().core().set_privilege_mode(PrivilegeMode::Supervisor);
            if (!trigger_no_media_storage_error(machine)) {
                std::remove(first_binary.c_str());
                std::remove(second_binary.c_str());
                std::remove(first_elf.c_str());
                std::remove(second_elf.c_str());
                std::remove(storage_image.c_str());
                return 1;
            }

            machine.load_elf(second_elf);
            if (!expect_core_reset(machine, kElfAddr) ||
                !expect_loaded_bytes(machine, kElfAddr, second_bytes[0], second_bytes[1]) ||
                !expect_storage_error_cleared(machine)) {
                std::remove(first_binary.c_str());
                std::remove(second_binary.c_str());
                std::remove(first_elf.c_str());
                std::remove(second_elf.c_str());
                std::remove(storage_image.c_str());
                return 1;
            }
        }

        {
            Machine machine;
            machine.load_elf(first_elf);
            machine.cpu().core().set_cycle(33);
            machine.cpu().core().set_instret(7);
            machine.load_binary_payload(second_binary, kBinaryAddr);
            if (!expect_payload_load_preserves_state(machine,
                                                     kElfAddr,
                                                     33,
                                                     7,
                                                     kBinaryAddr,
                                                     second_bytes[0],
                                                     second_bytes[1])) {
                std::remove(first_binary.c_str());
                std::remove(second_binary.c_str());
                std::remove(first_elf.c_str());
                std::remove(second_elf.c_str());
                std::remove(storage_image.c_str());
                return 1;
            }
        }

        {
            Machine machine;
            machine.load_binary(first_binary, kBinaryAddr);
            machine.cpu().trap().set_reservation(kBinaryAddr, 4);
            machine.load_binary(second_binary, kBinaryAddr);
            if (!expect_stale_store_conditional_fails(machine,
                                                       kBinaryAddr,
                                                       0xA5A5A5A5U,
                                                       0x2211,
                                                       "store-conditional apply should complete after image reload")) {
                std::remove(first_binary.c_str());
                std::remove(second_binary.c_str());
                std::remove(first_elf.c_str());
                std::remove(second_elf.c_str());
                std::remove(storage_image.c_str());
                return 1;
            }
        }

        {
            Machine machine;
            machine.load_binary(first_binary, kBinaryAddr);
            machine.cpu().trap().set_reservation(kBinaryAddr, 4);
            machine.load_binary_payload(second_binary, kBinaryAddr);
            if (!expect_stale_store_conditional_fails(machine,
                                                       kBinaryAddr,
                                                       0x5A5A5A5AU,
                                                       0x2211,
                                                       "store-conditional apply should complete after payload load")) {
                std::remove(first_binary.c_str());
                std::remove(second_binary.c_str());
                std::remove(first_elf.c_str());
                std::remove(second_elf.c_str());
                std::remove(storage_image.c_str());
                return 1;
            }
        }

        {
            Machine machine;
            machine.load_binary(first_binary, kBinaryAddr);
            if (!expect_payload_load_invalidates_enabled_l1d(machine,
                                                             second_binary,
                                                             kBinaryAddr,
                                                             0x2211)) {
                std::remove(first_binary.c_str());
                std::remove(second_binary.c_str());
                std::remove(first_elf.c_str());
                std::remove(second_elf.c_str());
                std::remove(storage_image.c_str());
                return 1;
            }
        }

        {
            Machine machine;
            machine.load_elf(first_elf);
            machine.cpu().core().set_cycle(41);
            machine.cpu().core().set_instret(11);
            machine.set_gpr("a1", 0x88000000ULL);
            if (!expect_gpr_seed_preserves_state(machine,
                                                 kElfAddr,
                                                 41,
                                                 11,
                                                 11,
                                                 0x88000000ULL) ||
                !expect(machine.cpu().core().read_gpr(0) == 0,
                        "set_gpr should not clobber x0")) {
                std::remove(first_binary.c_str());
                std::remove(second_binary.c_str());
                std::remove(first_elf.c_str());
                std::remove(second_elf.c_str());
                std::remove(storage_image.c_str());
                return 1;
            }

            machine.set_gpr("x0", 0xDEADBEEFULL);
            if (!expect(machine.cpu().core().read_gpr(0) == 0,
                        "set_gpr should keep x0 hard-wired to zero")) {
                std::remove(first_binary.c_str());
                std::remove(second_binary.c_str());
                std::remove(first_elf.c_str());
                std::remove(second_elf.c_str());
                std::remove(storage_image.c_str());
                return 1;
            }
        }

        std::remove(first_binary.c_str());
        std::remove(second_binary.c_str());
        std::remove(first_elf.c_str());
        std::remove(second_elf.c_str());
        std::remove(storage_image.c_str());
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
