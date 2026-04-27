#define SYS_CLONE3 435
#define SYS_EXECVE 221
#define SYS_WAIT4 260
#define SYS_UNLINKAT 35
#define SYS_OPENAT 56
#define SYS_CLOSE 57
#define SYS_READ 63
#define SYS_WRITE 64
#define SYS_EXIT 93
#define SYS_EXIT_GROUP 94
#define SYS_NANOSLEEP 101

#define AT_FDCWD -100
#define SIGCHLD 17
#define O_CREAT 64
#define O_RDONLY 0
#define O_RDWR 2
#define O_TRUNC 512

    .section .rodata
console_path:
    .asciz "/dev/console"
post_init_data_path:
    .asciz "/post-init-data.txt"
post_init_data_expected:
    .asciz "post-init-data-ok\n"
post_init_data_expected_end:
post_init_rw_path:
    .asciz "/post-init-rw-smoke.txt"
post_init_exec_smoke_path:
    .asciz "/post-init-exec-smoke"
post_init_rw_expected:
    .asciz "post-init-rw-ok\n"
post_init_rw_expected_end:
stage_file_readable_msg:
    .asciz "mycpu linux userland: stage=file-readable\n"
stage_file_readable_msg_end:
stage_rootfs_rw_roundtrip_ok_msg:
    .asciz "mycpu linux userland: stage=rootfs-rw-roundtrip-ok\n"
stage_rootfs_rw_roundtrip_ok_msg_end:
stage_fork_child_wrote_msg:
    .asciz "mycpu linux userland: stage=fork-child-wrote\n"
stage_fork_child_wrote_msg_end:
stage_parent_wait4_ok_msg:
    .asciz "mycpu linux userland: stage=parent-wait4-ok\n"
stage_parent_wait4_ok_msg_end:
stage_execve_third_stage_msg:
    .asciz "mycpu linux userland: stage=execve-third-stage\n"
stage_execve_third_stage_msg_end:
post_init_reached_msg:
    .asciz "mycpu linux userland: post-init reached\n"
post_init_reached_msg_end:
post_init_write_short_msg:
    .asciz "mycpu linux userland: write short\n"
post_init_write_short_msg_end:
post_init_file_smoke_fail_msg:
    .asciz "mycpu linux userland: file smoke failed\n"
post_init_file_smoke_fail_msg_end:
post_init_rootfs_rw_smoke_fail_msg:
    .asciz "mycpu linux userland: rootfs rw smoke failed\n"
post_init_rootfs_rw_smoke_fail_msg_end:
post_init_process_smoke_fail_msg:
    .asciz "mycpu linux userland: process smoke failed\n"
post_init_process_smoke_fail_msg_end:
post_init_exec_third_stage_fail_msg:
    .asciz "mycpu linux userland: exec third-stage failed\n"
post_init_exec_third_stage_fail_msg_end:
post_init_console_fail_msg:
    .asciz "mycpu linux userland: console open failed\n"
post_init_console_fail_msg_end:
post_init_write_fail_msg:
    .asciz "mycpu linux userland: write failed\n"
post_init_write_fail_msg_end:
clone3_args:
    .dword 0
    .dword 0
    .dword 0
    .dword 0
    .dword SIGCHLD
    .dword 0
    .dword 0
    .dword 0
    .dword 0
    .dword 0
    .dword 0
    .balign 8
post_init_exec_smoke_argv:
    .dword post_init_exec_smoke_path
    .dword 0
post_init_exec_smoke_envp:
    .dword 0

.set post_init_data_expected_len, post_init_data_expected_end - post_init_data_expected
.set post_init_data_expected_len, post_init_data_expected_len - 1
.set post_init_rw_expected_len, post_init_rw_expected_end - post_init_rw_expected
.set post_init_rw_expected_len, post_init_rw_expected_len - 1
.set stage_file_readable_msg_len, stage_file_readable_msg_end - stage_file_readable_msg
.set stage_file_readable_msg_len, stage_file_readable_msg_len - 1
.set stage_rootfs_rw_roundtrip_ok_msg_len, stage_rootfs_rw_roundtrip_ok_msg_end - stage_rootfs_rw_roundtrip_ok_msg
.set stage_rootfs_rw_roundtrip_ok_msg_len, stage_rootfs_rw_roundtrip_ok_msg_len - 1
.set stage_fork_child_wrote_msg_len, stage_fork_child_wrote_msg_end - stage_fork_child_wrote_msg
.set stage_fork_child_wrote_msg_len, stage_fork_child_wrote_msg_len - 1
.set stage_parent_wait4_ok_msg_len, stage_parent_wait4_ok_msg_end - stage_parent_wait4_ok_msg
.set stage_parent_wait4_ok_msg_len, stage_parent_wait4_ok_msg_len - 1
.set stage_execve_third_stage_msg_len, stage_execve_third_stage_msg_end - stage_execve_third_stage_msg
.set stage_execve_third_stage_msg_len, stage_execve_third_stage_msg_len - 1
.set post_init_reached_msg_len, post_init_reached_msg_end - post_init_reached_msg
.set post_init_reached_msg_len, post_init_reached_msg_len - 1
.set post_init_write_short_msg_len, post_init_write_short_msg_end - post_init_write_short_msg
.set post_init_write_short_msg_len, post_init_write_short_msg_len - 1
.set post_init_file_smoke_fail_msg_len, post_init_file_smoke_fail_msg_end - post_init_file_smoke_fail_msg
.set post_init_file_smoke_fail_msg_len, post_init_file_smoke_fail_msg_len - 1
.set post_init_rootfs_rw_smoke_fail_msg_len, post_init_rootfs_rw_smoke_fail_msg_end - post_init_rootfs_rw_smoke_fail_msg
.set post_init_rootfs_rw_smoke_fail_msg_len, post_init_rootfs_rw_smoke_fail_msg_len - 1
.set post_init_process_smoke_fail_msg_len, post_init_process_smoke_fail_msg_end - post_init_process_smoke_fail_msg
.set post_init_process_smoke_fail_msg_len, post_init_process_smoke_fail_msg_len - 1
.set post_init_exec_third_stage_fail_msg_len, post_init_exec_third_stage_fail_msg_end - post_init_exec_third_stage_fail_msg
.set post_init_exec_third_stage_fail_msg_len, post_init_exec_third_stage_fail_msg_len - 1
.set post_init_console_fail_msg_len, post_init_console_fail_msg_end - post_init_console_fail_msg
.set post_init_console_fail_msg_len, post_init_console_fail_msg_len - 1
.set post_init_write_fail_msg_len, post_init_write_fail_msg_end - post_init_write_fail_msg
.set post_init_write_fail_msg_len, post_init_write_fail_msg_len - 1
.set clone3_args_size, 88

sleep_spec:
    .dword 1
    .dword 0

    .section .bss
    .balign 8
post_init_data_buf:
    .space post_init_data_expected_len
post_init_rw_buf:
    .space post_init_rw_expected_len
    .balign 8
post_init_wait_status:
    .word 0

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

    jal ra, rootfs_rw_smoke
    bltz a0, rootfs_rw_smoke_failed
    or s1, s1, a1

    jal ra, process_smoke
    bltz a0, process_smoke_failed
    or s1, s1, a1

    mv a0, s0
    la a1, stage_execve_third_stage_msg
    li a2, stage_execve_third_stage_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    la a0, post_init_exec_smoke_path
    la a1, post_init_exec_smoke_argv
    la a2, post_init_exec_smoke_envp
    li a7, SYS_EXECVE
    ecall
    j exec_third_stage_failed

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

rootfs_rw_smoke_failed:
    mv a0, s0
    la a1, post_init_rootfs_rw_smoke_fail_msg
    li a2, post_init_rootfs_rw_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

process_smoke_failed:
    mv a0, s0
    la a1, post_init_process_smoke_fail_msg
    li a2, post_init_process_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

exec_third_stage_failed:
    mv a0, s0
    la a1, post_init_exec_third_stage_fail_msg
    li a2, post_init_exec_third_stage_fail_msg_len
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

write_exact_loop:
    beqz t2, write_exact_done
    mv a0, t0
    mv a1, t1
    mv a2, t2
    li a7, SYS_WRITE
    ecall
    bltz a0, write_exact_error
    beqz a0, write_exact_zero

write_exact_advance:
    add t1, t1, a0
    sub t2, t2, a0
    j write_exact_loop

write_exact_done:
    li a0, 0
    li a1, 0
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

rootfs_rw_smoke:
    mv s2, ra
    li t5, 0

    li a0, AT_FDCWD
    la a1, post_init_rw_path
    li a2, O_RDWR | O_CREAT | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, rootfs_rw_smoke_error
    mv t0, a0

    mv a0, t0
    la a1, post_init_rw_expected
    li a2, post_init_rw_expected_len
    jal ra, write_exact
    bltz a0, rootfs_rw_smoke_close_error
    or t5, t5, a1

    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    bltz a0, rootfs_rw_smoke_unlink_error

    li a0, AT_FDCWD
    la a1, post_init_rw_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, rootfs_rw_smoke_unlink_error
    mv t0, a0

    mv a0, t0
    la a1, post_init_rw_buf
    li a2, post_init_rw_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_rw_expected_len
    bne a0, t1, rootfs_rw_smoke_close_read_error

    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    bltz a0, rootfs_rw_smoke_unlink_error

    la a0, post_init_rw_buf
    la a1, post_init_rw_expected
    li a2, post_init_rw_expected_len
    jal ra, compare_bytes
    bltz a0, rootfs_rw_smoke_unlink_error

    li a0, AT_FDCWD
    la a1, post_init_rw_path
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, rootfs_rw_smoke_error

    mv a0, s0
    la a1, stage_rootfs_rw_roundtrip_ok_msg
    li a2, stage_rootfs_rw_roundtrip_ok_msg_len
    jal ra, write_exact
    bltz a0, rootfs_rw_smoke_error
    or t5, t5, a1

    mv ra, s2
    li a0, 0
    mv a1, t5
    ret

rootfs_rw_smoke_close_error:
    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    j rootfs_rw_smoke_unlink_error

rootfs_rw_smoke_close_read_error:
    mv a0, t0
    li a7, SYS_CLOSE
    ecall

rootfs_rw_smoke_unlink_error:
    li a0, AT_FDCWD
    la a1, post_init_rw_path
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall

rootfs_rw_smoke_error:
    mv ra, s2
    li a0, -1
    li a1, 0
    ret

process_smoke:
    mv s2, ra
    li t5, 0

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, process_smoke_error
    beqz a0, process_smoke_child
    mv t0, a0

    la t1, post_init_wait_status
    sw zero, 0(t1)
    mv a0, t0
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, process_smoke_error
    bne a0, t0, process_smoke_error
    lw t2, 0(t1)
    bnez t2, process_smoke_error

    mv a0, s0
    la a1, stage_parent_wait4_ok_msg
    li a2, stage_parent_wait4_ok_msg_len
    jal ra, write_exact
    bltz a0, process_smoke_error
    or t5, t5, a1

    mv ra, s2
    li a0, 0
    mv a1, t5
    ret

process_smoke_child:
    mv a0, s0
    la a1, stage_fork_child_wrote_msg
    li a2, stage_fork_child_wrote_msg_len
    jal ra, write_exact
    bltz a0, process_smoke_child_fail
    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

process_smoke_child_fail:
    li a0, 1
    li a7, SYS_EXIT_GROUP
    ecall

process_smoke_error:
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
