#pragma once

#include <stdint.h>

#include "platform_mmio.h"

void platform_uart_putc(uint8_t ch);
void platform_uart_enable_thre_irq(void);
void platform_uart_disable_irq(void);

void platform_plic_supervisor_init(void);
uint32_t platform_plic_supervisor_claim(void);
void platform_plic_supervisor_complete(uint32_t source_id);

uint64_t platform_clint_read_mtime(void);
void platform_clint_write_mtimecmp(uint64_t value);

uint64_t platform_storage_read_u64(uint64_t offset);
uint64_t platform_storage_read_block(uint64_t lba, void* destination);

void platform_shutdown(uint64_t code) __attribute__((noreturn));
