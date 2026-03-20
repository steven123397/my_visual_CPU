#pragma once

#include <cstdint>

class Ram;

class BinaryLoader {
public:
    void load(Ram& ram, const char* path, uint64_t addr) const;
};
