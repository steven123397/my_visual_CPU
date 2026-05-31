#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "linux_compat.h"

typedef struct LinuxCompatRootfsNode {
    const char* path;
    const uint8_t* data;
    size_t size;
    bool executable;
    bool directory;
    uint64_t inode;
    uint32_t mode;
} linux_compat_rootfs_node_t;

const char* linux_compat_rootfs_source_name(void);
size_t linux_compat_rootfs_node_count(void);
const linux_compat_rootfs_node_t* linux_compat_rootfs_node_at(size_t index);
