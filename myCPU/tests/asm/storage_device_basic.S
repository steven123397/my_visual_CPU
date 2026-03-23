# storage_device_basic.S - 验证块化 MMIO 存储设备的 read/write/error 路径
#include "../../include/platform_mmio.h"
.section .text
.global _start
_start:
    li   s0, UART_BASE
    li   s1, STORAGE_BASE

    ld   t0, STORAGE_REG_MAGIC(s1)
    li   t1, STORAGE_MMIO_MAGIC
    bne  t0, t1, fail

    ld   t0, STORAGE_REG_VERSION(s1)
    li   t1, STORAGE_MMIO_VERSION
    bne  t0, t1, fail

    ld   t0, STORAGE_REG_CAPACITY_BLOCKS(s1)
    li   t1, 1
    bne  t0, t1, fail

    ld   t0, STORAGE_REG_BLOCK_SIZE(s1)
    li   t1, STORAGE_BLOCK_SIZE
    bne  t0, t1, fail

    ld   t0, STORAGE_REG_STATUS(s1)
    li   t1, STORAGE_STATUS_READY | STORAGE_STATUS_ATTACHED
    bne  t0, t1, fail

    sd   zero, STORAGE_REG_LBA(s1)
    li   t0, 1
    sd   t0, STORAGE_REG_BLOCK_COUNT(s1)
    li   t0, STORAGE_CMD_READ
    sd   t0, STORAGE_REG_COMMAND(s1)

    ld   t0, STORAGE_REG_STATUS(s1)
    li   t1, STORAGE_STATUS_READY | STORAGE_STATUS_ATTACHED
    bne  t0, t1, fail

    addi t2, s1, STORAGE_DATA_WINDOW_OFFSET
    lb   t0, 0(t2)
    li   t1, 'S'
    bne  t0, t1, fail
    lb   t0, 1(t2)
    li   t1, 't'
    bne  t0, t1, fail
    lb   t0, 2(t2)
    li   t1, 'o'
    bne  t0, t1, fail
    lb   t0, 3(t2)
    li   t1, 'r'
    bne  t0, t1, fail

    li   t0, '!'
    sb   t0, 15(t2)
    sd   zero, STORAGE_REG_LBA(s1)
    li   t0, 1
    sd   t0, STORAGE_REG_BLOCK_COUNT(s1)
    li   t0, STORAGE_CMD_WRITE
    sd   t0, STORAGE_REG_COMMAND(s1)

    ld   t0, STORAGE_REG_STATUS(s1)
    li   t1, STORAGE_STATUS_READY | STORAGE_STATUS_ATTACHED
    bne  t0, t1, fail

    sd   zero, STORAGE_REG_LBA(s1)
    li   t0, 1
    sd   t0, STORAGE_REG_BLOCK_COUNT(s1)
    li   t0, STORAGE_CMD_READ
    sd   t0, STORAGE_REG_COMMAND(s1)
    lb   t0, 15(t2)
    li   t1, '!'
    bne  t0, t1, fail

    li   t0, 1
    sd   t0, STORAGE_REG_LBA(s1)
    li   t0, 1
    sd   t0, STORAGE_REG_BLOCK_COUNT(s1)
    li   t0, STORAGE_CMD_READ
    sd   t0, STORAGE_REG_COMMAND(s1)

    ld   t0, STORAGE_REG_STATUS(s1)
    li   t1, STORAGE_STATUS_READY | STORAGE_STATUS_ATTACHED | STORAGE_STATUS_ERROR
    bne  t0, t1, fail

    ld   t0, STORAGE_REG_ERROR(s1)
    li   t1, STORAGE_ERR_LBA_RANGE
    bne  t0, t1, fail

    li   t0, STORAGE_CMD_NONE
    sd   t0, STORAGE_REG_COMMAND(s1)
    ld   t0, STORAGE_REG_STATUS(s1)
    li   t1, STORAGE_STATUS_READY | STORAGE_STATUS_ATTACHED
    bne  t0, t1, fail
    ld   t0, STORAGE_REG_ERROR(s1)
    bnez t0, fail

    li   t0, 'D'
    sb   t0, UART_REG_THR(s0)
    li   t0, 'S'
    sb   t0, UART_REG_THR(s0)
    li   t0, 'K'
    sb   t0, UART_REG_THR(s0)

    li   a7, 93
    li   a0, 0
    ecall

fail:
    li   t0, 'X'
    sb   t0, UART_REG_THR(s0)
    li   a7, 93
    li   a0, 1
    ecall
