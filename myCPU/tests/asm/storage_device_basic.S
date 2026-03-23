# storage_device_basic.S - 验证宿主镜像附加到最小 MMIO 存储设备后可被客体顺序读写
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE
    li   s1, 0x10001000   # STORAGE_BASE

    ld   t0, 0(s1)
    li   t1, 0x31474b4254534d4d
    bne  t0, t1, fail

    ld   t0, 8(s1)        # capacity
    li   t1, 17
    bne  t0, t1, fail

    ld   t0, 16(s1)       # block size
    li   t1, 512
    bne  t0, t1, fail

    ld   t0, 32(s1)       # status
    li   t1, 1
    bne  t0, t1, fail

    sd   zero, 24(s1)     # cursor = 0

    lb   t0, 40(s1)
    li   t1, 'S'
    bne  t0, t1, fail
    lb   t0, 40(s1)
    li   t1, 't'
    bne  t0, t1, fail
    lb   t0, 40(s1)
    li   t1, 'o'
    bne  t0, t1, fail
    lb   t0, 40(s1)
    li   t1, 'r'
    bne  t0, t1, fail

    li   t0, 12
    sd   t0, 24(s1)
    lb   t0, 40(s1)
    li   t1, 'D'
    bne  t0, t1, fail

    li   t0, 15
    sd   t0, 24(s1)
    li   t1, '!'
    sb   t1, 40(s1)
    li   t0, 15
    sd   t0, 24(s1)
    lb   t0, 40(s1)
    li   t1, '!'
    bne  t0, t1, fail

    li   t0, 'D'
    sb   t0, 0(s0)
    li   t0, 'S'
    sb   t0, 0(s0)
    li   t0, 'K'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
