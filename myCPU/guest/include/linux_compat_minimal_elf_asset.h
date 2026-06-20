#pragma once

#include <stddef.h>
#include <stdint.h>

#define LINUX_COMPAT_MINIMAL_ELF_ASSET_SIZE 166U

/* 内置最小 RV64 ELF，服务无外部 rootfs 的 Linux compat smoke。 */
extern const uint8_t
    g_linux_compat_minimal_elf_asset[LINUX_COMPAT_MINIMAL_ELF_ASSET_SIZE];
