#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "../../src/loader/elf_loader.h"
#include "../../src/mem/ram.h"
#include "../../src/platform/address_map.h"

namespace {

constexpr uint32_t kElfMagic = 0x464C457F;
constexpr uint16_t kElfClass64 = 2;
constexpr uint8_t kElfDataLittleEndian = 1;
constexpr uint8_t kElfIdentVersionCurrent = 1;
constexpr uint16_t kElfTypeExec = 2;
constexpr uint16_t kElfMachineRiscv = 243;
constexpr uint32_t kElfVersionCurrent = 1;
constexpr uint32_t kProgramHeaderLoad = 1;
constexpr uint64_t kSegmentAddr = MEM_BASE + 0x2000;
constexpr uint64_t kSegmentSize = 32;

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

std::string create_test_elf() {
    char path_template[] = "/tmp/mycpu_elf_bss_XXXXXX";
    const int fd = mkstemp(path_template);
    if (fd < 0) {
        throw std::runtime_error("failed to create temp ELF");
    }
    close(fd);

    Elf64Ehdr ehdr{};
    std::memcpy(ehdr.e_ident, &kElfMagic, sizeof(kElfMagic));
    ehdr.e_ident[4] = kElfClass64;
    ehdr.e_ident[5] = kElfDataLittleEndian;
    ehdr.e_ident[6] = kElfIdentVersionCurrent;
    ehdr.e_type = kElfTypeExec;
    ehdr.e_machine = kElfMachineRiscv;
    ehdr.e_version = kElfVersionCurrent;
    ehdr.e_entry = kSegmentAddr;
    ehdr.e_phoff = sizeof(Elf64Ehdr);
    ehdr.e_ehsize = sizeof(Elf64Ehdr);
    ehdr.e_phentsize = sizeof(Elf64Phdr);
    ehdr.e_phnum = 1;

    Elf64Phdr phdr{};
    phdr.p_type = kProgramHeaderLoad;
    phdr.p_vaddr = kSegmentAddr;
    phdr.p_paddr = kSegmentAddr;
    phdr.p_filesz = 0;
    phdr.p_memsz = kSegmentSize;
    phdr.p_align = 0x1000;

    std::ofstream file(path_template, std::ios::binary);
    if (!file) {
        std::remove(path_template);
        throw std::runtime_error("failed to open temp ELF for writing");
    }

    file.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
    file.write(reinterpret_cast<const char*>(&phdr), sizeof(phdr));
    file.close();

    return path_template;
}

void assert_zero_filled(Ram& ram, uint64_t addr, uint64_t size) {
    for (uint64_t i = 0; i < size; ++i) {
        if (ram.load(addr + i, 1) != 0) {
            std::fprintf(stderr, "expected zero-filled byte at 0x%llx\n",
                         static_cast<unsigned long long>(addr + i));
            std::exit(1);
        }
    }
}

}  // namespace

int main() {
    try {
        const std::string path = create_test_elf();

        Ram ram;
        ram.fill(kSegmentAddr, 0xAA, kSegmentSize);

        ElfLoader loader;
        const uint64_t entry = loader.load(ram, path.c_str());
        std::remove(path.c_str());

        if (entry != kSegmentAddr) {
            std::fprintf(stderr, "unexpected ELF entry: 0x%llx\n",
                         static_cast<unsigned long long>(entry));
            return 1;
        }

        assert_zero_filled(ram, kSegmentAddr, kSegmentSize);
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
