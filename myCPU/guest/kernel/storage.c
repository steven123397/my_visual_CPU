#include <stddef.h>
#include <stdbool.h>

#include "storage.h"

#include "platform.h"

bool storage_read_info(storage_info_t* info) {
    if (info == NULL) {
        return false;
    }

    info->magic = platform_storage_read_u64(STORAGE_REG_MAGIC);
    info->version = platform_storage_read_u64(STORAGE_REG_VERSION);
    info->block_size = platform_storage_read_u64(STORAGE_REG_BLOCK_SIZE);
    info->capacity_blocks = platform_storage_read_u64(STORAGE_REG_CAPACITY_BLOCKS);
    info->status = platform_storage_read_u64(STORAGE_REG_STATUS);

    return info->magic == STORAGE_MMIO_MAGIC &&
           info->version == STORAGE_MMIO_VERSION &&
           info->block_size == STORAGE_BLOCK_SIZE;
}

bool storage_probe(storage_info_t* info) {
    storage_info_t local_info = {0};
    storage_info_t* target = info != NULL ? info : &local_info;

    if (!storage_read_info(target) ||
        target->capacity_blocks == 0 ||
        (target->status & (STORAGE_STATUS_ATTACHED | STORAGE_STATUS_READY)) !=
            (STORAGE_STATUS_ATTACHED | STORAGE_STATUS_READY) ||
        (target->status & STORAGE_STATUS_ERROR) != 0) {
        return false;
    }

    return true;
}

uint64_t storage_status(void) {
    return platform_storage_read_status();
}

uint64_t storage_error(void) {
    return platform_storage_read_error();
}

void storage_clear_error(void) {
    platform_storage_issue_command(STORAGE_CMD_NONE);
}

uint64_t storage_read_block(uint64_t lba, void* destination) {
    return platform_storage_read_block(lba, destination);
}

uint64_t storage_read_block_with_count(uint64_t lba,
                                       uint64_t block_count,
                                       void* destination) {
    return platform_storage_read_block_custom(lba, block_count, destination);
}
