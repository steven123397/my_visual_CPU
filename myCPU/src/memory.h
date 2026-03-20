#pragma once
#include <stdint.h>
#include <stddef.h>

#include "platform/address_map.h"

typedef struct Memory {
    uint8_t *data;
} Memory;

void mem_init(Memory *mem);
void mem_free(Memory *mem);
uint64_t mem_read(Memory *mem, uint64_t addr, int size);
void mem_write(Memory *mem, uint64_t addr, uint64_t val, int size);
void mem_write_range(Memory *mem, uint64_t addr, const void *data, size_t size);
void mem_fill_range(Memory *mem, uint64_t addr, uint8_t value, size_t size);
