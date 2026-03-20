#include "elf_loader.h"

#include "../mem/ram.h"

extern "C" {
uint64_t elf_load(Memory* mem, const char* path);
}

uint64_t ElfLoader::load(Ram& ram, const char* path) const {
    return elf_load(&ram.mem_, path);
}
