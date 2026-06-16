#include "../../include/platform_mmio.h"
.option norelax
.section .text

.global platform_uart_putc
.global platform_uart_rx_ready
.global platform_uart_getc
.global platform_uart_enable_thre_irq
.global platform_uart_enable_rx_irq
.global platform_uart_disable_irq
.global platform_plic_supervisor_init
.global platform_plic_supervisor_enable_source
.global platform_plic_supervisor_claim
.global platform_plic_supervisor_complete
.global platform_clint_read_mtime
.global platform_clint_write_mtimecmp
.global platform_storage_read_u64
.global platform_storage_write_u64
.global platform_storage_read_status
.global platform_storage_read_error
.global platform_storage_issue_command
.global platform_storage_read_block
.global platform_storage_read_block_custom
.global platform_shutdown

#include "../include/platform_drivers.inc"
