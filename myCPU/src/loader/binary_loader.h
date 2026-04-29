#pragma once

#include <cstdint>

class Ram;

class BinaryLoader {
public:
    uint64_t load(Ram& ram, const char* path, uint64_t addr) const;
};
