# Minimal guest-side platform driver shim built on top of platform_mmio.h.

platform_uart_putc:
    li   t0, UART_BASE
    sb   a0, UART_REG_THR(t0)
    ret

platform_uart_enable_thre_irq:
    li   t0, UART_BASE
    li   t1, UART_IER_THRI
    sb   t1, UART_REG_IER(t0)
    ret

platform_uart_disable_irq:
    li   t0, UART_BASE
    sb   zero, UART_REG_IER(t0)
    ret

platform_plic_supervisor_init:
    li   t0, PLIC_BASE
    li   t1, 1
    li   t2, PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE)
    add  t2, t0, t2
    sw   t1, 0(t2)

    li   t1, (1 << PLIC_SOURCE_UART_THRE)
    li   t2, PLIC_ENABLE_OFFSET(PLIC_CONTEXT_SUPERVISOR)
    add  t2, t0, t2
    sw   t1, 0(t2)

    li   t2, PLIC_THRESHOLD_OFFSET(PLIC_CONTEXT_SUPERVISOR)
    add  t2, t0, t2
    sw   zero, 0(t2)
    ret

platform_plic_supervisor_claim:
    li   t0, PLIC_BASE
    li   t1, PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR)
    add  t1, t0, t1
    lw   a0, 0(t1)
    ret

platform_plic_supervisor_complete:
    li   t0, PLIC_BASE
    li   t1, PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR)
    add  t1, t0, t1
    sw   a0, 0(t1)
    ret

platform_clint_read_mtime:
    li   t0, CLINT_BASE
    li   t1, CLINT_REG_MTIME
    add  t1, t0, t1
    ld   a0, 0(t1)
    ret

platform_clint_write_mtimecmp:
    li   t0, CLINT_BASE
    li   t1, CLINT_REG_MTIMECMP
    add  t1, t0, t1
    sd   a0, 0(t1)
    ret

# a0 = lba, a1 = destination buffer, returns a0 = 0 on success or error code.
platform_storage_read_block:
    mv   t6, a1
    li   t0, STORAGE_BASE
    sd   a0, STORAGE_REG_LBA(t0)
    li   t1, 1
    sd   t1, STORAGE_REG_BLOCK_COUNT(t0)
    li   t1, STORAGE_CMD_READ
    sd   t1, STORAGE_REG_COMMAND(t0)

    ld   t1, STORAGE_REG_STATUS(t0)
    andi t1, t1, STORAGE_STATUS_ERROR
    bnez t1, platform_storage_read_block_error

    addi t2, t0, STORAGE_DATA_WINDOW_OFFSET
    li   t3, STORAGE_BLOCK_SIZE
1:
    lbu  t4, 0(t2)
    sb   t4, 0(t6)
    addi t2, t2, 1
    addi t6, t6, 1
    addi t3, t3, -1
    bnez t3, 1b

    li   a0, 0
    ret

platform_storage_read_block_error:
    ld   a0, STORAGE_REG_ERROR(t0)
    ret

platform_shutdown:
    li   a7, 93
    ecall
1:
    j    1b
