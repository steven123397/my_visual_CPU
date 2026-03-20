#include "elf_loader.h"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

#include "../mem/ram.h"
#include "../platform/address_map.h"

namespace {

constexpr uint16_t kElfClass64 = 2;
constexpr uint32_t kElfMagic = 0x464C457F;
constexpr uint32_t kProgramHeaderLoad = 1;

struct Elf64_Ehdr {
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

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

static_assert(sizeof(Elf64_Ehdr) == 64, "Elf64_Ehdr layout mismatch");
static_assert(sizeof(Elf64_Phdr) == 56, "Elf64_Phdr layout mismatch");

template <typename T>
void read_exact(std::ifstream& file, T& value, const char* what, const char* path) {
    if (!file.read(reinterpret_cast<char*>(&value), sizeof(T))) {
        throw std::runtime_error(std::string("failed to read ") + what + ": " + path);
    }
}

}  // namespace

uint64_t ElfLoader::load(Ram& ram, const char* path) const {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::string("failed to open ELF: ") + path);
    }

    Elf64_Ehdr ehdr{};
    read_exact(file, ehdr, "ELF header", path);

    uint32_t magic = 0;
    std::memcpy(&magic, ehdr.e_ident, sizeof(magic));
    if (magic != kElfMagic) {
        throw std::runtime_error("not an ELF file");
    }
    if (ehdr.e_ident[4] != kElfClass64) {
        throw std::runtime_error("not ELF64");
    }
    if (ehdr.e_ehsize != sizeof(Elf64_Ehdr)) {
        throw std::runtime_error("unexpected ELF header size");
    }
    if (ehdr.e_phentsize != sizeof(Elf64_Phdr)) {
        throw std::runtime_error("unexpected program header size");
    }

    const uint64_t memory_end = MEM_BASE + MEM_SIZE;
    for (uint16_t i = 0; i < ehdr.e_phnum; ++i) {
        file.seekg(static_cast<std::streamoff>(ehdr.e_phoff + static_cast<uint64_t>(i) * ehdr.e_phentsize), std::ios::beg);
        if (!file) {
            throw std::runtime_error(std::string("failed to seek to program header: ") + path);
        }

        Elf64_Phdr phdr{};
        read_exact(file, phdr, "ELF program header", path);

        if (phdr.p_type != kProgramHeaderLoad || phdr.p_filesz == 0) {
            continue;
        }
        if (phdr.p_filesz > phdr.p_memsz) {
            throw std::runtime_error("ELF segment file size exceeds memory size");
        }
        if (phdr.p_paddr < MEM_BASE || phdr.p_paddr > memory_end || phdr.p_memsz > memory_end - phdr.p_paddr) {
            throw std::runtime_error("segment out of memory range");
        }

        std::string segment(static_cast<size_t>(phdr.p_filesz), '\0');
        file.seekg(static_cast<std::streamoff>(phdr.p_offset), std::ios::beg);
        if (!file) {
            throw std::runtime_error(std::string("failed to seek to ELF segment: ") + path);
        }
        if (!file.read(segment.data(), static_cast<std::streamsize>(segment.size()))) {
            throw std::runtime_error(std::string("failed to read ELF segment: ") + path);
        }

        ram.write_bytes(phdr.p_paddr, segment.data(), segment.size());
        if (phdr.p_memsz > phdr.p_filesz) {
            ram.fill(phdr.p_paddr + phdr.p_filesz, 0, static_cast<size_t>(phdr.p_memsz - phdr.p_filesz));
        }
    }

    return ehdr.e_entry;
}
