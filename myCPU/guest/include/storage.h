#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 平台存储设备 MMIO 封装：读取元数据、探测就绪、读写块设备。
   这里只薄封装平台寄存器，bring-up 合同与负向 demo 都依赖它。 */
typedef struct StorageInfo {
    uint64_t magic;
    uint64_t version;
    uint64_t block_size;
    uint64_t capacity_blocks;
    uint64_t status;
} storage_info_t;

/* 读取存储设备元数据（magic/version/块大小/容量/状态），并校验 magic 与版本。 */
bool storage_read_info(storage_info_t* info);
/* 探测设备是否 attached + ready + 无 error 且容量非 0，用于 bring-up 合同。 */
bool storage_probe(storage_info_t* info);
/* 读取并返回当前 status 寄存器原始值。 */
uint64_t storage_status(void);
/* 读取并返回当前 error 寄存器原始值。 */
uint64_t storage_error(void);
/* 发 COMMAND=NONE 清除粘滞 error 状态。 */
void storage_clear_error(void);
/* 按 LBA 读取单个块到 destination，返回平台结果码。 */
uint64_t storage_read_block(uint64_t lba, void* destination);
/* 按 LBA 读取 block_count 个块到 destination，返回平台结果码。 */
uint64_t storage_read_block_with_count(uint64_t lba,
                                       uint64_t block_count,
                                       void* destination);
