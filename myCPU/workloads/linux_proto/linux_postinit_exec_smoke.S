#define SYS_MKDIRAT 34
#define SYS_UNLINKAT 35
#define SYS_RENAMEAT2 276
#define SYS_CHDIR 49
#define SYS_OPENAT 56
#define SYS_CLOSE 57
#define SYS_GETDENTS64 61
#define SYS_READ 63
#define SYS_WRITE 64
#define SYS_NEWFSTATAT 79
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
current_dir_path:
    .asciz "."
parent_dir_path:
    .asciz ".."
third_stage_dir_path:
    .asciz "/post-init-dir-smoke"
third_stage_dir_name:
    .asciz "post-init-dir-smoke"
third_stage_nested_relative_path:
    .asciz "nested.txt"
third_stage_nested_absolute_path:
    .asciz "/post-init-dir-smoke/nested.txt"
third_stage_renamed_relative_path:
    .asciz "renamed.txt"
third_stage_renamed_absolute_path:
    .asciz "/post-init-dir-smoke/renamed.txt"
third_stage_nested_expected:
    .asciz "third-stage-nested-ok\n"
third_stage_nested_expected_end:
stage_mkdir_chdir_ok_msg:
    .asciz "mycpu linux userland: stage=mkdir-chdir-ok\n"
stage_mkdir_chdir_ok_msg_end:
stage_nested_file_roundtrip_ok_msg:
    .asciz "mycpu linux userland: stage=nested-file-roundtrip-ok\n"
stage_nested_file_roundtrip_ok_msg_end:
stage_getdents64_nested_visible_msg:
    .asciz "mycpu linux userland: stage=getdents64-nested-visible\n"
stage_getdents64_nested_visible_msg_end:
stage_fstatat_nested_stat_ok_msg:
    .asciz "mycpu linux userland: stage=fstatat-nested-stat-ok\n"
stage_fstatat_nested_stat_ok_msg_end:
stage_renameat2_nested_ok_msg:
    .asciz "mycpu linux userland: stage=renameat2-nested-ok\n"
stage_renameat2_nested_ok_msg_end:
stage_renameat2_dirent_updated_msg:
    .asciz "mycpu linux userland: stage=renameat2-dirent-updated\n"
stage_renameat2_dirent_updated_msg_end:
stage_renameat2_syscall_ok_msg:
    .asciz "mycpu linux userland: stage=renameat2-syscall-ok\n"
stage_renameat2_syscall_ok_msg_end:
stage_third_stage_reached_msg:
    .asciz "mycpu linux userland: stage=third-stage-reached\n"
stage_third_stage_reached_msg_end:
stage_renameat2_cleanup_ok_msg:
    .asciz "mycpu linux userland: stage=renameat2-cleanup-ok\n"
stage_renameat2_cleanup_ok_msg_end:
stage_unlinkat_parent_dirent_gone_msg:
    .asciz "mycpu linux userland: stage=unlinkat-parent-dirent-gone\n"
stage_unlinkat_parent_dirent_gone_msg_end:
stage_mkdirat_dir_name_reusable_msg:
    .asciz "mycpu linux userland: stage=mkdirat-dir-name-reusable\n"
stage_mkdirat_dir_name_reusable_msg_end:
stage_mkdirat_reused_dir_empty_msg:
    .asciz "mycpu linux userland: stage=mkdirat-reused-dir-empty\n"
stage_mkdirat_reused_dir_empty_msg_end:
stage_mkdirat_reused_dir_dot_only_msg:
    .asciz "mycpu linux userland: stage=mkdirat-reused-dir-dot-only\n"
stage_mkdirat_reused_dir_dot_only_msg_end:
stage_mkdirat_reused_dir_parent_stat_ok_msg:
    .asciz "mycpu linux userland: stage=mkdirat-reused-dir-parent-stat-ok\n"
stage_mkdirat_reused_dir_parent_stat_ok_msg_end:
post_init_reached_msg:
    .asciz "mycpu linux userland: post-init reached\n"
post_init_reached_msg_end:
third_stage_dir_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage dir smoke failed\n"
third_stage_dir_smoke_fail_msg_end:
third_stage_nested_file_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage nested file smoke failed\n"
third_stage_nested_file_smoke_fail_msg_end:
third_stage_getdents64_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage getdents64 smoke failed\n"
third_stage_getdents64_smoke_fail_msg_end:
third_stage_fstatat_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage fstatat smoke failed\n"
third_stage_fstatat_smoke_fail_msg_end:
third_stage_renameat2_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage renameat2 smoke failed\n"
third_stage_renameat2_smoke_fail_msg_end:
third_stage_renameat2_dirent_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage renameat2 dirent smoke failed\n"
third_stage_renameat2_dirent_smoke_fail_msg_end:
third_stage_renameat2_cleanup_fail_msg:
    .asciz "mycpu linux userland: third-stage renameat2 cleanup failed\n"
third_stage_renameat2_cleanup_fail_msg_end:
third_stage_unlinkat_dirent_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage unlinkat dirent smoke failed\n"
third_stage_unlinkat_dirent_smoke_fail_msg_end:
third_stage_mkdirat_reuse_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage mkdirat reuse smoke failed\n"
third_stage_mkdirat_reuse_smoke_fail_msg_end:
third_stage_mkdirat_reused_dir_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage mkdirat reused dir smoke failed\n"
third_stage_mkdirat_reused_dir_smoke_fail_msg_end:
third_stage_mkdirat_reused_dir_dot_only_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage mkdirat reused dir dot-only smoke failed\n"
third_stage_mkdirat_reused_dir_dot_only_smoke_fail_msg_end:
third_stage_mkdirat_reused_dir_parent_stat_smoke_fail_msg:
    .asciz "mycpu linux userland: third-stage mkdirat reused dir parent stat smoke failed\n"
third_stage_mkdirat_reused_dir_parent_stat_smoke_fail_msg_end:
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
.set stage_getdents64_nested_visible_msg_len, stage_getdents64_nested_visible_msg_end - stage_getdents64_nested_visible_msg
.set stage_getdents64_nested_visible_msg_len, stage_getdents64_nested_visible_msg_len - 1
.set stage_fstatat_nested_stat_ok_msg_len, stage_fstatat_nested_stat_ok_msg_end - stage_fstatat_nested_stat_ok_msg
.set stage_fstatat_nested_stat_ok_msg_len, stage_fstatat_nested_stat_ok_msg_len - 1
.set stage_renameat2_nested_ok_msg_len, stage_renameat2_nested_ok_msg_end - stage_renameat2_nested_ok_msg
.set stage_renameat2_nested_ok_msg_len, stage_renameat2_nested_ok_msg_len - 1
.set stage_renameat2_dirent_updated_msg_len, stage_renameat2_dirent_updated_msg_end - stage_renameat2_dirent_updated_msg
.set stage_renameat2_dirent_updated_msg_len, stage_renameat2_dirent_updated_msg_len - 1
.set stage_renameat2_syscall_ok_msg_len, stage_renameat2_syscall_ok_msg_end - stage_renameat2_syscall_ok_msg
.set stage_renameat2_syscall_ok_msg_len, stage_renameat2_syscall_ok_msg_len - 1
.set stage_third_stage_reached_msg_len, stage_third_stage_reached_msg_end - stage_third_stage_reached_msg
.set stage_third_stage_reached_msg_len, stage_third_stage_reached_msg_len - 1
.set stage_renameat2_cleanup_ok_msg_len, stage_renameat2_cleanup_ok_msg_end - stage_renameat2_cleanup_ok_msg
.set stage_renameat2_cleanup_ok_msg_len, stage_renameat2_cleanup_ok_msg_len - 1
.set stage_unlinkat_parent_dirent_gone_msg_len, stage_unlinkat_parent_dirent_gone_msg_end - stage_unlinkat_parent_dirent_gone_msg
.set stage_unlinkat_parent_dirent_gone_msg_len, stage_unlinkat_parent_dirent_gone_msg_len - 1
.set stage_mkdirat_dir_name_reusable_msg_len, stage_mkdirat_dir_name_reusable_msg_end - stage_mkdirat_dir_name_reusable_msg
.set stage_mkdirat_dir_name_reusable_msg_len, stage_mkdirat_dir_name_reusable_msg_len - 1
.set stage_mkdirat_reused_dir_empty_msg_len, stage_mkdirat_reused_dir_empty_msg_end - stage_mkdirat_reused_dir_empty_msg
.set stage_mkdirat_reused_dir_empty_msg_len, stage_mkdirat_reused_dir_empty_msg_len - 1
.set stage_mkdirat_reused_dir_dot_only_msg_len, stage_mkdirat_reused_dir_dot_only_msg_end - stage_mkdirat_reused_dir_dot_only_msg
.set stage_mkdirat_reused_dir_dot_only_msg_len, stage_mkdirat_reused_dir_dot_only_msg_len - 1
.set stage_mkdirat_reused_dir_parent_stat_ok_msg_len, stage_mkdirat_reused_dir_parent_stat_ok_msg_end - stage_mkdirat_reused_dir_parent_stat_ok_msg
.set stage_mkdirat_reused_dir_parent_stat_ok_msg_len, stage_mkdirat_reused_dir_parent_stat_ok_msg_len - 1
.set post_init_reached_msg_len, post_init_reached_msg_end - post_init_reached_msg
.set post_init_reached_msg_len, post_init_reached_msg_len - 1
.set third_stage_dir_smoke_fail_msg_len, third_stage_dir_smoke_fail_msg_end - third_stage_dir_smoke_fail_msg
.set third_stage_dir_smoke_fail_msg_len, third_stage_dir_smoke_fail_msg_len - 1
.set third_stage_nested_file_smoke_fail_msg_len, third_stage_nested_file_smoke_fail_msg_end - third_stage_nested_file_smoke_fail_msg
.set third_stage_nested_file_smoke_fail_msg_len, third_stage_nested_file_smoke_fail_msg_len - 1
.set third_stage_getdents64_smoke_fail_msg_len, third_stage_getdents64_smoke_fail_msg_end - third_stage_getdents64_smoke_fail_msg
.set third_stage_getdents64_smoke_fail_msg_len, third_stage_getdents64_smoke_fail_msg_len - 1
.set third_stage_fstatat_smoke_fail_msg_len, third_stage_fstatat_smoke_fail_msg_end - third_stage_fstatat_smoke_fail_msg
.set third_stage_fstatat_smoke_fail_msg_len, third_stage_fstatat_smoke_fail_msg_len - 1
.set third_stage_renameat2_smoke_fail_msg_len, third_stage_renameat2_smoke_fail_msg_end - third_stage_renameat2_smoke_fail_msg
.set third_stage_renameat2_smoke_fail_msg_len, third_stage_renameat2_smoke_fail_msg_len - 1
.set third_stage_renameat2_dirent_smoke_fail_msg_len, third_stage_renameat2_dirent_smoke_fail_msg_end - third_stage_renameat2_dirent_smoke_fail_msg
.set third_stage_renameat2_dirent_smoke_fail_msg_len, third_stage_renameat2_dirent_smoke_fail_msg_len - 1
.set third_stage_renameat2_cleanup_fail_msg_len, third_stage_renameat2_cleanup_fail_msg_end - third_stage_renameat2_cleanup_fail_msg
.set third_stage_renameat2_cleanup_fail_msg_len, third_stage_renameat2_cleanup_fail_msg_len - 1
.set third_stage_unlinkat_dirent_smoke_fail_msg_len, third_stage_unlinkat_dirent_smoke_fail_msg_end - third_stage_unlinkat_dirent_smoke_fail_msg
.set third_stage_unlinkat_dirent_smoke_fail_msg_len, third_stage_unlinkat_dirent_smoke_fail_msg_len - 1
.set third_stage_mkdirat_reuse_smoke_fail_msg_len, third_stage_mkdirat_reuse_smoke_fail_msg_end - third_stage_mkdirat_reuse_smoke_fail_msg
.set third_stage_mkdirat_reuse_smoke_fail_msg_len, third_stage_mkdirat_reuse_smoke_fail_msg_len - 1
.set third_stage_mkdirat_reused_dir_smoke_fail_msg_len, third_stage_mkdirat_reused_dir_smoke_fail_msg_end - third_stage_mkdirat_reused_dir_smoke_fail_msg
.set third_stage_mkdirat_reused_dir_smoke_fail_msg_len, third_stage_mkdirat_reused_dir_smoke_fail_msg_len - 1
.set third_stage_mkdirat_reused_dir_dot_only_smoke_fail_msg_len, third_stage_mkdirat_reused_dir_dot_only_smoke_fail_msg_end - third_stage_mkdirat_reused_dir_dot_only_smoke_fail_msg
.set third_stage_mkdirat_reused_dir_dot_only_smoke_fail_msg_len, third_stage_mkdirat_reused_dir_dot_only_smoke_fail_msg_len - 1
.set third_stage_mkdirat_reused_dir_parent_stat_smoke_fail_msg_len, third_stage_mkdirat_reused_dir_parent_stat_smoke_fail_msg_end - third_stage_mkdirat_reused_dir_parent_stat_smoke_fail_msg
.set third_stage_mkdirat_reused_dir_parent_stat_smoke_fail_msg_len, third_stage_mkdirat_reused_dir_parent_stat_smoke_fail_msg_len - 1
.set third_stage_console_fail_msg_len, third_stage_console_fail_msg_end - third_stage_console_fail_msg
.set third_stage_console_fail_msg_len, third_stage_console_fail_msg_len - 1
.set third_stage_write_fail_msg_len, third_stage_write_fail_msg_end - third_stage_write_fail_msg
.set third_stage_write_fail_msg_len, third_stage_write_fail_msg_len - 1
.set third_stage_write_short_msg_len, third_stage_write_short_msg_end - third_stage_write_short_msg
.set third_stage_write_short_msg_len, third_stage_write_short_msg_len - 1
.set third_stage_dirent_buf_len, 1024
.set third_stage_stat_buf_len, 128
.set third_stage_stat_st_mode_offset, 16
.set third_stage_stat_st_size_offset, 48
.set third_stage_stat_mode_mask, 61440
.set third_stage_stat_mode_dir, 16384
.set third_stage_stat_mode_regular, 32768

sleep_spec:
    .dword 1
    .dword 0

    .section .bss
third_stage_nested_buf:
    .space third_stage_nested_expected_len
third_stage_dirent_buf:
    .space third_stage_dirent_buf_len
third_stage_stat_buf:
    .space third_stage_stat_buf_len

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

    jal ra, getdents64_smoke
    bltz a0, getdents64_smoke_failed
    or s1, s1, a1

    jal ra, fstatat_smoke
    bltz a0, fstatat_smoke_failed
    or s1, s1, a1

    jal ra, renameat_smoke
    li t0, -5
    beq a0, t0, mkdirat_reused_dir_smoke_failed
    li t0, -4
    beq a0, t0, mkdirat_reuse_smoke_failed
    li t0, -3
    beq a0, t0, unlinkat_dirent_smoke_failed
    li t0, -2
    beq a0, t0, renameat2_dirent_smoke_failed
    bltz a0, renameat2_smoke_failed
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

getdents64_smoke_failed:
    mv a0, s0
    la a1, third_stage_getdents64_smoke_fail_msg
    li a2, third_stage_getdents64_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

fstatat_smoke_failed:
    mv a0, s0
    la a1, third_stage_fstatat_smoke_fail_msg
    li a2, third_stage_fstatat_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

renameat2_smoke_failed:
    mv a0, s0
    la a1, third_stage_renameat2_smoke_fail_msg
    li a2, third_stage_renameat2_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

renameat2_dirent_smoke_failed:
    mv a0, s0
    la a1, third_stage_renameat2_dirent_smoke_fail_msg
    li a2, third_stage_renameat2_dirent_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

unlinkat_dirent_smoke_failed:
    mv a0, s0
    la a1, third_stage_unlinkat_dirent_smoke_fail_msg
    li a2, third_stage_unlinkat_dirent_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

mkdirat_reuse_smoke_failed:
    mv a0, s0
    la a1, third_stage_mkdirat_reuse_smoke_fail_msg
    li a2, third_stage_mkdirat_reuse_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

mkdirat_reused_dir_smoke_failed:
    mv a0, s0
    la a1, third_stage_mkdirat_reused_dir_smoke_fail_msg
    li a2, third_stage_mkdirat_reused_dir_smoke_fail_msg_len
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

cleanup_dir_smoke_paths:
    la a0, root_dir_path
    li a7, SYS_CHDIR
    ecall

    li a0, AT_FDCWD
    la a1, third_stage_renamed_absolute_path
    li a2, 0
    li a7, SYS_UNLINKAT
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

getdents64_smoke:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s3, 16(sp)
    sd s4, 8(sp)

    li s3, 0
    li s4, -1

    li a0, AT_FDCWD
    la a1, current_dir_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, getdents64_smoke_cleanup_error
    mv s4, a0

getdents64_read_loop:
    mv a0, s4
    la a1, third_stage_dirent_buf
    li a2, third_stage_dirent_buf_len
    li a7, SYS_GETDENTS64
    ecall
    bltz a0, getdents64_smoke_close_dir_error
    beqz a0, getdents64_smoke_close_not_found
    mv t0, a0
    la t1, third_stage_dirent_buf

getdents64_parse_loop:
    beqz t0, getdents64_read_loop
    lhu t2, 16(t1)
    li t3, 19
    bltu t2, t3, getdents64_smoke_close_dir_error
    bltu t0, t2, getdents64_smoke_close_dir_error
    addi t3, t1, 19
    la t4, third_stage_nested_relative_path

getdents64_name_loop:
    lbu t5, 0(t3)
    lbu t6, 0(t4)
    bne t5, t6, getdents64_next_entry
    beqz t6, getdents64_found_entry
    addi t3, t3, 1
    addi t4, t4, 1
    j getdents64_name_loop

getdents64_next_entry:
    add t1, t1, t2
    sub t0, t0, t2
    j getdents64_parse_loop

getdents64_found_entry:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    bltz a0, getdents64_smoke_cleanup_error
    li s4, -1

    mv a0, s0
    la a1, stage_getdents64_nested_visible_msg
    li a2, stage_getdents64_nested_visible_msg_len
    jal ra, write_exact
    bltz a0, getdents64_smoke_cleanup_error
    or s3, s3, a1

    li a0, 0
    mv a1, s3
    j getdents64_smoke_done

getdents64_smoke_close_not_found:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    li s4, -1
    j getdents64_smoke_cleanup_error

getdents64_smoke_close_dir_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    li s4, -1

getdents64_smoke_cleanup_error:
    li t0, -1
    beq s4, t0, getdents64_smoke_do_cleanup
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    li s4, -1

getdents64_smoke_do_cleanup:
    jal ra, cleanup_dir_smoke_paths
    li a0, -1
    li a1, 0

getdents64_smoke_done:
    ld s4, 8(sp)
    ld s3, 16(sp)
    ld ra, 24(sp)
    addi sp, sp, 32
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
    la a1, third_stage_dirent_buf
    li a2, third_stage_dirent_buf_len
    li a7, SYS_GETDENTS64
    ecall
    bltz a0, dir_contains_name_close_error
    beqz a0, dir_contains_name_close_not_found
    mv s6, a0
    la s3, third_stage_dirent_buf

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

fstatat_smoke:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s3, 0(sp)

    li s3, 0

    li a0, AT_FDCWD
    la a1, third_stage_nested_relative_path
    la a2, third_stage_stat_buf
    li a3, 0
    li a7, SYS_NEWFSTATAT
    ecall
    bltz a0, fstatat_smoke_cleanup_error

    la t0, third_stage_stat_buf
    lw t1, third_stage_stat_st_mode_offset(t0)
    li t2, third_stage_stat_mode_mask
    and t1, t1, t2
    li t2, third_stage_stat_mode_regular
    bne t1, t2, fstatat_smoke_cleanup_error

    ld t1, third_stage_stat_st_size_offset(t0)
    li t2, third_stage_nested_expected_len
    bne t1, t2, fstatat_smoke_cleanup_error

    mv a0, s0
    la a1, stage_fstatat_nested_stat_ok_msg
    li a2, stage_fstatat_nested_stat_ok_msg_len
    jal ra, write_exact
    bltz a0, fstatat_smoke_cleanup_error
    or s3, s3, a1

    li a0, 0
    mv a1, s3
    j fstatat_smoke_done

fstatat_smoke_cleanup_error:
    jal ra, cleanup_dir_smoke_paths
    li a0, -1
    li a1, 0

fstatat_smoke_done:
    ld s3, 0(sp)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

renameat_smoke:
    addi sp, sp, -24
    sd ra, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)

    li s3, 0
    li s4, -1

    li a0, AT_FDCWD
    la a1, third_stage_nested_relative_path
    li a2, AT_FDCWD
    la a3, third_stage_renamed_relative_path
    li a4, 0
    li a7, SYS_RENAMEAT2
    ecall
    bltz a0, renameat_smoke_cleanup_error

    mv a0, s0
    la a1, stage_renameat2_syscall_ok_msg
    li a2, stage_renameat2_syscall_ok_msg_len
    jal ra, write_exact
    bltz a0, renameat_smoke_cleanup_error
    or s3, s3, a1

    li a0, AT_FDCWD
    la a1, third_stage_renamed_relative_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, renameat_smoke_cleanup_error
    mv s4, a0

    mv a0, s4
    la a1, third_stage_nested_buf
    li a2, third_stage_nested_expected_len
    li a7, SYS_READ
    ecall
    li t1, third_stage_nested_expected_len
    bne a0, t1, renameat_smoke_close_read_error

    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    bltz a0, renameat_smoke_cleanup_error
    li s4, -1

    la a0, third_stage_nested_buf
    la a1, third_stage_nested_expected
    li a2, third_stage_nested_expected_len
    jal ra, compare_bytes
    bltz a0, renameat_smoke_cleanup_error

    li a0, AT_FDCWD
    la a1, third_stage_nested_relative_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, renameat_smoke_old_name_missing
    mv s4, a0
    j renameat_smoke_old_name_still_exists

renameat_smoke_old_name_missing:
    mv a0, s0
    la a1, stage_renameat2_nested_ok_msg
    li a2, stage_renameat2_nested_ok_msg_len
    jal ra, write_exact
    bltz a0, renameat_smoke_cleanup_error
    or s3, s3, a1

    la a0, third_stage_renamed_relative_path
    jal ra, dir_contains_name
    li t0, 1
    bne a0, t0, renameat_smoke_cleanup_dirent_error

    la a0, third_stage_nested_relative_path
    jal ra, dir_contains_name
    bnez a0, renameat_smoke_cleanup_dirent_error

    mv a0, s0
    la a1, stage_renameat2_dirent_updated_msg
    li a2, stage_renameat2_dirent_updated_msg_len
    jal ra, write_exact
    bltz a0, renameat_smoke_cleanup_dirent_error
    or s3, s3, a1

    jal ra, cleanup_dir_smoke_paths

    li a0, AT_FDCWD
    la a1, third_stage_renamed_absolute_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, renameat_smoke_cleanup_check_dir
    mv s4, a0
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    j renameat_smoke_cleanup_verify_error

renameat_smoke_cleanup_check_dir:
    li a0, AT_FDCWD
    la a1, third_stage_dir_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, renameat_smoke_cleanup_verified
    mv s4, a0
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    j renameat_smoke_cleanup_verify_error

renameat_smoke_cleanup_verified:
    mv a0, s0
    la a1, stage_renameat2_cleanup_ok_msg
    li a2, stage_renameat2_cleanup_ok_msg_len
    jal ra, write_exact
    bltz a0, renameat_smoke_cleanup_verify_error
    or s3, s3, a1

    la a0, third_stage_dir_name
    jal ra, dir_contains_name
    bnez a0, renameat_smoke_unlinkat_dirent_error

    mv a0, s0
    la a1, stage_unlinkat_parent_dirent_gone_msg
    li a2, stage_unlinkat_parent_dirent_gone_msg_len
    jal ra, write_exact
    bltz a0, renameat_smoke_unlinkat_dirent_error
    or s3, s3, a1

    li a0, AT_FDCWD
    la a1, third_stage_dir_path
    li a2, 493
    li a7, SYS_MKDIRAT
    ecall
    bltz a0, renameat_smoke_mkdirat_reuse_error

    la a0, third_stage_dir_name
    jal ra, dir_contains_name
    li t0, 1
    bne a0, t0, renameat_smoke_mkdirat_reuse_error

    mv a0, s0
    la a1, stage_mkdirat_dir_name_reusable_msg
    li a2, stage_mkdirat_dir_name_reusable_msg_len
    jal ra, write_exact
    bltz a0, renameat_smoke_mkdirat_reuse_error
    or s3, s3, a1

    la a0, third_stage_dir_path
    li a7, SYS_CHDIR
    ecall
    bltz a0, renameat_smoke_reused_dir_error

    la a0, third_stage_nested_relative_path
    jal ra, dir_contains_name
    bnez a0, renameat_smoke_reused_dir_error

    la a0, third_stage_renamed_relative_path
    jal ra, dir_contains_name
    bnez a0, renameat_smoke_reused_dir_error

    mv a0, s0
    la a1, stage_mkdirat_reused_dir_empty_msg
    li a2, stage_mkdirat_reused_dir_empty_msg_len
    jal ra, write_exact
    bltz a0, renameat_smoke_reused_dir_error
    or s3, s3, a1

    jal ra, reused_dir_dot_only_smoke
    bltz a0, renameat_smoke_reused_dir_dot_only_error
    or s3, s3, a1

    jal ra, reused_dir_parent_stat_smoke
    bltz a0, renameat_smoke_reused_dir_parent_stat_error
    or s3, s3, a1

    jal ra, cleanup_dir_smoke_paths

    li a0, 0
    mv a1, s3
    j renameat_smoke_done

renameat_smoke_old_name_still_exists:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    li s4, -1
    j renameat_smoke_cleanup_error

renameat_smoke_close_read_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    li s4, -1

renameat_smoke_cleanup_dirent_error:
    jal ra, cleanup_dir_smoke_paths
    li a0, -2
    li a1, 0
    j renameat_smoke_done

renameat_smoke_cleanup_verify_error:
    mv a0, s0
    la a1, third_stage_renameat2_cleanup_fail_msg
    li a2, third_stage_renameat2_cleanup_fail_msg_len
    jal ra, write_exact
    li a0, -2
    li a1, 0
    j renameat_smoke_done

renameat_smoke_unlinkat_dirent_error:
    li a0, -3
    li a1, 0
    j renameat_smoke_done

renameat_smoke_mkdirat_reuse_error:
    jal ra, cleanup_dir_smoke_paths
    li a0, -4
    li a1, 0
    j renameat_smoke_done

renameat_smoke_reused_dir_error:
    jal ra, cleanup_dir_smoke_paths
    li a0, -5
    li a1, 0
    j renameat_smoke_done

renameat_smoke_reused_dir_dot_only_error:
    mv a0, s0
    la a1, third_stage_mkdirat_reused_dir_dot_only_smoke_fail_msg
    li a2, third_stage_mkdirat_reused_dir_dot_only_smoke_fail_msg_len
    jal ra, write_exact
    li a0, -6
    li a1, 0
    j renameat_smoke_done

renameat_smoke_reused_dir_parent_stat_error:
    mv a0, s0
    la a1, third_stage_mkdirat_reused_dir_parent_stat_smoke_fail_msg
    li a2, third_stage_mkdirat_reused_dir_parent_stat_smoke_fail_msg_len
    jal ra, write_exact
    li a0, -7
    li a1, 0
    j renameat_smoke_done

renameat_smoke_cleanup_error:
    li t0, -1
    beq s4, t0, renameat_smoke_do_cleanup
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    li s4, -1

renameat_smoke_do_cleanup:
    jal ra, cleanup_dir_smoke_paths
    li a0, -1
    li a1, 0

renameat_smoke_done:
    ld s4, 0(sp)
    ld s3, 8(sp)
    ld ra, 16(sp)
    addi sp, sp, 24
    ret

reused_dir_dot_only_smoke:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s3, 32(sp)
    sd s4, 24(sp)
    sd s5, 16(sp)
    sd s6, 8(sp)

    li s3, 0
    li s4, -1

    li a0, AT_FDCWD
    la a1, current_dir_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, reused_dir_dot_only_smoke_cleanup_error
    mv s4, a0
    mv s5, a0
    la s6, third_stage_dirent_buf

reused_dir_dot_only_read_loop:
    mv a0, s4
    la a1, third_stage_dirent_buf
    li a2, third_stage_dirent_buf_len
    li a7, SYS_GETDENTS64
    ecall
    bltz a0, reused_dir_dot_only_smoke_close_error
    beqz a0, reused_dir_dot_only_smoke_eof
    mv s5, a0
    la s6, third_stage_dirent_buf

reused_dir_dot_only_parse_loop:
    beqz s5, reused_dir_dot_only_read_loop
    lhu t2, 16(s6)
    li t3, 19
    bltu t2, t3, reused_dir_dot_only_smoke_close_error
    bltu s5, t2, reused_dir_dot_only_smoke_close_error
    addi a0, s6, 19
    la a1, current_dir_path
    jal ra, compare_cstr
    beqz a0, reused_dir_dot_only_seen_dot
    addi a0, s6, 19
    la a1, parent_dir_path
    jal ra, compare_cstr
    beqz a0, reused_dir_dot_only_seen_dotdot
    j reused_dir_dot_only_smoke_close_error

reused_dir_dot_only_seen_dot:
    li t4, 1
    or s3, s3, t4
    j reused_dir_dot_only_next_entry

reused_dir_dot_only_seen_dotdot:
    li t4, 2
    or s3, s3, t4

reused_dir_dot_only_next_entry:
    add s6, s6, t2
    sub s5, s5, t2
    j reused_dir_dot_only_parse_loop

reused_dir_dot_only_smoke_eof:
    li t4, 3
    bne s3, t4, reused_dir_dot_only_smoke_close_error

    li a0, AT_FDCWD
    la a1, current_dir_path
    la a2, third_stage_stat_buf
    li a3, 0
    li a7, SYS_NEWFSTATAT
    ecall
    bltz a0, reused_dir_dot_only_smoke_close_error

    la t0, third_stage_stat_buf
    lw t1, third_stage_stat_st_mode_offset(t0)
    li t2, third_stage_stat_mode_mask
    and t1, t1, t2
    li t2, third_stage_stat_mode_dir
    bne t1, t2, reused_dir_dot_only_smoke_close_error

    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    bltz a0, reused_dir_dot_only_smoke_cleanup_error
    li s4, -1

    mv a0, s0
    la a1, stage_mkdirat_reused_dir_dot_only_msg
    li a2, stage_mkdirat_reused_dir_dot_only_msg_len
    jal ra, write_exact
    bltz a0, reused_dir_dot_only_smoke_cleanup_error
    li a0, 0
    li a1, 1
    j reused_dir_dot_only_smoke_done

reused_dir_dot_only_smoke_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    li s4, -1

reused_dir_dot_only_smoke_cleanup_error:
    jal ra, cleanup_dir_smoke_paths
    li a0, -1
    li a1, 0

reused_dir_dot_only_smoke_done:
    ld s6, 8(sp)
    ld s5, 16(sp)
    ld s4, 24(sp)
    ld s3, 32(sp)
    ld ra, 40(sp)
    addi sp, sp, 48
    ret

reused_dir_parent_stat_smoke:
    addi sp, sp, -16
    sd ra, 8(sp)

    li a0, AT_FDCWD
    la a1, parent_dir_path
    la a2, third_stage_stat_buf
    li a3, 0
    li a7, SYS_NEWFSTATAT
    ecall
    bltz a0, reused_dir_parent_stat_smoke_cleanup_error

    la t0, third_stage_stat_buf
    lw t1, third_stage_stat_st_mode_offset(t0)
    li t2, third_stage_stat_mode_mask
    and t1, t1, t2
    li t2, third_stage_stat_mode_dir
    bne t1, t2, reused_dir_parent_stat_smoke_cleanup_error

    mv a0, s0
    la a1, stage_mkdirat_reused_dir_parent_stat_ok_msg
    li a2, stage_mkdirat_reused_dir_parent_stat_ok_msg_len
    jal ra, write_exact
    bltz a0, reused_dir_parent_stat_smoke_cleanup_error

    li a0, 0
    li a1, 1
    j reused_dir_parent_stat_smoke_done

reused_dir_parent_stat_smoke_cleanup_error:
    jal ra, cleanup_dir_smoke_paths
    li a0, -1
    li a1, 0

reused_dir_parent_stat_smoke_done:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

sleep_forever:
    la a0, sleep_spec
    li a1, 0
    li a7, SYS_NANOSLEEP
    ecall
    j sleep_forever
