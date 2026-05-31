#include "linux_compat_rootfs.h"

static const uint8_t k_busybox_stub[128] = {
    [0] = 0x7f,
    [1] = 'E',
    [2] = 'L',
    [3] = 'F',
    [4] = 2,
    [5] = 1,
    [6] = 1,
    [16] = 2,
    [18] = 0xf3,
    [20] = 1,
    [24] = 0x00,
    [25] = 0x10,
    [26] = 0x40,
    [32] = 64,
    [52] = 64,
    [54] = 56,
    [56] = 1,
    [64] = 1,
};

static const uint8_t k_git_stub[128] = {
    [0] = 0x7f,
    [1] = 'E',
    [2] = 'L',
    [3] = 'F',
    [4] = 2,
    [5] = 1,
    [6] = 1,
    [16] = 2,
    [18] = 0xf3,
    [20] = 1,
    [24] = 0x00,
    [25] = 0x20,
    [26] = 0x40,
    [32] = 64,
    [52] = 64,
    [54] = 56,
    [56] = 1,
    [64] = 1,
};

static const linux_compat_rootfs_node_t k_rootfs_nodes[] = {
    {"/",
     0,
     0,
     false,
     true,
     1U,
     LINUX_COMPAT_S_IFDIR | LINUX_COMPAT_S_IRUSR | LINUX_COMPAT_S_IWUSR |
         LINUX_COMPAT_S_IXUSR | LINUX_COMPAT_S_IRGRP |
         LINUX_COMPAT_S_IXGRP | LINUX_COMPAT_S_IROTH |
         LINUX_COMPAT_S_IXOTH},
    {"/bin",
     0,
     0,
     false,
     true,
     2U,
     LINUX_COMPAT_S_IFDIR | LINUX_COMPAT_S_IRUSR | LINUX_COMPAT_S_IXUSR |
         LINUX_COMPAT_S_IRGRP | LINUX_COMPAT_S_IXGRP |
         LINUX_COMPAT_S_IROTH | LINUX_COMPAT_S_IXOTH},
    {"/usr",
     0,
     0,
     false,
     true,
     3U,
     LINUX_COMPAT_S_IFDIR | LINUX_COMPAT_S_IRUSR | LINUX_COMPAT_S_IXUSR |
         LINUX_COMPAT_S_IRGRP | LINUX_COMPAT_S_IXGRP |
         LINUX_COMPAT_S_IROTH | LINUX_COMPAT_S_IXOTH},
    {"/usr/bin",
     0,
     0,
     false,
     true,
     4U,
     LINUX_COMPAT_S_IFDIR | LINUX_COMPAT_S_IRUSR | LINUX_COMPAT_S_IXUSR |
         LINUX_COMPAT_S_IRGRP | LINUX_COMPAT_S_IXGRP |
         LINUX_COMPAT_S_IROTH | LINUX_COMPAT_S_IXOTH},
    {"/bin/busybox",
     k_busybox_stub,
     sizeof(k_busybox_stub),
     true,
     false,
     5U,
     LINUX_COMPAT_S_IFREG | LINUX_COMPAT_S_IRUSR | LINUX_COMPAT_S_IXUSR |
         LINUX_COMPAT_S_IRGRP | LINUX_COMPAT_S_IXGRP |
         LINUX_COMPAT_S_IROTH | LINUX_COMPAT_S_IXOTH},
    {"/usr/bin/git",
     k_git_stub,
     sizeof(k_git_stub),
     true,
     false,
     6U,
     LINUX_COMPAT_S_IFREG | LINUX_COMPAT_S_IRUSR | LINUX_COMPAT_S_IXUSR |
         LINUX_COMPAT_S_IRGRP | LINUX_COMPAT_S_IXGRP |
         LINUX_COMPAT_S_IROTH | LINUX_COMPAT_S_IXOTH},
};

const char* linux_compat_rootfs_source_name(void) {
    return "builtin";
}

size_t linux_compat_rootfs_node_count(void) {
    return sizeof(k_rootfs_nodes) / sizeof(k_rootfs_nodes[0]);
}

const linux_compat_rootfs_node_t* linux_compat_rootfs_node_at(size_t index) {
    if (index >= linux_compat_rootfs_node_count()) {
        return 0;
    }
    return &k_rootfs_nodes[index];
}
