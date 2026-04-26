#define SYS_OPENAT 56
#define SYS_CLOSE 57
#define SYS_READ 63
#define SYS_WRITE 64
#define SYS_EXIT 93
#define SYS_NANOSLEEP 101

#define AT_FDCWD -100
#define O_RDONLY 0
#define O_RDWR 2

    .section .rodata
console_path:
    .asciz "/dev/console"
post_init_data_path:
    .asciz "/post-init-data.txt"
post_init_data_expected:
    .asciz "post-init-data-ok\n"
post_init_data_expected_end:
stage_file_readable_msg:
    .asciz "mycpu linux userland: stage=file-readable\n"
stage_file_readable_msg_end:
post_init_reached_msg:
    .asciz "mycpu linux userland: post-init reached\n"
post_init_reached_msg_end:
post_init_write_short_msg:
    .asciz "mycpu linux userland: write short\n"
post_init_write_short_msg_end:
post_init_file_smoke_fail_msg:
    .asciz "mycpu linux userland: file smoke failed\n"
post_init_file_smoke_fail_msg_end:
post_init_console_fail_msg:
    .asciz "mycpu linux userland: console open failed\n"
post_init_console_fail_msg_end:
post_init_write_fail_msg:
    .asciz "mycpu linux userland: write failed\n"
post_init_write_fail_msg_end:

.set post_init_data_expected_len, post_init_data_expected_end - post_init_data_expected
.set post_init_data_expected_len, post_init_data_expected_len - 1
.set stage_file_readable_msg_len, stage_file_readable_msg_end - stage_file_readable_msg
.set stage_file_readable_msg_len, stage_file_readable_msg_len - 1
.set post_init_reached_msg_len, post_init_reached_msg_end - post_init_reached_msg
.set post_init_reached_msg_len, post_init_reached_msg_len - 1
.set post_init_write_short_msg_len, post_init_write_short_msg_end - post_init_write_short_msg
.set post_init_write_short_msg_len, post_init_write_short_msg_len - 1
.set post_init_file_smoke_fail_msg_len, post_init_file_smoke_fail_msg_end - post_init_file_smoke_fail_msg
.set post_init_file_smoke_fail_msg_len, post_init_file_smoke_fail_msg_len - 1
.set post_init_console_fail_msg_len, post_init_console_fail_msg_end - post_init_console_fail_msg
.set post_init_console_fail_msg_len, post_init_console_fail_msg_len - 1
.set post_init_write_fail_msg_len, post_init_write_fail_msg_end - post_init_write_fail_msg
.set post_init_write_fail_msg_len, post_init_write_fail_msg_len - 1

sleep_spec:
    .dword 1
    .dword 0

    .section .bss
    .balign 8
post_init_data_buf:
    .space post_init_data_expected_len

    .section .text
    .globl _start
_start:
    li a0, AT_FDCWD
    la a1, console_path
    li a2, O_RDWR
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, console_open_failed
    mv s0, a0
    li s1, 0

    jal ra, file_smoke
    bltz a0, file_smoke_failed
    or s1, s1, a1

    mv a0, s0
    la a1, post_init_reached_msg
    li a2, post_init_reached_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1
    beqz s1, sleep_forever

    mv a0, s0
    la a1, post_init_write_short_msg
    li a2, post_init_write_short_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    j sleep_forever

console_open_failed:
    li a0, 1
    la a1, post_init_console_fail_msg
    li a2, post_init_console_fail_msg_len
    li a7, SYS_WRITE
    ecall
    li a0, 1
    li a7, SYS_EXIT
    ecall

file_smoke_failed:
    mv a0, s0
    la a1, post_init_file_smoke_fail_msg
    li a2, post_init_file_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

write_failed:
    li a0, 1
    la a1, post_init_write_fail_msg
    li a2, post_init_write_fail_msg_len
    li a7, SYS_WRITE
    ecall
    li a0, 1
    li a7, SYS_EXIT
    ecall

write_exact:
    mv t0, a0
    mv t1, a1
    mv t2, a2
    li t3, 0

write_exact_loop:
    beqz t2, write_exact_done
    mv a0, t0
    mv a1, t1
    mv a2, t2
    li a7, SYS_WRITE
    ecall
    bltz a0, write_exact_error
    beqz a0, write_exact_zero
    bge a0, t2, write_exact_advance
    li t3, 1

write_exact_advance:
    add t1, t1, a0
    sub t2, t2, a0
    j write_exact_loop

write_exact_done:
    li a0, 0
    mv a1, t3
    ret

write_exact_error:
    li a1, 0
    ret

write_exact_zero:
    li a0, -1
    li a1, 0
    ret

compare_bytes:
    beqz a2, compare_bytes_equal

compare_bytes_loop:
    lbu t0, 0(a0)
    lbu t1, 0(a1)
    bne t0, t1, compare_bytes_notequal
    addi a0, a0, 1
    addi a1, a1, 1
    addi a2, a2, -1
    bnez a2, compare_bytes_loop

compare_bytes_equal:
    li a0, 0
    ret

compare_bytes_notequal:
    li a0, -1
    ret

file_smoke:
    mv s2, ra
    li t5, 0

    li a0, AT_FDCWD
    la a1, post_init_data_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, file_smoke_error
    mv t0, a0

    mv a0, t0
    la a1, post_init_data_buf
    li a2, post_init_data_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_expected_len
    bne a0, t1, file_smoke_close_error

    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    bltz a0, file_smoke_error

    la a0, post_init_data_buf
    la a1, post_init_data_expected
    li a2, post_init_data_expected_len
    jal ra, compare_bytes
    bltz a0, file_smoke_error

    mv a0, s0
    la a1, stage_file_readable_msg
    li a2, stage_file_readable_msg_len
    jal ra, write_exact
    bltz a0, file_smoke_error
    or t5, t5, a1

    mv ra, s2
    li a0, 0
    mv a1, t5
    ret

file_smoke_close_error:
    mv a0, t0
    li a7, SYS_CLOSE
    ecall

file_smoke_error:
    mv ra, s2
    li a0, -1
    li a1, 0
    ret

sleep_forever:
    la a0, sleep_spec
    li a1, 0
    li a7, SYS_NANOSLEEP
    ecall
    j sleep_forever
