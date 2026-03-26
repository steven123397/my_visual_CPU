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
constexpr uint32_t kProgramHeaderLoad = 1;
constexpr uint64_t kSegmentAddr = MEM_BASE + 0x4000;
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

Elf64Ehdr make_ehdr() {
    Elf64Ehdr ehdr{};
    std::memcpy(ehdr.e_ident, &kElfMagic, sizeof(kElfMagic));
    ehdr.e_ident[4] = kElfClass64;
    ehdr.e_entry = kSegmentAddr;
    ehdr.e_phoff = sizeof(Elf64Ehdr);
    ehdr.e_ehsize = sizeof(Elf64Ehdr);
    ehdr.e_phentsize = sizeof(Elf64Phdr);
    ehdr.e_phnum = 1;
    return ehdr;
}

std::string create_temp_path() {
    char path_template[] = "/tmp/mycpu_elf_rejects_XXXXXX";
    const int fd = mkstemp(path_template);
    if (fd < 0) {
        throw std::runtime_error("failed to create temp ELF");
    }
    close(fd);
    return path_template;
}

std::string write_test_elf(const Elf64Phdr& phdr,
                           const void* segment_data,
                           size_t segment_size) {
    const std::string path = create_temp_path();
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::remove(path.c_str());
        throw std::runtime_error("failed to open temp ELF for writing");
    }

    const Elf64Ehdr ehdr = make_ehdr();
    file.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
    file.write(reinterpret_cast<const char*>(&phdr), sizeof(phdr));
    if (segment_data != nullptr && segment_size != 0) {
        file.seekp(static_cast<std::streamoff>(phdr.p_offset), std::ios::beg);
        file.write(reinterpret_cast<const char*>(segment_data),
                   static_cast<std::streamsize>(segment_size));
    }
    file.close();
    return path;
}

bool expect_load_failure(const Elf64Phdr& phdr,
                         const void* segment_data,
                         size_t segment_size,
                         const char* expected_message) {
    const std::string path = write_test_elf(phdr, segment_data, segment_size);
    Ram ram;
    ram.fill(kSegmentAddr, 0xAA, 16);

    bool threw = false;
    try {
        ElfLoader loader;
        (void)loader.load(ram, path.c_str());
    } catch (const std::runtime_error& ex) {
        threw = true;
        if (std::string(ex.what()).find(expected_message) == std::string::npos) {
            std::fprintf(stderr,
                         "unexpected error: expected '%s', got '%s'\n",
                         expected_message,
                         ex.what());
            std::remove(path.c_str());
            return false;
        }
    }

    std::remove(path.c_str());
    if (!threw) {
        std::fprintf(stderr, "expected loader to reject ELF: %s\n", expected_message);
        return false;
    }
    for (uint64_t i = 0; i < 16; ++i) {
        if (ram.load(kSegmentAddr + i, 1) != 0xAA) {
            std::fprintf(stderr, "loader modified RAM despite rejecting ELF\n");
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    try {
        const uint8_t segment_bytes[8] = {0, 1, 2, 3, 4, 5, 6, 7};

        {
            Elf64Phdr phdr{};
            phdr.p_type = kProgramHeaderLoad;
            phdr.p_offset = kSegmentOffset;
            phdr.p_vaddr = kSegmentAddr;
            phdr.p_paddr = kSegmentAddr;
            phdr.p_filesz = 16;
            phdr.p_memsz = 8;
            phdr.p_align = 0x1000;
            if (!expect_load_failure(phdr,
                                     segment_bytes,
                                     sizeof(segment_bytes),
                                     "ELF segment file size exceeds memory size")) {
                return 1;
            }
        }

        {
            Elf64Phdr phdr{};
            phdr.p_type = kProgramHeaderLoad;
            phdr.p_offset = kSegmentOffset;
            phdr.p_vaddr = MEM_BASE + MEM_SIZE - 4;
            phdr.p_paddr = MEM_BASE + MEM_SIZE - 4;
            phdr.p_filesz = 4;
            phdr.p_memsz = 8;
            phdr.p_align = 0x1000;
            if (!expect_load_failure(phdr,
                                     segment_bytes,
                                     sizeof(segment_bytes),
                                     "segment out of memory range")) {
                return 1;
            }
        }

        {
            Elf64Phdr phdr{};
            phdr.p_type = kProgramHeaderLoad;
            phdr.p_offset = kSegmentOffset;
            phdr.p_vaddr = kSegmentAddr;
            phdr.p_paddr = kSegmentAddr;
            phdr.p_filesz = 8;
            phdr.p_memsz = 8;
            phdr.p_align = 0x1000;
            if (!expect_load_failure(phdr,
                                     segment_bytes,
                                     4,
                                     "failed to read ELF segment")) {
                return 1;
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
