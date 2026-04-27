#define SYS_UNLINKAT 35
#define SYS_CHDIR 49
#define SYS_OPENAT 56
#define SYS_CLOSE 57
#define SYS_GETDENTS64 61
#define SYS_WRITE 64
#define SYS_EXIT 93
#define SYS_NANOSLEEP 101

#define AT_FDCWD -100
#define AT_REMOVEDIR 512
#define O_RDONLY 0
#define O_RDWR 2

    .section .rodata
console_path:
    .asciz "/dev/console"
root_dir_path:
    .asciz "/"
current_dir_path:
    .asciz "."
reused_dir_path:
    .asciz "/post-init-dir-smoke"
reused_dir_name:
    .asciz "post-init-dir-smoke"
stage_unlinkat_reused_dirent_gone_msg:
    .asciz "mycpu linux userland: stage=unlinkat-reused-dirent-gone\n"
stage_unlinkat_reused_dirent_gone_msg_end:
stage_fourth_stage_reached_msg:
    .asciz "mycpu linux userland: stage=fourth-stage-reached\n"
stage_fourth_stage_reached_msg_end:
post_init_reached_msg:
    .asciz "mycpu linux userland: post-init reached\n"
post_init_reached_msg_end:
fourth_stage_reused_dir_cleanup_fail_msg:
    .asciz "mycpu linux userland: fourth-stage reused dir cleanup failed\n"
fourth_stage_reused_dir_cleanup_fail_msg_end:
fourth_stage_reused_dir_dirent_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage reused dir dirent smoke failed\n"
fourth_stage_reused_dir_dirent_smoke_fail_msg_end:
fourth_stage_console_fail_msg:
    .asciz "mycpu linux userland: fourth-stage console open failed\n"
fourth_stage_console_fail_msg_end:
fourth_stage_write_fail_msg:
    .asciz "mycpu linux userland: fourth-stage write failed\n"
fourth_stage_write_fail_msg_end:
fourth_stage_write_short_msg:
    .asciz "mycpu linux userland: fourth-stage write short\n"
fourth_stage_write_short_msg_end:

.set stage_unlinkat_reused_dirent_gone_msg_len, stage_unlinkat_reused_dirent_gone_msg_end - stage_unlinkat_reused_dirent_gone_msg
.set stage_unlinkat_reused_dirent_gone_msg_len, stage_unlinkat_reused_dirent_gone_msg_len - 1
.set stage_fourth_stage_reached_msg_len, stage_fourth_stage_reached_msg_end - stage_fourth_stage_reached_msg
.set stage_fourth_stage_reached_msg_len, stage_fourth_stage_reached_msg_len - 1
.set post_init_reached_msg_len, post_init_reached_msg_end - post_init_reached_msg
.set post_init_reached_msg_len, post_init_reached_msg_len - 1
.set fourth_stage_reused_dir_cleanup_fail_msg_len, fourth_stage_reused_dir_cleanup_fail_msg_end - fourth_stage_reused_dir_cleanup_fail_msg
.set fourth_stage_reused_dir_cleanup_fail_msg_len, fourth_stage_reused_dir_cleanup_fail_msg_len - 1
.set fourth_stage_reused_dir_dirent_smoke_fail_msg_len, fourth_stage_reused_dir_dirent_smoke_fail_msg_end - fourth_stage_reused_dir_dirent_smoke_fail_msg
.set fourth_stage_reused_dir_dirent_smoke_fail_msg_len, fourth_stage_reused_dir_dirent_smoke_fail_msg_len - 1
.set fourth_stage_console_fail_msg_len, fourth_stage_console_fail_msg_end - fourth_stage_console_fail_msg
.set fourth_stage_console_fail_msg_len, fourth_stage_console_fail_msg_len - 1
.set fourth_stage_write_fail_msg_len, fourth_stage_write_fail_msg_end - fourth_stage_write_fail_msg
.set fourth_stage_write_fail_msg_len, fourth_stage_write_fail_msg_len - 1
.set fourth_stage_write_short_msg_len, fourth_stage_write_short_msg_end - fourth_stage_write_short_msg
.set fourth_stage_write_short_msg_len, fourth_stage_write_short_msg_len - 1
.set fourth_stage_dirent_buf_len, 1024

sleep_spec:
    .dword 1
    .dword 0

    .section .bss
fourth_stage_dirent_buf:
    .space fourth_stage_dirent_buf_len

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

    la a0, root_dir_path
    li a7, SYS_CHDIR
    ecall
    bltz a0, reused_dir_cleanup_failed

    li a0, AT_FDCWD
    la a1, reused_dir_path
    li a2, AT_REMOVEDIR
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, reused_dir_cleanup_failed

    la a0, reused_dir_name
    jal ra, dir_contains_name
    bnez a0, reused_dir_dirent_smoke_failed

    mv a0, s0
    la a1, stage_unlinkat_reused_dirent_gone_msg
    li a2, stage_unlinkat_reused_dirent_gone_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s0
    la a1, stage_fourth_stage_reached_msg
    li a2, stage_fourth_stage_reached_msg_len
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
    la a1, fourth_stage_write_short_msg
    li a2, fourth_stage_write_short_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    j sleep_forever

console_open_failed:
    li a0, 1
    la a1, fourth_stage_console_fail_msg
    li a2, fourth_stage_console_fail_msg_len
    li a7, SYS_WRITE
    ecall
    li a0, 1
    li a7, SYS_EXIT
    ecall

reused_dir_cleanup_failed:
    mv a0, s0
    la a1, fourth_stage_reused_dir_cleanup_fail_msg
    li a2, fourth_stage_reused_dir_cleanup_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

reused_dir_dirent_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_reused_dir_dirent_smoke_fail_msg
    li a2, fourth_stage_reused_dir_dirent_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

write_failed:
    li a0, 1
    la a1, fourth_stage_write_fail_msg
    li a2, fourth_stage_write_fail_msg_len
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

compare_cstr:
compare_cstr_loop:
    lbu t0, 0(a0)
    lbu t1, 0(a1)
    bne t0, t1, compare_cstr_notequal
    beqz t0, compare_cstr_equal
    addi a0, a0, 1
    addi a1, a1, 1
    j compare_cstr_loop

compare_cstr_equal:
    li a0, 0
    ret

compare_cstr_notequal:
    li a0, -1
    ret

dir_contains_name:
    addi sp, sp, -40
    sd ra, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    sd s6, 0(sp)

    mv s5, a0
    li s4, -1

    li a0, AT_FDCWD
    la a1, current_dir_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, dir_contains_name_error
    mv s4, a0

dir_contains_name_read_loop:
    mv a0, s4
    la a1, fourth_stage_dirent_buf
    li a2, fourth_stage_dirent_buf_len
    li a7, SYS_GETDENTS64
    ecall
    bltz a0, dir_contains_name_close_error
    beqz a0, dir_contains_name_close_not_found
    mv s6, a0
    la s3, fourth_stage_dirent_buf

dir_contains_name_parse_loop:
    beqz s6, dir_contains_name_read_loop
    lhu t2, 16(s3)
    li t1, 19
    bltu t2, t1, dir_contains_name_close_error
    bltu s6, t2, dir_contains_name_close_error
    addi a0, s3, 19
    mv a1, s5
    jal ra, compare_cstr
    beqz a0, dir_contains_name_found
    add s3, s3, t2
    sub s6, s6, t2
    j dir_contains_name_parse_loop

dir_contains_name_found:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    bltz a0, dir_contains_name_error
    li a0, 1
    j dir_contains_name_done

dir_contains_name_close_not_found:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    bltz a0, dir_contains_name_error
    li a0, 0
    j dir_contains_name_done

dir_contains_name_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall

dir_contains_name_error:
    li a0, -1

dir_contains_name_done:
    ld s6, 0(sp)
    ld s5, 8(sp)
    ld s4, 16(sp)
    ld s3, 24(sp)
    ld ra, 32(sp)
    addi sp, sp, 40
    ret

sleep_forever:
    la a0, sleep_spec
    li a1, 0
    li a7, SYS_NANOSLEEP
    ecall
    j sleep_forever
