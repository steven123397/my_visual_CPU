#define SYS_MKDIRAT 34
#define SYS_UNLINKAT 35
#define SYS_CHDIR 49
#define SYS_OPENAT 56
#define SYS_CLOSE 57
#define SYS_READ 63
#define SYS_WRITE 64
#define SYS_EXIT 93
#define SYS_NANOSLEEP 101

#define AT_FDCWD -100
#define AT_REMOVEDIR 512
#define O_CREAT 64
#define O_RDONLY 0
#define O_RDWR 2
#define O_TRUNC 512

    .section .rodata
console_path:
    .asciz "/dev/console"
root_dir_path:
    .asciz "/"
third_stage_dir_path:
    .asciz "/post-init-dir-smoke"
third_stage_nested_relative_path:
    .asciz "nested.txt"
third_stage_nested_absolute_path:
    .asciz "/post-init-dir-smoke/nested.txt"
third_stage_nested_expected:
    .asciz "third-stage-nested-ok\n"
third_stage_nested_expected_end:
stage_mkdir_chdir_ok_msg:
    .asciz "mycpu linux userland: stage=mkdir-chdir-ok\n"
stage_mkdir_chdir_ok_msg_end:
stage_nested_file_roundtrip_ok_msg:
    .asciz "mycpu linux userland: stage=nested-file-roundtrip-ok\n"
stage_nested_file_roundtrip_ok_msg_end:
stage_third_stage_reached_msg:
    .asciz "mycpu linux userland: stage=third-stage-reached\n"
stage_third_stage_reached_msg_end:
post_init_reached_msg:
    .asciz "mycpu linux userland: post-init reached\n"
post_init_reached_msg_end:
third_stage_dir_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage dir smoke failed\n"
third_stage_dir_smoke_fail_msg_end:
third_stage_nested_file_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage nested file smoke failed\n"
third_stage_nested_file_smoke_fail_msg_end:
third_stage_console_fail_msg:
    .asciz "mycpu linux userland: third-stage console open failed\n"
third_stage_console_fail_msg_end:
third_stage_write_fail_msg:
    .asciz "mycpu linux userland: third-stage write failed\n"
third_stage_write_fail_msg_end:
third_stage_write_short_msg:
    .asciz "mycpu linux userland: third-stage write short\n"
third_stage_write_short_msg_end:

.set third_stage_nested_expected_len, third_stage_nested_expected_end - third_stage_nested_expected
.set third_stage_nested_expected_len, third_stage_nested_expected_len - 1
.set stage_mkdir_chdir_ok_msg_len, stage_mkdir_chdir_ok_msg_end - stage_mkdir_chdir_ok_msg
.set stage_mkdir_chdir_ok_msg_len, stage_mkdir_chdir_ok_msg_len - 1
.set stage_nested_file_roundtrip_ok_msg_len, stage_nested_file_roundtrip_ok_msg_end - stage_nested_file_roundtrip_ok_msg
.set stage_nested_file_roundtrip_ok_msg_len, stage_nested_file_roundtrip_ok_msg_len - 1
.set stage_third_stage_reached_msg_len, stage_third_stage_reached_msg_end - stage_third_stage_reached_msg
.set stage_third_stage_reached_msg_len, stage_third_stage_reached_msg_len - 1
.set post_init_reached_msg_len, post_init_reached_msg_end - post_init_reached_msg
.set post_init_reached_msg_len, post_init_reached_msg_len - 1
.set third_stage_dir_smoke_fail_msg_len, third_stage_dir_smoke_fail_msg_end - third_stage_dir_smoke_fail_msg
.set third_stage_dir_smoke_fail_msg_len, third_stage_dir_smoke_fail_msg_len - 1
.set third_stage_nested_file_smoke_fail_msg_len, third_stage_nested_file_smoke_fail_msg_end - third_stage_nested_file_smoke_fail_msg
.set third_stage_nested_file_smoke_fail_msg_len, third_stage_nested_file_smoke_fail_msg_len - 1
.set third_stage_console_fail_msg_len, third_stage_console_fail_msg_end - third_stage_console_fail_msg
.set third_stage_console_fail_msg_len, third_stage_console_fail_msg_len - 1
.set third_stage_write_fail_msg_len, third_stage_write_fail_msg_end - third_stage_write_fail_msg
.set third_stage_write_fail_msg_len, third_stage_write_fail_msg_len - 1
.set third_stage_write_short_msg_len, third_stage_write_short_msg_end - third_stage_write_short_msg
.set third_stage_write_short_msg_len, third_stage_write_short_msg_len - 1

sleep_spec:
    .dword 1
    .dword 0

    .section .bss
third_stage_nested_buf:
    .space third_stage_nested_expected_len

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

    jal ra, dir_smoke
    bltz a0, dir_smoke_failed
    or s1, s1, a1

    jal ra, nested_file_smoke
    bltz a0, nested_file_smoke_failed
    or s1, s1, a1

    mv a0, s0
    la a1, stage_third_stage_reached_msg
    li a2, stage_third_stage_reached_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s0
    la a1, post_init_reached_msg
    li a2, post_init_reached_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1
    beqz s1, sleep_forever

    mv a0, s0
    la a1, third_stage_write_short_msg
    li a2, third_stage_write_short_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    j sleep_forever

console_open_failed:
    li a0, 1
    la a1, third_stage_console_fail_msg
    li a2, third_stage_console_fail_msg_len
    li a7, SYS_WRITE
    ecall
    li a0, 1
    li a7, SYS_EXIT
    ecall

dir_smoke_failed:
    mv a0, s0
    la a1, third_stage_dir_smoke_fail_msg
    li a2, third_stage_dir_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

nested_file_smoke_failed:
    mv a0, s0
    la a1, third_stage_nested_file_smoke_fail_msg
    li a2, third_stage_nested_file_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

write_failed:
    li a0, 1
    la a1, third_stage_write_fail_msg
    li a2, third_stage_write_fail_msg_len
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

cleanup_dir_smoke_paths:
    la a0, root_dir_path
    li a7, SYS_CHDIR
    ecall

    li a0, AT_FDCWD
    la a1, third_stage_nested_absolute_path
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall

    li a0, AT_FDCWD
    la a1, third_stage_dir_path
    li a2, AT_REMOVEDIR
    li a7, SYS_UNLINKAT
    ecall
    ret

dir_smoke:
    mv s2, ra
    li t5, 0

    jal ra, cleanup_dir_smoke_paths

    li a0, AT_FDCWD
    la a1, third_stage_dir_path
    li a2, 493
    li a7, SYS_MKDIRAT
    ecall
    bltz a0, dir_smoke_error

    la a0, third_stage_dir_path
    li a7, SYS_CHDIR
    ecall
    bltz a0, dir_smoke_cleanup_error

    mv a0, s0
    la a1, stage_mkdir_chdir_ok_msg
    li a2, stage_mkdir_chdir_ok_msg_len
    jal ra, write_exact
    bltz a0, dir_smoke_cleanup_error
    or t5, t5, a1

    mv ra, s2
    li a0, 0
    mv a1, t5
    ret

dir_smoke_cleanup_error:
    jal ra, cleanup_dir_smoke_paths

dir_smoke_error:
    mv ra, s2
    li a0, -1
    li a1, 0
    ret

nested_file_smoke:
    mv s2, ra
    li t5, 0

    li a0, AT_FDCWD
    la a1, third_stage_nested_relative_path
    li a2, O_RDWR | O_CREAT | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, nested_file_smoke_cleanup_error
    mv t0, a0

    mv a0, t0
    la a1, third_stage_nested_expected
    li a2, third_stage_nested_expected_len
    jal ra, write_exact
    bltz a0, nested_file_smoke_close_write_error
    or t5, t5, a1

    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    bltz a0, nested_file_smoke_cleanup_error

    li a0, AT_FDCWD
    la a1, third_stage_nested_relative_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, nested_file_smoke_cleanup_error
    mv t0, a0

    mv a0, t0
    la a1, third_stage_nested_buf
    li a2, third_stage_nested_expected_len
    li a7, SYS_READ
    ecall
    li t1, third_stage_nested_expected_len
    bne a0, t1, nested_file_smoke_close_read_error

    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    bltz a0, nested_file_smoke_cleanup_error

    la a0, third_stage_nested_buf
    la a1, third_stage_nested_expected
    li a2, third_stage_nested_expected_len
    jal ra, compare_bytes
    bltz a0, nested_file_smoke_cleanup_error

    li a0, AT_FDCWD
    la a1, third_stage_nested_relative_path
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, nested_file_smoke_cleanup_error

    la a0, root_dir_path
    li a7, SYS_CHDIR
    ecall
    bltz a0, nested_file_smoke_cleanup_error

    li a0, AT_FDCWD
    la a1, third_stage_dir_path
    li a2, AT_REMOVEDIR
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, nested_file_smoke_cleanup_error

    mv a0, s0
    la a1, stage_nested_file_roundtrip_ok_msg
    li a2, stage_nested_file_roundtrip_ok_msg_len
    jal ra, write_exact
    bltz a0, nested_file_smoke_error
    or t5, t5, a1

    mv ra, s2
    li a0, 0
    mv a1, t5
    ret

nested_file_smoke_close_write_error:
    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    j nested_file_smoke_cleanup_error

nested_file_smoke_close_read_error:
    mv a0, t0
    li a7, SYS_CLOSE
    ecall

nested_file_smoke_cleanup_error:
    jal ra, cleanup_dir_smoke_paths

nested_file_smoke_error:
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
