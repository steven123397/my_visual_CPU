#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

constexpr uint64_t kTextAddr = MEM_BASE + 0x3000;
constexpr uint64_t kTextFileSize = 8;
constexpr uint64_t kTextMemSize = 16;
constexpr uint64_t kTextOffset = 0x100;

constexpr uint64_t kDataAddr = MEM_BASE + 0x5000;
constexpr uint64_t kDataFileSize = 6;
constexpr uint64_t kDataMemSize = 12;
constexpr uint64_t kDataOffset = 0x180;

constexpr std::array<uint8_t, kTextFileSize> kTextBytes{
    0x13, 0x00, 0x00, 0x00, 0x97, 0x00, 0x00, 0x00,
};
constexpr std::array<uint8_t, kDataFileSize> kDataBytes{
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
};

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

std::string create_segmented_test_elf() {
    char path_template[] = "/tmp/mycpu_elf_segments_XXXXXX";
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
    ehdr.e_entry = kTextAddr;
    ehdr.e_phoff = sizeof(Elf64Ehdr);
    ehdr.e_ehsize = sizeof(Elf64Ehdr);
    ehdr.e_phentsize = sizeof(Elf64Phdr);
    ehdr.e_phnum = 2;

    Elf64Phdr text{};
    text.p_type = kProgramHeaderLoad;
    text.p_offset = kTextOffset;
    text.p_vaddr = kTextAddr;
    text.p_paddr = kTextAddr;
    text.p_filesz = kTextFileSize;
    text.p_memsz = kTextMemSize;
    text.p_align = 0x1000;

    Elf64Phdr data{};
    data.p_type = kProgramHeaderLoad;
    data.p_offset = kDataOffset;
    data.p_vaddr = kDataAddr;
    data.p_paddr = kDataAddr;
    data.p_filesz = kDataFileSize;
    data.p_memsz = kDataMemSize;
    data.p_align = 0x1000;

    std::ofstream file(path_template, std::ios::binary);
    if (!file) {
        std::remove(path_template);
        throw std::runtime_error("failed to open temp ELF for writing");
    }

    file.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
    file.write(reinterpret_cast<const char*>(&text), sizeof(text));
    file.write(reinterpret_cast<const char*>(&data), sizeof(data));

    file.seekp(static_cast<std::streamoff>(kTextOffset), std::ios::beg);
    file.write(reinterpret_cast<const char*>(kTextBytes.data()),
               static_cast<std::streamsize>(kTextBytes.size()));

    file.seekp(static_cast<std::streamoff>(kDataOffset), std::ios::beg);
    file.write(reinterpret_cast<const char*>(kDataBytes.data()),
               static_cast<std::streamsize>(kDataBytes.size()));
    file.close();

    return path_template;
}

bool expect_byte(Ram& ram, uint64_t addr, uint8_t expected, const char* message) {
    if (ram.load(addr, 1) != expected) {
        std::fprintf(stderr,
                     "%s at 0x%llx: expected 0x%02x, got 0x%02llx\n",
                     message,
                     static_cast<unsigned long long>(addr),
                     expected,
                     static_cast<unsigned long long>(ram.load(addr, 1)));
        return false;
    }
    return true;
}

bool expect_zero_filled(Ram& ram, uint64_t addr, uint64_t size, const char* message) {
    for (uint64_t i = 0; i < size; ++i) {
        if (!expect_byte(ram, addr + i, 0, message)) {
            return false;
        }
    }
    return true;
}

bool expect_bytes(Ram& ram,
                  uint64_t addr,
                  const uint8_t* data,
                  uint64_t size,
                  const char* message) {
    for (uint64_t i = 0; i < size; ++i) {
        if (!expect_byte(ram, addr + i, data[i], message)) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    try {
        const std::string path = create_segmented_test_elf();

        Ram ram;
        ram.fill(kTextAddr - 1, 0xAA, 1);
        ram.fill(kTextAddr, 0xAA, kTextMemSize);
        ram.fill(kTextAddr + kTextMemSize, 0xAA, 16);
        ram.fill(kDataAddr - 1, 0xAA, 1);
        ram.fill(kDataAddr, 0xAA, kDataMemSize);
        ram.fill(kDataAddr + kDataMemSize, 0xAA, 16);

        ElfLoader loader;
        const uint64_t entry = loader.load(ram, path.c_str());
        std::remove(path.c_str());

        if (entry != kTextAddr) {
            std::fprintf(stderr,
                         "unexpected ELF entry: 0x%llx\n",
                         static_cast<unsigned long long>(entry));
            return 1;
        }

        if (!expect_bytes(ram, kTextAddr, kTextBytes.data(), kTextBytes.size(), "text bytes mismatch") ||
            !expect_zero_filled(ram,
                                kTextAddr + kTextFileSize,
                                kTextMemSize - kTextFileSize,
                                "text BSS tail should be zero-filled") ||
            !expect_byte(ram, kTextAddr - 1, 0xAA, "byte before text segment should remain untouched") ||
            !expect_byte(ram,
                         kTextAddr + kTextMemSize,
                         0xAA,
                         "byte after text segment should remain untouched") ||
            !expect_bytes(ram, kDataAddr, kDataBytes.data(), kDataBytes.size(), "data bytes mismatch") ||
            !expect_zero_filled(ram,
                                kDataAddr + kDataFileSize,
                                kDataMemSize - kDataFileSize,
                                "data BSS tail should be zero-filled") ||
            !expect_byte(ram, kDataAddr - 1, 0xAA, "byte before data segment should remain untouched") ||
            !expect_byte(ram,
                         kDataAddr + kDataMemSize,
                         0xAA,
                         "byte after data segment should remain untouched")) {
            return 1;
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
