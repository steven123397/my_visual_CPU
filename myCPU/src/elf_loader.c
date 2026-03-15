#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Minimal ELF64 structures
#define ET_EXEC 2
#define PT_LOAD 1
#define ELF_MAGIC "\x7f""ELF"

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags, e_ehsize;
    uint16_t e_phentsize, e_phnum;
    uint16_t e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Elf64_Phdr;

uint64_t elf_load(Memory *mem, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }

    Elf64_Ehdr ehdr;
    fread(&ehdr, sizeof(ehdr), 1, f);

    if (memcmp(ehdr.e_ident, ELF_MAGIC, 4) != 0) {
        fprintf(stderr, "Not an ELF file\n"); exit(1);
    }
    if (ehdr.e_ident[4] != 2) { // EI_CLASS != ELFCLASS64
        fprintf(stderr, "Not ELF64\n"); exit(1);
    }

    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr phdr;
        fseek(f, ehdr.e_phoff + i * ehdr.e_phentsize, SEEK_SET);
        fread(&phdr, sizeof(phdr), 1, f);
        if (phdr.p_type != PT_LOAD || phdr.p_filesz == 0) continue;

        if (phdr.p_paddr < MEM_BASE || phdr.p_paddr + phdr.p_memsz > MEM_BASE + MEM_SIZE) {
            fprintf(stderr, "Segment out of memory range: 0x%lx\n", phdr.p_paddr);
            exit(1);
        }

        fseek(f, phdr.p_offset, SEEK_SET);
        fread(mem->data + (phdr.p_paddr - MEM_BASE), 1, phdr.p_filesz, f);
        // Zero BSS
        if (phdr.p_memsz > phdr.p_filesz)
            memset(mem->data + (phdr.p_paddr - MEM_BASE) + phdr.p_filesz,
                   0, phdr.p_memsz - phdr.p_filesz);
    }

    fclose(f);
    return ehdr.e_entry;
}
