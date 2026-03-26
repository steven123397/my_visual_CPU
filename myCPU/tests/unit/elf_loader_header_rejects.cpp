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
constexpr uint64_t kProbeAddr = MEM_BASE + 0x6000;

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

Elf64Ehdr make_valid_ehdr() {
    Elf64Ehdr ehdr{};
    std::memcpy(ehdr.e_ident, &kElfMagic, sizeof(kElfMagic));
    ehdr.e_ident[4] = kElfClass64;
    ehdr.e_entry = MEM_BASE;
    ehdr.e_phoff = sizeof(Elf64Ehdr);
    ehdr.e_ehsize = sizeof(Elf64Ehdr);
    ehdr.e_phentsize = sizeof(Elf64Phdr);
    ehdr.e_phnum = 1;
    return ehdr;
}

std::string create_temp_path() {
    char path_template[] = "/tmp/mycpu_elf_header_rejects_XXXXXX";
    const int fd = mkstemp(path_template);
    if (fd < 0) {
        throw std::runtime_error("failed to create temp ELF");
    }
    close(fd);
    return path_template;
}

bool expect_loader_failure(const std::string& path, const char* expected_message) {
    Ram ram;
    ram.fill(kProbeAddr, 0xAA, 16);

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
        std::fprintf(stderr, "expected loader failure containing '%s'\n", expected_message);
        return false;
    }
    for (uint64_t i = 0; i < 16; ++i) {
        if (ram.load(kProbeAddr + i, 1) != 0xAA) {
            std::fprintf(stderr, "loader modified RAM while rejecting malformed ELF\n");
            return false;
        }
    }
    return true;
}

template <typename Fn>
bool write_case_and_expect_failure(Fn&& writer, const char* expected_message) {
    const std::string path = create_temp_path();
    {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            std::remove(path.c_str());
            throw std::runtime_error("failed to open temp ELF for writing");
        }
        writer(file);
    }
    return expect_loader_failure(path, expected_message);
}

}  // namespace

int main() {
    try {
        if (!write_case_and_expect_failure(
                [](std::ofstream& file) {
                    const uint8_t bytes[8] = {0};
                    file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
                },
                "failed to read ELF header")) {
            return 1;
        }

        if (!write_case_and_expect_failure(
                [](std::ofstream& file) {
                    Elf64Ehdr ehdr = make_valid_ehdr();
                    ehdr.e_ident[0] = 0;
                    file.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
                },
                "not an ELF file")) {
            return 1;
        }

        if (!write_case_and_expect_failure(
                [](std::ofstream& file) {
                    Elf64Ehdr ehdr = make_valid_ehdr();
                    ehdr.e_ident[4] = 1;
                    file.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
                },
                "not ELF64")) {
            return 1;
        }

        if (!write_case_and_expect_failure(
                [](std::ofstream& file) {
                    Elf64Ehdr ehdr = make_valid_ehdr();
                    ehdr.e_ehsize = sizeof(Elf64Ehdr) - 1;
                    file.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
                },
                "unexpected ELF header size")) {
            return 1;
        }

        if (!write_case_and_expect_failure(
                [](std::ofstream& file) {
                    Elf64Ehdr ehdr = make_valid_ehdr();
                    ehdr.e_phentsize = sizeof(Elf64Phdr) - 8;
                    file.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
                },
                "unexpected program header size")) {
            return 1;
        }

        if (!write_case_and_expect_failure(
                [](std::ofstream& file) {
                    const Elf64Ehdr ehdr = make_valid_ehdr();
                    file.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
                },
                "failed to read ELF program header")) {
            return 1;
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
