#include "../../include/platform_mmio.h"
.option norelax
.section .text

.global platform_uart_putc
.global platform_uart_enable_thre_irq
.global platform_uart_disable_irq
.global platform_plic_supervisor_init
.global platform_plic_supervisor_claim
.global platform_plic_supervisor_complete
.global platform_clint_read_mtime
.global platform_clint_write_mtimecmp
.global platform_storage_read_block
.global platform_shutdown

#include "../include/platform_drivers.inc"
