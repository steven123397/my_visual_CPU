# sv39_page_fault.S - 验证 Sv39 页错误处理

.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    # 设置页表（只映射必要的地址范围）
    li   t0, 0x80100000

    # Level 2 PTE 0: 映射 VPN[2]=0（UART/CLINT）
    li   t1, 0x00000
    slli t1, t1, 10
    ori  t1, t1, 0xF
    sd   t1, 0(t0)

    # Level 2 PTE 2: 映射 VPN[2]=2（主内存）
    li   t1, 0x80000
    slli t1, t1, 10
    ori  t1, t1, 0xF
    li   t2, 2
    slli t2, t2, 3
    add  t3, t0, t2
    sd   t1, 0(t3)

    # 设置 M-mode trap handler（作为后备）
    la   t0, machine_trap
    csrw mtvec, t0

    # 设置 S-mode trap handler
    la   t0, supervisor_trap
    csrw stvec, t0

    # 委托 page faults 到 S-mode
    # cause 12 = instruction page fault
    # cause 13 = load page fault
    # cause 15 = store page fault
    li   t0, (1 << 12) | (1 << 13) | (1 << 15)
    csrw medeleg, t0

    # 设置 satp
    li   t0, 8
    slli t0, t0, 60
    li   t1, 0x80100
    or   t0, t0, t1
    csrw satp, t0
    sfence.vma

    # 切换到 S-mode
    li   t0, 0x800
    csrw mstatus, t0
    la   t0, supervisor_main
    csrw mepc, t0
    mret

supervisor_main:
    li   s1, 0            # 测试计数器

    # 测试 1: load page fault
    li   t0, 0xC0000000   # 未映射的地址
    lw   t1, 0(t0)        # 应该触发 load page fault
    j    fail             # 不应该执行到这里

after_load_fault:
    addi s1, s1, 1

    # 测试 2: store page fault
    li   t0, 0xC0001000   # 另一个未映射的地址
    li   t1, 0x12345678
    sw   t1, 0(t0)        # 应该触发 store page fault
    j    fail             # 不应该执行到这里

after_store_fault:
    addi s1, s1, 1

    # 验证两个测试都通过
    li   t0, 2
    bne  s1, t0, fail

    # 输出成功标记
    li   t0, 'P'
    sb   t0, 0(s0)
    li   t0, 'F'
    sb   t0, 0(s0)
    li   t0, 'L'
    sb   t0, 0(s0)
    li   t0, 'T'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
success_exit:
    ebreak

supervisor_trap:
    csrr t0, scause
    csrr t1, stval

    # 检查是否是 load page fault (cause = 13)
    li   t2, 13
    beq  t0, t2, handle_load_fault

    # 检查是否是 store page fault (cause = 15)
    li   t2, 15
    beq  t0, t2, handle_store_fault

    j    fail

handle_load_fault:
    # 验证 stval 是触发异常的地址
    li   t2, 0xC0000000
    bne  t1, t2, fail

    # 跳转到下一个测试
    la   t0, after_load_fault
    csrw sepc, t0
    sret

handle_store_fault:
    # 验证 stval 是触发异常的地址
    li   t2, 0xC0001000
    bne  t1, t2, fail

    # 跳转到验证代码
    la   t0, after_store_fault
    csrw sepc, t0
    sret

machine_trap:
    csrr t0, mcause
    li   t1, 3
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, success_exit
    bne  t0, t1, fail

    li   a7, 93
    li   a0, 0
    ecall

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
