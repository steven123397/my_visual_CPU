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

static const uint8_t k_dynamic_app_stub[256] = {
    [0] = 0x7f,
    [1] = 'E',
    [2] = 'L',
    [3] = 'F',
    [4] = 2,
    [5] = 1,
    [6] = 1,
    [16] = 3,
    [18] = 0xf3,
    [20] = 1,
    [24] = 0x00,
    [25] = 0x12,
    [32] = 64,
    [52] = 64,
    [54] = 56,
    [56] = 2,
    [64] = 1,
    [68] = 5,
    [96] = 0x80,
    [104] = 0x00,
    [105] = 0x01,
    [120] = 3,
    [124] = 4,
    [128] = 0xc0,
    [152] = 26,
    [160] = 26,
    [192] = '/',
    [193] = 'l',
    [194] = 'i',
    [195] = 'b',
    [196] = '/',
    [197] = 'l',
    [198] = 'd',
    [199] = '-',
    [200] = 'm',
    [201] = 'u',
    [202] = 's',
    [203] = 'l',
    [204] = '-',
    [205] = 'r',
    [206] = 'i',
    [207] = 's',
    [208] = 'c',
    [209] = 'v',
    [210] = '6',
    [211] = '4',
    [212] = '.',
    [213] = 's',
    [214] = 'o',
    [215] = '.',
    [216] = '1',
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
    {"/usr/bin/dynamic-app",
     k_dynamic_app_stub,
     sizeof(k_dynamic_app_stub),
     true,
     false,
     7U,
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
