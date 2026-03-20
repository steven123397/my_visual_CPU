#include "binary_loader.h"

#include "../mem/ram.h"

extern "C" {
#include "../memory.h"
}

void BinaryLoader::load(Ram& ram, const char* path, uint64_t addr) const {
    mem_load_binary(&ram.mem_, path, addr);
}
