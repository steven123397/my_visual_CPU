#include "storage.h"

#include "platform.h"

uint64_t storage_read_block(uint64_t lba, void* destination) {
    return platform_storage_read_block(lba, destination);
}
