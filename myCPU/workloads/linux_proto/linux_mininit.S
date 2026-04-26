#define SYS_DUP 23
#define SYS_MKDIRAT 34
#define SYS_MOUNT 40
#define SYS_OPENAT 56
#define SYS_CLOSE 57
#define SYS_READ 63
#define SYS_WRITE 64
#define SYS_EXIT 93
#define SYS_NANOSLEEP 101
#define SYS_EXECVE 221

#define AT_FDCWD -100
#define O_RDONLY 0
#define O_RDWR 2
#define O_CREAT 64
#define O_TRUNC 512

    .section .rodata
dev_dir:
    .asciz "/dev"
console_path:
    .asciz "/dev/console"
proc_dir:
    .asciz "/proc"
proc_cmdline_path:
    .asciz "/proc/cmdline"
sys_dir:
    .asciz "/sys"
sys_cpu_online_path:
    .asciz "/sys/devices/system/cpu/online"
post_init_path:
    .asciz "/post-init-smoke"
rootfs_smoke_path:
    .asciz "/rootfs-smoke.txt"
devtmpfs_name:
    .asciz "devtmpfs"
proc_name:
    .asciz "proc"
sysfs_name:
    .asciz "sysfs"
stage_console_opened_msg:
    .asciz "mycpu linux initrd: stage=console-opened\n"
stage_console_opened_msg_end:
stage_proc_readable_msg:
    .asciz "mycpu linux initrd: stage=proc-readable\n"
stage_proc_readable_msg_end:
stage_sys_readable_msg:
    .asciz "mycpu linux initrd: stage=sys-readable\n"
stage_sys_readable_msg_end:
stage_rootfs_rw_ok_msg:
    .asciz "mycpu linux initrd: stage=rootfs-rw-ok\n"
stage_rootfs_rw_ok_msg_end:
stage_execve_post_init_msg:
    .asciz "mycpu linux initrd: stage=execve-post-init\n"
stage_execve_post_init_msg_end:
boot_ok_msg:
    .asciz "mycpu linux initrd: /init reached\n"
boot_ok_msg_end:
rootfs_smoke_payload:
    .asciz "rootfs-rw-ok\n"
rootfs_smoke_payload_end:
boot_write_short_msg:
    .asciz "mycpu linux initrd: write short\n"
boot_write_short_msg_end:
boot_write_fail_msg:
    .asciz "mycpu linux initrd: write failed\n"
boot_write_fail_msg_end:
boot_rootfs_smoke_fail_msg:
    .asciz "mycpu linux initrd: rootfs smoke failed\n"
boot_rootfs_smoke_fail_msg_end:
boot_proc_smoke_fail_msg:
    .asciz "mycpu linux initrd: proc smoke failed\n"
boot_proc_smoke_fail_msg_end:
boot_sys_smoke_fail_msg:
    .asciz "mycpu linux initrd: sys smoke failed\n"
boot_sys_smoke_fail_msg_end:
boot_exec_post_init_fail_msg:
    .asciz "mycpu linux initrd: exec post-init failed\n"
boot_exec_post_init_fail_msg_end:
boot_console_fail_msg:
    .asciz "mycpu linux initrd: console open failed\n"
boot_console_fail_msg_end:

post_init_argv:
    .dword post_init_path
    .dword 0
empty_envp:
    .dword 0

.set stage_console_opened_msg_len, stage_console_opened_msg_end - stage_console_opened_msg
.set stage_console_opened_msg_len, stage_console_opened_msg_len - 1
.set stage_proc_readable_msg_len, stage_proc_readable_msg_end - stage_proc_readable_msg
.set stage_proc_readable_msg_len, stage_proc_readable_msg_len - 1
.set stage_sys_readable_msg_len, stage_sys_readable_msg_end - stage_sys_readable_msg
.set stage_sys_readable_msg_len, stage_sys_readable_msg_len - 1
.set stage_rootfs_rw_ok_msg_len, stage_rootfs_rw_ok_msg_end - stage_rootfs_rw_ok_msg
.set stage_rootfs_rw_ok_msg_len, stage_rootfs_rw_ok_msg_len - 1
.set stage_execve_post_init_msg_len, stage_execve_post_init_msg_end - stage_execve_post_init_msg
.set stage_execve_post_init_msg_len, stage_execve_post_init_msg_len - 1
.set boot_ok_msg_len, boot_ok_msg_end - boot_ok_msg
.set boot_ok_msg_len, boot_ok_msg_len - 1
.set rootfs_smoke_payload_len, rootfs_smoke_payload_end - rootfs_smoke_payload
.set rootfs_smoke_payload_len, rootfs_smoke_payload_len - 1
.set boot_write_short_msg_len, boot_write_short_msg_end - boot_write_short_msg
.set boot_write_short_msg_len, boot_write_short_msg_len - 1
.set boot_write_fail_msg_len, boot_write_fail_msg_end - boot_write_fail_msg
.set boot_write_fail_msg_len, boot_write_fail_msg_len - 1
.set boot_rootfs_smoke_fail_msg_len, boot_rootfs_smoke_fail_msg_end - boot_rootfs_smoke_fail_msg
.set boot_rootfs_smoke_fail_msg_len, boot_rootfs_smoke_fail_msg_len - 1
.set boot_proc_smoke_fail_msg_len, boot_proc_smoke_fail_msg_end - boot_proc_smoke_fail_msg
.set boot_proc_smoke_fail_msg_len, boot_proc_smoke_fail_msg_len - 1
.set boot_sys_smoke_fail_msg_len, boot_sys_smoke_fail_msg_end - boot_sys_smoke_fail_msg
.set boot_sys_smoke_fail_msg_len, boot_sys_smoke_fail_msg_len - 1
.set boot_exec_post_init_fail_msg_len, boot_exec_post_init_fail_msg_end - boot_exec_post_init_fail_msg
.set boot_exec_post_init_fail_msg_len, boot_exec_post_init_fail_msg_len - 1
.set boot_console_fail_msg_len, boot_console_fail_msg_end - boot_console_fail_msg
.set boot_console_fail_msg_len, boot_console_fail_msg_len - 1
.equ proc_cmdline_buf_len, 128
.equ sys_online_buf_len, 32

sleep_spec:
    .dword 1
    .dword 0

    .section .bss
    .balign 8
rootfs_smoke_read_buf:
    .space rootfs_smoke_payload_len
proc_cmdline_buf:
    .space proc_cmdline_buf_len
sys_online_buf:
    .space sys_online_buf_len

    .section .text
    .globl _start
_start:
    li a0, AT_FDCWD
    la a1, dev_dir
    li a2, 493
    li a7, SYS_MKDIRAT
    ecall

    la a0, devtmpfs_name
    la a1, dev_dir
    la a2, devtmpfs_name
    li a3, 0
    li a4, 0
    li a7, SYS_MOUNT
    ecall
    mv s0, a0

    li a0, AT_FDCWD
    la a1, console_path
    li a2, O_RDWR
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    mv s1, a0
    bltz s1, console_open_failed
    li s2, 0

    # block-rootfs boots may have devtmpfs mounted before /init runs.
    mv a0, s1
    la a1, stage_console_opened_msg
    li a2, stage_console_opened_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s2, s2, a1

    jal ra, rootfs_smoke
    bltz a0, rootfs_smoke_failed
    or s2, s2, a1

    jal ra, proc_smoke
    bltz a0, proc_smoke_failed
    or s2, s2, a1

    jal ra, sys_smoke
    bltz a0, sys_smoke_failed
    or s2, s2, a1

    mv a0, s1
    la a1, boot_ok_msg
    li a2, boot_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s2, s2, a1
    bnez s2, report_write_short

    jal ra, exec_post_init
    bltz a0, exec_post_init_failed
    j sleep_forever

report_write_short:
    mv a0, s1
    la a1, boot_write_short_msg
    li a2, boot_write_short_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    j sleep_forever

console_open_failed:
    li a0, 1
    la a1, boot_console_fail_msg
    li a2, boot_console_fail_msg_len
    li a7, SYS_WRITE
    ecall
    li a0, 1
    li a7, SYS_EXIT
    ecall

proc_smoke_failed:
    mv a0, s1
    la a1, boot_proc_smoke_fail_msg
    li a2, boot_proc_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

sys_smoke_failed:
    mv a0, s1
    la a1, boot_sys_smoke_fail_msg
    li a2, boot_sys_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

exec_post_init_failed:
    mv a0, s1
    la a1, boot_exec_post_init_fail_msg
    li a2, boot_exec_post_init_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

rootfs_smoke_failed:
    mv a0, s1
    la a1, boot_rootfs_smoke_fail_msg
    li a2, boot_rootfs_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

write_failed:
    li a0, 1
    la a1, boot_write_fail_msg
    li a2, boot_write_fail_msg_len
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

rootfs_smoke:
    mv s3, ra
    li t5, 0

    li a0, AT_FDCWD
    la a1, rootfs_smoke_path
    li a2, O_RDWR | O_CREAT | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, rootfs_smoke_error
    mv t0, a0

    mv a0, t0
    la a1, rootfs_smoke_payload
    li a2, rootfs_smoke_payload_len
    jal ra, write_exact
    bltz a0, rootfs_smoke_close_error
    or t5, t5, a1

    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    bltz a0, rootfs_smoke_error

    li a0, AT_FDCWD
    la a1, rootfs_smoke_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, rootfs_smoke_error
    mv t0, a0

    mv a0, t0
    la a1, rootfs_smoke_read_buf
    li a2, rootfs_smoke_payload_len
    li a7, SYS_READ
    ecall
    li t1, rootfs_smoke_payload_len
    bne a0, t1, rootfs_smoke_close_error

    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    bltz a0, rootfs_smoke_error

    la a0, rootfs_smoke_read_buf
    la a1, rootfs_smoke_payload
    li a2, rootfs_smoke_payload_len
    jal ra, compare_bytes
    bltz a0, rootfs_smoke_error

    mv a0, s1
    la a1, stage_rootfs_rw_ok_msg
    li a2, stage_rootfs_rw_ok_msg_len
    jal ra, write_exact
    bltz a0, rootfs_smoke_error
    or t5, t5, a1

    mv ra, s3
    li a0, 0
    mv a1, t5
    ret

rootfs_smoke_close_error:
    mv a0, t0
    li a7, SYS_CLOSE
    ecall

rootfs_smoke_error:
    mv ra, s3
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

proc_smoke:
    mv s4, ra
    li t5, 0

    li a0, AT_FDCWD
    la a1, proc_dir
    li a2, 493
    li a7, SYS_MKDIRAT
    ecall

    la a0, proc_name
    la a1, proc_dir
    la a2, proc_name
    li a3, 0
    li a4, 0
    li a7, SYS_MOUNT
    ecall
    bltz a0, proc_smoke_error

    li a0, AT_FDCWD
    la a1, proc_cmdline_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, proc_smoke_error
    mv t0, a0

    mv a0, t0
    la a1, proc_cmdline_buf
    li a2, proc_cmdline_buf_len
    li a7, SYS_READ
    ecall
    blez a0, proc_smoke_close_error

    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    bltz a0, proc_smoke_error

    mv a0, s1
    la a1, stage_proc_readable_msg
    li a2, stage_proc_readable_msg_len
    jal ra, write_exact
    bltz a0, proc_smoke_error
    or t5, t5, a1

    mv ra, s4
    li a0, 0
    mv a1, t5
    ret

proc_smoke_close_error:
    mv a0, t0
    li a7, SYS_CLOSE
    ecall

proc_smoke_error:
    mv ra, s4
    li a0, -1
    li a1, 0
    ret

sys_smoke:
    mv s5, ra
    li t5, 0

    li a0, AT_FDCWD
    la a1, sys_dir
    li a2, 493
    li a7, SYS_MKDIRAT
    ecall

    # block-rootfs may eventually pre-mount sysfs; open() is the real gate.
    la a0, sysfs_name
    la a1, sys_dir
    la a2, sysfs_name
    li a3, 0
    li a4, 0
    li a7, SYS_MOUNT
    ecall

    li a0, AT_FDCWD
    la a1, sys_cpu_online_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, sys_smoke_error
    mv t0, a0

    mv a0, t0
    la a1, sys_online_buf
    li a2, sys_online_buf_len
    li a7, SYS_READ
    ecall
    blez a0, sys_smoke_close_error

    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    bltz a0, sys_smoke_error

    mv a0, s1
    la a1, stage_sys_readable_msg
    li a2, stage_sys_readable_msg_len
    jal ra, write_exact
    bltz a0, sys_smoke_error
    or t5, t5, a1

    mv ra, s5
    li a0, 0
    mv a1, t5
    ret

sys_smoke_close_error:
    mv a0, t0
    li a7, SYS_CLOSE
    ecall

sys_smoke_error:
    mv ra, s5
    li a0, -1
    li a1, 0
    ret

exec_post_init:
    mv s6, ra

    mv a0, s1
    la a1, stage_execve_post_init_msg
    li a2, stage_execve_post_init_msg_len
    jal ra, write_exact
    bltz a0, exec_post_init_error
    bnez a1, exec_post_init_short

    la a0, post_init_path
    la a1, post_init_argv
    la a2, empty_envp
    li a7, SYS_EXECVE
    ecall

    mv ra, s6
    ret

exec_post_init_short:
    li a0, -1

exec_post_init_error:
    mv ra, s6
    ret

sleep_forever:
    la a0, sleep_spec
    li a1, 0
    li a7, SYS_NANOSLEEP
    ecall
    j sleep_forever
