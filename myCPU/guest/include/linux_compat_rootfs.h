#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "linux_compat.h"

/* Linux compat rootfs catalog：内置或生成资产都通过同一只读节点接口暴露。 */
typedef struct LinuxCompatRootfsNode {
    const char* path;
    const uint8_t* data;
    size_t size;
    bool executable;
    bool directory;
    uint64_t inode;
    uint32_t mode;
} linux_compat_rootfs_node_t;

/* 返回当前 rootfs 来源名（builtin / external / generated）。 */
const char* linux_compat_rootfs_source_name(void);
/* 返回 rootfs 节点总数。 */
size_t linux_compat_rootfs_node_count(void);
/* 按索引取只读 rootfs 节点。 */
const linux_compat_rootfs_node_t* linux_compat_rootfs_node_at(size_t index);
