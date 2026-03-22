# counteren_access_control.S - 验证 mcounteren/scounteren 对 cycle/time/instret 的特权访问控制
.section .text
.global _start
_start:
    li   s0, 0x10000000   # UART_BASE

    la   t0, trap_handler
    csrw mtvec, t0

    li   t0, 0
    csrw mscratch, t0
    csrw mcounteren, t0
    csrw scounteren, t0

    li   t0, 0x800        # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_cycle_denied
    csrw mepc, t0
    mret

supervisor_cycle_denied:
    csrr t0, cycle
    j    fail

supervisor_cycle_allowed:
    csrr t0, cycle
supervisor_cycle_ecall:
    ecall

user_cycle_denied:
    csrr t0, cycle
    j    fail

user_cycle_allowed:
    csrr t0, cycle
user_cycle_ecall:
    ecall

supervisor_time_denied:
    csrr t0, time
    j    fail

supervisor_time_allowed:
    csrr t0, time
supervisor_time_ecall:
    ecall

user_time_denied:
    csrr t0, time
    j    fail

user_time_allowed:
    csrr t0, time
user_time_ecall:
    ecall

supervisor_instret_denied:
    csrr t0, instret
    j    fail

supervisor_instret_allowed:
    csrr t0, instret
supervisor_instret_ecall:
    ecall

user_instret_denied:
    csrr t0, instret
    j    fail

user_instret_allowed:
    csrr t0, instret
user_instret_ecall:
    ecall

after_tests:
    li   t0, 'C'
    sb   t0, 0(s0)
    li   t0, 'N'
    sb   t0, 0(s0)
    li   t0, 'T'
    sb   t0, 0(s0)

    li   a7, 93
    li   a0, 0
    ecall

trap_handler:
    csrr t0, mscratch
    beqz t0, handle_supervisor_cycle_denied

    li   t1, 1
    beq  t0, t1, handle_supervisor_cycle_allowed

    li   t1, 2
    beq  t0, t1, handle_user_cycle_denied

    li   t1, 3
    beq  t0, t1, handle_user_cycle_allowed

    li   t1, 4
    beq  t0, t1, handle_supervisor_time_denied

    li   t1, 5
    beq  t0, t1, handle_supervisor_time_allowed

    li   t1, 6
    beq  t0, t1, handle_user_time_denied

    li   t1, 7
    beq  t0, t1, handle_user_time_allowed

    li   t1, 8
    beq  t0, t1, handle_supervisor_instret_denied

    li   t1, 9
    beq  t0, t1, handle_supervisor_instret_allowed

    li   t1, 10
    beq  t0, t1, handle_user_instret_denied

    li   t1, 11
    beq  t0, t1, handle_user_instret_allowed

    j    fail

handle_supervisor_cycle_denied:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, supervisor_cycle_denied
    bne  t0, t1, fail

    li   t0, 1
    csrw mscratch, t0
    csrw mcounteren, t0   # CY
    li   t0, 0
    csrw scounteren, t0

    li   t0, 0x800        # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_cycle_allowed
    csrw mepc, t0
    mret

handle_supervisor_cycle_allowed:
    csrr t0, mcause
    li   t1, 9
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, supervisor_cycle_ecall
    bne  t0, t1, fail

    li   t0, 2
    csrw mscratch, t0
    li   t0, 1            # CY
    csrw mcounteren, t0
    li   t0, 0
    csrw scounteren, t0

    li   t0, 0            # MPP = U
    csrw mstatus, t0
    la   t0, user_cycle_denied
    csrw mepc, t0
    mret

handle_user_cycle_denied:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, user_cycle_denied
    bne  t0, t1, fail

    li   t0, 3
    csrw mscratch, t0
    li   t0, 1            # CY
    csrw mcounteren, t0
    csrw scounteren, t0

    li   t0, 0            # MPP = U
    csrw mstatus, t0
    la   t0, user_cycle_allowed
    csrw mepc, t0
    mret

handle_user_cycle_allowed:
    csrr t0, mcause
    li   t1, 8
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, user_cycle_ecall
    bne  t0, t1, fail

    li   t0, 4
    csrw mscratch, t0
    li   t0, 1            # CY only, TM remains blocked
    csrw mcounteren, t0

    li   t0, 0x800        # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_time_denied
    csrw mepc, t0
    mret

handle_supervisor_time_denied:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, supervisor_time_denied
    bne  t0, t1, fail

    li   t0, 5
    csrw mscratch, t0
    li   t0, 3            # CY | TM
    csrw mcounteren, t0

    li   t0, 0x800        # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_time_allowed
    csrw mepc, t0
    mret

handle_supervisor_time_allowed:
    csrr t0, mcause
    li   t1, 9
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, supervisor_time_ecall
    bne  t0, t1, fail

    li   t0, 6
    csrw mscratch, t0
    li   t0, 3            # CY | TM
    csrw mcounteren, t0
    li   t0, 1            # CY only, TM still blocked for U
    csrw scounteren, t0

    li   t0, 0            # MPP = U
    csrw mstatus, t0
    la   t0, user_time_denied
    csrw mepc, t0
    mret

handle_user_time_denied:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, user_time_denied
    bne  t0, t1, fail

    li   t0, 7
    csrw mscratch, t0
    li   t0, 3            # CY | TM
    csrw mcounteren, t0
    csrw scounteren, t0

    li   t0, 0            # MPP = U
    csrw mstatus, t0
    la   t0, user_time_allowed
    csrw mepc, t0
    mret

handle_user_time_allowed:
    csrr t0, mcause
    li   t1, 8
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, user_time_ecall
    bne  t0, t1, fail

    li   t0, 8
    csrw mscratch, t0
    li   t0, 3            # CY | TM, IR blocked
    csrw mcounteren, t0
    li   t0, 0
    csrw scounteren, t0

    li   t0, 0x800        # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_instret_denied
    csrw mepc, t0
    mret

handle_supervisor_instret_denied:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, supervisor_instret_denied
    bne  t0, t1, fail

    li   t0, 9
    csrw mscratch, t0
    li   t0, 7            # CY | TM | IR
    csrw mcounteren, t0

    li   t0, 0x800        # MPP = S
    csrw mstatus, t0
    la   t0, supervisor_instret_allowed
    csrw mepc, t0
    mret

handle_supervisor_instret_allowed:
    csrr t0, mcause
    li   t1, 9
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, supervisor_instret_ecall
    bne  t0, t1, fail

    li   t0, 10
    csrw mscratch, t0
    li   t0, 7            # CY | TM | IR
    csrw mcounteren, t0
    li   t0, 0x3          # CY | TM only, IR blocked for U
    csrw scounteren, t0

    li   t0, 0            # MPP = U
    csrw mstatus, t0
    la   t0, user_instret_denied
    csrw mepc, t0
    mret

handle_user_instret_denied:
    csrr t0, mcause
    li   t1, 2
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, user_instret_denied
    bne  t0, t1, fail

    li   t0, 11
    csrw mscratch, t0
    li   t0, 7            # CY | TM | IR
    csrw mcounteren, t0
    csrw scounteren, t0

    li   t0, 0            # MPP = U
    csrw mstatus, t0
    la   t0, user_instret_allowed
    csrw mepc, t0
    mret

handle_user_instret_allowed:
    csrr t0, mcause
    li   t1, 8
    bne  t0, t1, fail

    csrr t0, mepc
    la   t1, user_instret_ecall
    bne  t0, t1, fail

    j    after_tests

fail:
    li   t0, 'X'
    sb   t0, 0(s0)
    li   a7, 93
    li   a0, 1
    ecall
