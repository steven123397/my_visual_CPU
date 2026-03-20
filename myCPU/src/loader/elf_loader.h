#pragma once

#include <cstdint>

class Ram;

class ElfLoader {
public:
    uint64_t load(Ram& ram, const char* path) const;
};
