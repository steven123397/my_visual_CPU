#pragma once
#include <stdint.h>
#include <stddef.h>

#define MEM_BASE   0x80000000UL
#define MEM_SIZE   (128 * 1024 * 1024)  // 128MB
#define UART_BASE  0x10000000UL
#define UART_SIZE  8
#define CLINT_BASE 0x02000000UL
#define CLINT_SIZE 0x10000

typedef struct Memory {
    uint8_t *data;
} Memory;

void mem_init(Memory *mem);
void mem_free(Memory *mem);
uint64_t mem_read(Memory *mem, uint64_t addr, int size);
void mem_write(Memory *mem, uint64_t addr, uint64_t val, int size);
