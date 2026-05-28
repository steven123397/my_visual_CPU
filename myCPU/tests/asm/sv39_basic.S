# sv39_basic.S - 验证 Sv39 虚拟内存基本功能

.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    la   t0, machine_trap
    csrw mtvec, t0

    # 在 M-mode 下设置页表
    # 页表放在 0x80100000
    li   t0, 0x80100000

    # Level 2 PTE 0: 映射 VPN[2]=0 到 PPN=0x00000（即 0x00000000-0x3FFFFFFF 物理地址）
    # 这包含了 UART (0x10000000) 和 CLINT (0x02000000)
    li   t1, 0x00000
    slli t1, t1, 10
    ori  t1, t1, 0xF      # V=1, R=1, W=1, X=1
    sd   t1, 0(t0)        # 写入第 0 项

    # Level 2 PTE 2: 映射 VPN[2]=2 到 PPN=0x80000（即 0x80000000-0xBFFFFFFF 物理地址）
    # 这包含了主内存
    li   t1, 0x80000
    slli t1, t1, 10
    ori  t1, t1, 0xF      # V=1, R=1, W=1, X=1
    li   t2, 2
    slli t2, t2, 3        # 每个 PTE 8 字节
    add  t3, t0, t2
    sd   t1, 0(t3)        # 写入第 2 项

    # 设置 satp: MODE=8 (Sv39), PPN=0x80100000 >> 12 = 0x80100
    li   t0, 8
    slli t0, t0, 60       # MODE 在 [63:60]
    li   t1, 0x80100
    or   t0, t0, t1
    csrw satp, t0

    # 当前还没有 TLB，但应接受合法的 sfence.vma 变体编码
    .word 0x12008073       # sfence.vma x1, x0
    .word 0x12200073       # sfence.vma x0, x2

    # 切换到 S-mode
    li   t0, 0x800        # MPP = S (01 << 11)
    csrw mstatus, t0
    la   t0, supervisor_main
    csrw mepc, t0
    mret

supervisor_main:
    # 现在在 S-mode，分页已启用
    # 虚拟地址 0x80000000 应该通过页表映射到物理地址 0x80000000

    # 测试: 通过虚拟地址写入和读取数据
    li   t0, 0x80000000
    li   t1, 0x12345678
    sw   t1, 0(t0)
    lw   t2, 0(t0)
    bne  t1, t2, fail

    # 输出成功标记
    li   t0, 'S'
    sb   t0, 0(s0)
    li   t0, 'V'
    sb   t0, 0(s0)
    li   t0, '3'
    sb   t0, 0(s0)
    li   t0, '9'
    sb   t0, 0(s0)

    # 退出
    li   a7, 93
    li   a0, 0
success_exit:
    ebreak

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
