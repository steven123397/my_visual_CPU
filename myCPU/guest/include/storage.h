#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct StorageInfo {
    uint64_t magic;
    uint64_t version;
    uint64_t block_size;
    uint64_t capacity_blocks;
    uint64_t status;
} storage_info_t;

bool storage_read_info(storage_info_t* info);
bool storage_probe(storage_info_t* info);
uint64_t storage_status(void);
uint64_t storage_error(void);
void storage_clear_error(void);
uint64_t storage_read_block(uint64_t lba, void* destination);
uint64_t storage_read_block_with_count(uint64_t lba,
                                       uint64_t block_count,
                                       void* destination);
