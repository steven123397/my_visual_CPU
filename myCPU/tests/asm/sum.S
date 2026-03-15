# 求和测试: sum = 1+2+...+10, 结果存 a0, 通过 UART 输出 '0'+sum%10
.section .text
.global _start
_start:
    li   t0, 1
    li   t1, 10
    li   a0, 0
sum_loop:
    add  a0, a0, t0
    addi t0, t0, 1
    ble  t0, t1, sum_loop

    # 输出结果数字 (55 -> '7')
    li   t0, 0x10000000
    addi a0, a0, 48       # '0' + 55 % 10 ... 直接输出 55 的十位和个位
    li   a1, 55
    li   a2, 10
    div  a3, a1, a2       # a3 = 5
    rem  a4, a1, a2       # a4 = 5
    addi a3, a3, 48
    addi a4, a4, 48
    sb   a3, 0(t0)
    sb   a4, 0(t0)
    li   a5, 10
    sb   a5, 0(t0)        # newline

    li   a7, 93
    li   a0, 0
    ecall
