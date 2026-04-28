#define SYS_CLONE3 435
#define SYS_DUP 23
#define SYS_EXECVE 221
#define SYS_WAIT4 260
#define SYS_EXIT_GROUP 94
#define SYS_UNLINKAT 35
#define SYS_SYMLINKAT 36
#define SYS_LINKAT 37
#define SYS_FTRUNCATE 46
#define SYS_CHDIR 49
#define SYS_OPENAT 56
#define SYS_CLOSE 57
#define SYS_GETDENTS64 61
#define SYS_LSEEK 62
#define SYS_READ 63
#define SYS_WRITE 64
#define SYS_READLINKAT 78
#define SYS_NEWFSTATAT 79
#define SYS_FSTAT 80
#define SYS_EXIT 93
#define SYS_NANOSLEEP 101

#define ENOENT 2
#define SIGCHLD 17

#define AT_FDCWD -100
#define AT_REMOVEDIR 512
#define AT_SYMLINK_NOFOLLOW 256
#define O_RDONLY 0
#define O_RDWR 2
#define SEEK_SET 0
#define SEEK_END 2

#define INHERITED_CONSOLE_FD 3

    .section .rodata
console_path:
    .asciz "/dev/console"
post_init_data_path:
    .asciz "/post-init-data.txt"
post_init_data_path_end:
post_init_data_name:
    .asciz "post-init-data.txt"
post_init_data_expected:
    .asciz "post-init-data-ok\n"
post_init_data_expected_end:
post_init_data_anonymous_expected:
    .asciz "post-init-anon-ok\n"
post_init_data_anonymous_expected_end:
post_init_data_dup_expected:
    .asciz "post-init-dupw-ok\n"
post_init_data_dup_expected_end:
post_init_data_dup_truncate_expected:
    .asciz "post-trunc-ok\n"
post_init_data_dup_truncate_expected_end:
post_init_data_dup_append_tail:
    .asciz "append-ok\n"
post_init_data_dup_append_tail_end:
post_init_data_dup_append_expected:
    .asciz "post-trunc-ok\nappend-ok\n"
post_init_data_dup_append_expected_end:
post_init_data_dup_fork_expected:
    .asciz "post-fork-dup-ok\nsyncok\n"
post_init_data_dup_fork_expected_end:
post_init_data_execve_expected:
    .asciz "post-exec-dup-ok\nsyncok\n"
post_init_data_execve_expected_end:
execve_open_fd_self_path:
    .asciz "/post-init-cleanup-smoke"
execve_open_fd_mode_arg:
    .asciz "--exec-open-fd"
post_init_data_link_path:
    .asciz "/post-init-data-link"
post_init_data_link_name:
    .asciz "post-init-data-link"
post_init_data_hardlink_path:
    .asciz "/post-init-data-hardlink"
post_init_data_hardlink_name:
    .asciz "post-init-data-hardlink"
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
stage_fourth_stage_entered_msg:
    .asciz "mycpu linux userland: stage=fourth-stage-entered\n"
stage_fourth_stage_entered_msg_end:
stage_fourth_stage_console_opened_msg:
    .asciz "mycpu linux userland: stage=fourth-stage-console-opened\n"
stage_fourth_stage_console_opened_msg_end:
stage_fourth_stage_console_fallback_msg:
    .asciz "mycpu linux userland: stage=fourth-stage-console-fallback\n"
stage_fourth_stage_console_fallback_msg_end:
stage_fourth_stage_root_chdir_ok_msg:
    .asciz "mycpu linux userland: stage=fourth-stage-root-chdir-ok\n"
stage_fourth_stage_root_chdir_ok_msg_end:
stage_fourth_stage_unlinkat_reused_dir_ok_msg:
    .asciz "mycpu linux userland: stage=fourth-stage-unlinkat-reused-dir-ok\n"
stage_fourth_stage_unlinkat_reused_dir_ok_msg_end:
stage_symlinkat_target_readable_msg:
    .asciz "mycpu linux userland: stage=symlinkat-target-readable\n"
stage_symlinkat_target_readable_msg_end:
stage_fstatat_symlink_nofollow_ok_msg:
    .asciz "mycpu linux userland: stage=fstatat-symlink-nofollow-ok\n"
stage_fstatat_symlink_nofollow_ok_msg_end:
stage_readlinkat_target_ok_msg:
    .asciz "mycpu linux userland: stage=readlinkat-target-ok\n"
stage_readlinkat_target_ok_msg_end:
stage_unlinkat_symlink_dirent_gone_msg:
    .asciz "mycpu linux userland: stage=unlinkat-symlink-dirent-gone\n"
stage_unlinkat_symlink_dirent_gone_msg_end:
stage_dirfd_relative_openat_ok_msg:
    .asciz "mycpu linux userland: stage=dirfd-relative-openat-ok\n"
stage_dirfd_relative_openat_ok_msg_end:
stage_dirfd_relative_fstatat_ok_msg:
    .asciz "mycpu linux userland: stage=dirfd-relative-fstatat-ok\n"
stage_dirfd_relative_fstatat_ok_msg_end:
stage_dirfd_relative_linkat_ok_msg:
    .asciz "mycpu linux userland: stage=dirfd-relative-linkat-ok\n"
stage_dirfd_relative_linkat_ok_msg_end:
stage_dirfd_relative_unlinkat_ok_msg:
    .asciz "mycpu linux userland: stage=dirfd-relative-unlinkat-ok\n"
stage_dirfd_relative_unlinkat_ok_msg_end:
stage_dirfd_relative_reopen_gone_msg:
    .asciz "mycpu linux userland: stage=dirfd-relative-reopen-gone\n"
stage_dirfd_relative_reopen_gone_msg_end:
stage_dirfd_relative_fstatat_gone_msg:
    .asciz "mycpu linux userland: stage=dirfd-relative-fstatat-gone\n"
stage_dirfd_relative_fstatat_gone_msg_end:
stage_dirfd_relative_getdents_gone_msg:
    .asciz "mycpu linux userland: stage=dirfd-relative-getdents-gone\n"
stage_dirfd_relative_getdents_gone_msg_end:
stage_linkat_target_readable_msg:
    .asciz "mycpu linux userland: stage=linkat-target-readable\n"
stage_linkat_target_readable_msg_end:
stage_linkat_shared_inode_ok_msg:
    .asciz "mycpu linux userland: stage=linkat-shared-inode-ok\n"
stage_linkat_shared_inode_ok_msg_end:
stage_unlinkat_origin_hardlink_survives_msg:
    .asciz "mycpu linux userland: stage=unlinkat-origin-hardlink-survives\n"
stage_unlinkat_origin_hardlink_survives_msg_end:
stage_unlinkat_origin_link_count_dropped_msg:
    .asciz "mycpu linux userland: stage=unlinkat-origin-link-count-dropped\n"
stage_unlinkat_origin_link_count_dropped_msg_end:
stage_unlinkat_hardlink_dirent_gone_msg:
    .asciz "mycpu linux userland: stage=unlinkat-hardlink-dirent-gone\n"
stage_unlinkat_hardlink_dirent_gone_msg_end:
stage_unlinkat_open_fd_nlink_zero_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-nlink-zero\n"
stage_unlinkat_open_fd_nlink_zero_msg_end:
stage_unlinkat_open_fd_survives_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-survives\n"
stage_unlinkat_open_fd_survives_msg_end:
stage_unlinkat_open_fd_anon_write_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-anon-write-ok\n"
stage_unlinkat_open_fd_anon_write_ok_msg_end:
stage_unlinkat_open_fd_anon_readback_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-anon-readback-ok\n"
stage_unlinkat_open_fd_anon_readback_ok_msg_end:
stage_unlinkat_open_fd_dup_survives_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-dup-survives\n"
stage_unlinkat_open_fd_dup_survives_msg_end:
stage_unlinkat_open_fd_dup_write_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-dup-write-ok\n"
stage_unlinkat_open_fd_dup_write_ok_msg_end:
stage_unlinkat_open_fd_dup_readback_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-dup-readback-ok\n"
stage_unlinkat_open_fd_dup_readback_ok_msg_end:
stage_unlinkat_open_fd_dup_truncate_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-dup-truncate-ok\n"
stage_unlinkat_open_fd_dup_truncate_ok_msg_end:
stage_unlinkat_open_fd_dup_truncate_roundtrip_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-dup-truncate-roundtrip-ok\n"
stage_unlinkat_open_fd_dup_truncate_roundtrip_ok_msg_end:
stage_unlinkat_open_fd_dup_append_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-dup-append-ok\n"
stage_unlinkat_open_fd_dup_append_ok_msg_end:
stage_unlinkat_open_fd_dup_append_truncate_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-dup-append-truncate-ok\n"
stage_unlinkat_open_fd_dup_append_truncate_ok_msg_end:
stage_unlinkat_open_fd_dup_grow_zero_fill_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-dup-grow-zero-fill-ok\n"
stage_unlinkat_open_fd_dup_grow_zero_fill_ok_msg_end:
stage_unlinkat_open_fd_dup_grow_tail_write_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-dup-grow-tail-write-ok\n"
stage_unlinkat_open_fd_dup_grow_tail_write_ok_msg_end:
stage_unlinkat_open_fd_dup_fork_child_write_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-dup-fork-child-write-ok\n"
stage_unlinkat_open_fd_dup_fork_child_write_ok_msg_end:
stage_unlinkat_open_fd_dup_fork_parent_readback_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-dup-fork-parent-readback-ok\n"
stage_unlinkat_open_fd_dup_fork_parent_readback_ok_msg_end:
stage_unlinkat_open_fd_procfd_reopen_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-procfd-reopen-ok\n"
stage_unlinkat_open_fd_procfd_reopen_ok_msg_end:
stage_unlinkat_open_fd_procfd_readback_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-procfd-readback-ok\n"
stage_unlinkat_open_fd_procfd_readback_ok_msg_end:
stage_unlinkat_open_fd_execve_child_readback_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-execve-child-readback-ok\n"
stage_unlinkat_open_fd_execve_child_readback_ok_msg_end:
stage_unlinkat_open_fd_execve_child_write_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-execve-child-write-ok\n"
stage_unlinkat_open_fd_execve_child_write_ok_msg_end:
stage_unlinkat_open_fd_execve_parent_readback_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-execve-parent-readback-ok\n"
stage_unlinkat_open_fd_execve_parent_readback_ok_msg_end:
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
fourth_stage_symlink_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage symlink smoke failed\n"
fourth_stage_symlink_smoke_fail_msg_end:
fourth_stage_symlink_stat_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage symlink stat smoke failed\n"
fourth_stage_symlink_stat_smoke_fail_msg_end:
fourth_stage_readlinkat_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage readlinkat smoke failed\n"
fourth_stage_readlinkat_smoke_fail_msg_end:
fourth_stage_symlink_cleanup_fail_msg:
    .asciz "mycpu linux userland: fourth-stage symlink cleanup failed\n"
fourth_stage_symlink_cleanup_fail_msg_end:
fourth_stage_symlink_dirent_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage symlink dirent smoke failed\n"
fourth_stage_symlink_dirent_smoke_fail_msg_end:
fourth_stage_dirfd_openat_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage dirfd openat smoke failed\n"
fourth_stage_dirfd_openat_smoke_fail_msg_end:
fourth_stage_dirfd_fstatat_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage dirfd fstatat smoke failed\n"
fourth_stage_dirfd_fstatat_smoke_fail_msg_end:
fourth_stage_dirfd_linkat_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage dirfd linkat smoke failed\n"
fourth_stage_dirfd_linkat_smoke_fail_msg_end:
fourth_stage_dirfd_unlinkat_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage dirfd unlinkat smoke failed\n"
fourth_stage_dirfd_unlinkat_smoke_fail_msg_end:
fourth_stage_dirfd_reopen_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage dirfd reopen smoke failed\n"
fourth_stage_dirfd_reopen_smoke_fail_msg_end:
fourth_stage_dirfd_gone_fstatat_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage dirfd gone-fstatat smoke failed\n"
fourth_stage_dirfd_gone_fstatat_smoke_fail_msg_end:
fourth_stage_dirfd_getdents_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage dirfd getdents smoke failed\n"
fourth_stage_dirfd_getdents_smoke_fail_msg_end:
fourth_stage_hardlink_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage hardlink smoke failed\n"
fourth_stage_hardlink_smoke_fail_msg_end:
fourth_stage_hardlink_stat_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage hardlink stat smoke failed\n"
fourth_stage_hardlink_stat_smoke_fail_msg_end:
fourth_stage_hardlink_survival_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage hardlink survival smoke failed\n"
fourth_stage_hardlink_survival_smoke_fail_msg_end:
fourth_stage_hardlink_nlink_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage hardlink nlink smoke failed\n"
fourth_stage_hardlink_nlink_smoke_fail_msg_end:
fourth_stage_hardlink_cleanup_fail_msg:
    .asciz "mycpu linux userland: fourth-stage hardlink cleanup failed\n"
fourth_stage_hardlink_cleanup_fail_msg_end:
fourth_stage_hardlink_dirent_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage hardlink dirent smoke failed\n"
fourth_stage_hardlink_dirent_smoke_fail_msg_end:
fourth_stage_open_fd_fstat_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd fstat smoke failed\n"
fourth_stage_open_fd_fstat_smoke_fail_msg_end:
fourth_stage_open_fd_survival_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd survival smoke failed\n"
fourth_stage_open_fd_survival_smoke_fail_msg_end:
fourth_stage_open_fd_anon_append_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd anon overwrite failed\n"
fourth_stage_open_fd_anon_append_fail_msg_end:
fourth_stage_open_fd_anon_stat_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd anon stat smoke failed\n"
fourth_stage_open_fd_anon_stat_fail_msg_end:
fourth_stage_open_fd_anon_write_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd anon write smoke failed\n"
fourth_stage_open_fd_anon_write_smoke_fail_msg_end:
fourth_stage_open_fd_anon_readback_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd anon readback smoke failed\n"
fourth_stage_open_fd_anon_readback_smoke_fail_msg_end:
fourth_stage_open_fd_dup_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd dup smoke failed\n"
fourth_stage_open_fd_dup_smoke_fail_msg_end:
fourth_stage_open_fd_dup_write_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd dup write smoke failed\n"
fourth_stage_open_fd_dup_write_smoke_fail_msg_end:
fourth_stage_open_fd_dup_readback_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd dup readback smoke failed\n"
fourth_stage_open_fd_dup_readback_smoke_fail_msg_end:
fourth_stage_open_fd_dup_truncate_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd dup truncate smoke failed\n"
fourth_stage_open_fd_dup_truncate_smoke_fail_msg_end:
fourth_stage_open_fd_dup_truncate_roundtrip_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd dup truncate roundtrip failed\n"
fourth_stage_open_fd_dup_truncate_roundtrip_fail_msg_end:
fourth_stage_open_fd_dup_append_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd dup append failed\n"
fourth_stage_open_fd_dup_append_fail_msg_end:
fourth_stage_open_fd_dup_append_truncate_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd dup append truncate failed\n"
fourth_stage_open_fd_dup_append_truncate_fail_msg_end:
fourth_stage_open_fd_dup_grow_zero_fill_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd dup grow zero-fill failed\n"
fourth_stage_open_fd_dup_grow_zero_fill_fail_msg_end:
fourth_stage_open_fd_dup_grow_tail_write_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd dup grow tail write failed\n"
fourth_stage_open_fd_dup_grow_tail_write_fail_msg_end:
fourth_stage_open_fd_dup_fork_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd dup fork smoke failed\n"
fourth_stage_open_fd_dup_fork_smoke_fail_msg_end:
fourth_stage_open_fd_procfd_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd procfd smoke failed\n"
fourth_stage_open_fd_procfd_smoke_fail_msg_end:
fourth_stage_open_fd_execve_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd execve smoke failed\n"
fourth_stage_open_fd_execve_smoke_fail_msg_end:
fourth_stage_console_fail_msg:
    .asciz "mycpu linux userland: fourth-stage console open failed\n"
fourth_stage_console_fail_msg_end:
fourth_stage_write_fail_msg:
    .asciz "mycpu linux userland: fourth-stage write failed\n"
fourth_stage_write_fail_msg_end:
fourth_stage_write_short_msg:
    .asciz "mycpu linux userland: fourth-stage write short\n"
fourth_stage_write_short_msg_end:

.set post_init_data_path_len, post_init_data_path_end - post_init_data_path
.set post_init_data_path_len, post_init_data_path_len - 1
.set post_init_data_expected_len, post_init_data_expected_end - post_init_data_expected
.set post_init_data_expected_len, post_init_data_expected_len - 1
.set post_init_data_anonymous_expected_len, post_init_data_anonymous_expected_end - post_init_data_anonymous_expected
.set post_init_data_anonymous_expected_len, post_init_data_anonymous_expected_len - 1
.set post_init_data_dup_expected_len, post_init_data_dup_expected_end - post_init_data_dup_expected
.set post_init_data_dup_expected_len, post_init_data_dup_expected_len - 1
.set post_init_data_dup_truncate_expected_len, post_init_data_dup_truncate_expected_end - post_init_data_dup_truncate_expected
.set post_init_data_dup_truncate_expected_len, post_init_data_dup_truncate_expected_len - 1
.set post_init_data_dup_append_tail_len, post_init_data_dup_append_tail_end - post_init_data_dup_append_tail
.set post_init_data_dup_append_tail_len, post_init_data_dup_append_tail_len - 1
.set post_init_data_dup_append_expected_len, post_init_data_dup_append_expected_end - post_init_data_dup_append_expected
.set post_init_data_dup_append_expected_len, post_init_data_dup_append_expected_len - 1
.set post_init_data_dup_fork_expected_len, post_init_data_dup_fork_expected_end - post_init_data_dup_fork_expected
.set post_init_data_dup_fork_expected_len, post_init_data_dup_fork_expected_len - 1
.set post_init_data_execve_expected_len, post_init_data_execve_expected_end - post_init_data_execve_expected
.set post_init_data_execve_expected_len, post_init_data_execve_expected_len - 1
.set fourth_stage_stat_buf_len, 128
.set fourth_stage_stat_st_ino_offset, 8
.set fourth_stage_stat_st_mode_offset, 16
.set fourth_stage_stat_st_nlink_offset, 20
.set fourth_stage_stat_st_size_offset, 48
.set fourth_stage_stat_mode_mask, 61440
.set fourth_stage_stat_mode_regular, 32768
.set fourth_stage_stat_mode_symlink, 40960
.set stage_unlinkat_reused_dirent_gone_msg_len, stage_unlinkat_reused_dirent_gone_msg_end - stage_unlinkat_reused_dirent_gone_msg
.set stage_unlinkat_reused_dirent_gone_msg_len, stage_unlinkat_reused_dirent_gone_msg_len - 1
.set stage_fourth_stage_entered_msg_len, stage_fourth_stage_entered_msg_end - stage_fourth_stage_entered_msg
.set stage_fourth_stage_entered_msg_len, stage_fourth_stage_entered_msg_len - 1
.set stage_fourth_stage_console_opened_msg_len, stage_fourth_stage_console_opened_msg_end - stage_fourth_stage_console_opened_msg
.set stage_fourth_stage_console_opened_msg_len, stage_fourth_stage_console_opened_msg_len - 1
.set stage_fourth_stage_console_fallback_msg_len, stage_fourth_stage_console_fallback_msg_end - stage_fourth_stage_console_fallback_msg
.set stage_fourth_stage_console_fallback_msg_len, stage_fourth_stage_console_fallback_msg_len - 1
.set stage_fourth_stage_root_chdir_ok_msg_len, stage_fourth_stage_root_chdir_ok_msg_end - stage_fourth_stage_root_chdir_ok_msg
.set stage_fourth_stage_root_chdir_ok_msg_len, stage_fourth_stage_root_chdir_ok_msg_len - 1
.set stage_fourth_stage_unlinkat_reused_dir_ok_msg_len, stage_fourth_stage_unlinkat_reused_dir_ok_msg_end - stage_fourth_stage_unlinkat_reused_dir_ok_msg
.set stage_fourth_stage_unlinkat_reused_dir_ok_msg_len, stage_fourth_stage_unlinkat_reused_dir_ok_msg_len - 1
.set stage_symlinkat_target_readable_msg_len, stage_symlinkat_target_readable_msg_end - stage_symlinkat_target_readable_msg
.set stage_symlinkat_target_readable_msg_len, stage_symlinkat_target_readable_msg_len - 1
.set stage_fstatat_symlink_nofollow_ok_msg_len, stage_fstatat_symlink_nofollow_ok_msg_end - stage_fstatat_symlink_nofollow_ok_msg
.set stage_fstatat_symlink_nofollow_ok_msg_len, stage_fstatat_symlink_nofollow_ok_msg_len - 1
.set stage_readlinkat_target_ok_msg_len, stage_readlinkat_target_ok_msg_end - stage_readlinkat_target_ok_msg
.set stage_readlinkat_target_ok_msg_len, stage_readlinkat_target_ok_msg_len - 1
.set stage_unlinkat_symlink_dirent_gone_msg_len, stage_unlinkat_symlink_dirent_gone_msg_end - stage_unlinkat_symlink_dirent_gone_msg
.set stage_unlinkat_symlink_dirent_gone_msg_len, stage_unlinkat_symlink_dirent_gone_msg_len - 1
.set stage_dirfd_relative_openat_ok_msg_len, stage_dirfd_relative_openat_ok_msg_end - stage_dirfd_relative_openat_ok_msg
.set stage_dirfd_relative_openat_ok_msg_len, stage_dirfd_relative_openat_ok_msg_len - 1
.set stage_dirfd_relative_fstatat_ok_msg_len, stage_dirfd_relative_fstatat_ok_msg_end - stage_dirfd_relative_fstatat_ok_msg
.set stage_dirfd_relative_fstatat_ok_msg_len, stage_dirfd_relative_fstatat_ok_msg_len - 1
.set stage_dirfd_relative_linkat_ok_msg_len, stage_dirfd_relative_linkat_ok_msg_end - stage_dirfd_relative_linkat_ok_msg
.set stage_dirfd_relative_linkat_ok_msg_len, stage_dirfd_relative_linkat_ok_msg_len - 1
.set stage_dirfd_relative_unlinkat_ok_msg_len, stage_dirfd_relative_unlinkat_ok_msg_end - stage_dirfd_relative_unlinkat_ok_msg
.set stage_dirfd_relative_unlinkat_ok_msg_len, stage_dirfd_relative_unlinkat_ok_msg_len - 1
.set stage_dirfd_relative_reopen_gone_msg_len, stage_dirfd_relative_reopen_gone_msg_end - stage_dirfd_relative_reopen_gone_msg
.set stage_dirfd_relative_reopen_gone_msg_len, stage_dirfd_relative_reopen_gone_msg_len - 1
.set stage_dirfd_relative_fstatat_gone_msg_len, stage_dirfd_relative_fstatat_gone_msg_end - stage_dirfd_relative_fstatat_gone_msg
.set stage_dirfd_relative_fstatat_gone_msg_len, stage_dirfd_relative_fstatat_gone_msg_len - 1
.set stage_dirfd_relative_getdents_gone_msg_len, stage_dirfd_relative_getdents_gone_msg_end - stage_dirfd_relative_getdents_gone_msg
.set stage_dirfd_relative_getdents_gone_msg_len, stage_dirfd_relative_getdents_gone_msg_len - 1
.set stage_linkat_target_readable_msg_len, stage_linkat_target_readable_msg_end - stage_linkat_target_readable_msg
.set stage_linkat_target_readable_msg_len, stage_linkat_target_readable_msg_len - 1
.set stage_linkat_shared_inode_ok_msg_len, stage_linkat_shared_inode_ok_msg_end - stage_linkat_shared_inode_ok_msg
.set stage_linkat_shared_inode_ok_msg_len, stage_linkat_shared_inode_ok_msg_len - 1
.set stage_unlinkat_origin_hardlink_survives_msg_len, stage_unlinkat_origin_hardlink_survives_msg_end - stage_unlinkat_origin_hardlink_survives_msg
.set stage_unlinkat_origin_hardlink_survives_msg_len, stage_unlinkat_origin_hardlink_survives_msg_len - 1
.set stage_unlinkat_origin_link_count_dropped_msg_len, stage_unlinkat_origin_link_count_dropped_msg_end - stage_unlinkat_origin_link_count_dropped_msg
.set stage_unlinkat_origin_link_count_dropped_msg_len, stage_unlinkat_origin_link_count_dropped_msg_len - 1
.set stage_unlinkat_hardlink_dirent_gone_msg_len, stage_unlinkat_hardlink_dirent_gone_msg_end - stage_unlinkat_hardlink_dirent_gone_msg
.set stage_unlinkat_hardlink_dirent_gone_msg_len, stage_unlinkat_hardlink_dirent_gone_msg_len - 1
.set stage_unlinkat_open_fd_nlink_zero_msg_len, stage_unlinkat_open_fd_nlink_zero_msg_end - stage_unlinkat_open_fd_nlink_zero_msg
.set stage_unlinkat_open_fd_nlink_zero_msg_len, stage_unlinkat_open_fd_nlink_zero_msg_len - 1
.set stage_unlinkat_open_fd_survives_msg_len, stage_unlinkat_open_fd_survives_msg_end - stage_unlinkat_open_fd_survives_msg
.set stage_unlinkat_open_fd_survives_msg_len, stage_unlinkat_open_fd_survives_msg_len - 1
.set stage_unlinkat_open_fd_anon_write_ok_msg_len, stage_unlinkat_open_fd_anon_write_ok_msg_end - stage_unlinkat_open_fd_anon_write_ok_msg
.set stage_unlinkat_open_fd_anon_write_ok_msg_len, stage_unlinkat_open_fd_anon_write_ok_msg_len - 1
.set stage_unlinkat_open_fd_anon_readback_ok_msg_len, stage_unlinkat_open_fd_anon_readback_ok_msg_end - stage_unlinkat_open_fd_anon_readback_ok_msg
.set stage_unlinkat_open_fd_anon_readback_ok_msg_len, stage_unlinkat_open_fd_anon_readback_ok_msg_len - 1
.set stage_unlinkat_open_fd_dup_survives_msg_len, stage_unlinkat_open_fd_dup_survives_msg_end - stage_unlinkat_open_fd_dup_survives_msg
.set stage_unlinkat_open_fd_dup_survives_msg_len, stage_unlinkat_open_fd_dup_survives_msg_len - 1
.set stage_unlinkat_open_fd_dup_write_ok_msg_len, stage_unlinkat_open_fd_dup_write_ok_msg_end - stage_unlinkat_open_fd_dup_write_ok_msg
.set stage_unlinkat_open_fd_dup_write_ok_msg_len, stage_unlinkat_open_fd_dup_write_ok_msg_len - 1
.set stage_unlinkat_open_fd_dup_readback_ok_msg_len, stage_unlinkat_open_fd_dup_readback_ok_msg_end - stage_unlinkat_open_fd_dup_readback_ok_msg
.set stage_unlinkat_open_fd_dup_readback_ok_msg_len, stage_unlinkat_open_fd_dup_readback_ok_msg_len - 1
.set stage_unlinkat_open_fd_dup_truncate_ok_msg_len, stage_unlinkat_open_fd_dup_truncate_ok_msg_end - stage_unlinkat_open_fd_dup_truncate_ok_msg
.set stage_unlinkat_open_fd_dup_truncate_ok_msg_len, stage_unlinkat_open_fd_dup_truncate_ok_msg_len - 1
.set stage_unlinkat_open_fd_dup_truncate_roundtrip_ok_msg_len, stage_unlinkat_open_fd_dup_truncate_roundtrip_ok_msg_end - stage_unlinkat_open_fd_dup_truncate_roundtrip_ok_msg
.set stage_unlinkat_open_fd_dup_truncate_roundtrip_ok_msg_len, stage_unlinkat_open_fd_dup_truncate_roundtrip_ok_msg_len - 1
.set stage_unlinkat_open_fd_dup_append_ok_msg_len, stage_unlinkat_open_fd_dup_append_ok_msg_end - stage_unlinkat_open_fd_dup_append_ok_msg
.set stage_unlinkat_open_fd_dup_append_ok_msg_len, stage_unlinkat_open_fd_dup_append_ok_msg_len - 1
.set stage_unlinkat_open_fd_dup_append_truncate_ok_msg_len, stage_unlinkat_open_fd_dup_append_truncate_ok_msg_end - stage_unlinkat_open_fd_dup_append_truncate_ok_msg
.set stage_unlinkat_open_fd_dup_append_truncate_ok_msg_len, stage_unlinkat_open_fd_dup_append_truncate_ok_msg_len - 1
.set stage_unlinkat_open_fd_dup_grow_zero_fill_ok_msg_len, stage_unlinkat_open_fd_dup_grow_zero_fill_ok_msg_end - stage_unlinkat_open_fd_dup_grow_zero_fill_ok_msg
.set stage_unlinkat_open_fd_dup_grow_zero_fill_ok_msg_len, stage_unlinkat_open_fd_dup_grow_zero_fill_ok_msg_len - 1
.set stage_unlinkat_open_fd_dup_grow_tail_write_ok_msg_len, stage_unlinkat_open_fd_dup_grow_tail_write_ok_msg_end - stage_unlinkat_open_fd_dup_grow_tail_write_ok_msg
.set stage_unlinkat_open_fd_dup_grow_tail_write_ok_msg_len, stage_unlinkat_open_fd_dup_grow_tail_write_ok_msg_len - 1
.set stage_unlinkat_open_fd_dup_fork_child_write_ok_msg_len, stage_unlinkat_open_fd_dup_fork_child_write_ok_msg_end - stage_unlinkat_open_fd_dup_fork_child_write_ok_msg
.set stage_unlinkat_open_fd_dup_fork_child_write_ok_msg_len, stage_unlinkat_open_fd_dup_fork_child_write_ok_msg_len - 1
.set stage_unlinkat_open_fd_dup_fork_parent_readback_ok_msg_len, stage_unlinkat_open_fd_dup_fork_parent_readback_ok_msg_end - stage_unlinkat_open_fd_dup_fork_parent_readback_ok_msg
.set stage_unlinkat_open_fd_dup_fork_parent_readback_ok_msg_len, stage_unlinkat_open_fd_dup_fork_parent_readback_ok_msg_len - 1
.set stage_unlinkat_open_fd_procfd_reopen_ok_msg_len, stage_unlinkat_open_fd_procfd_reopen_ok_msg_end - stage_unlinkat_open_fd_procfd_reopen_ok_msg
.set stage_unlinkat_open_fd_procfd_reopen_ok_msg_len, stage_unlinkat_open_fd_procfd_reopen_ok_msg_len - 1
.set stage_unlinkat_open_fd_procfd_readback_ok_msg_len, stage_unlinkat_open_fd_procfd_readback_ok_msg_end - stage_unlinkat_open_fd_procfd_readback_ok_msg
.set stage_unlinkat_open_fd_procfd_readback_ok_msg_len, stage_unlinkat_open_fd_procfd_readback_ok_msg_len - 1
.set stage_unlinkat_open_fd_execve_child_readback_ok_msg_len, stage_unlinkat_open_fd_execve_child_readback_ok_msg_end - stage_unlinkat_open_fd_execve_child_readback_ok_msg
.set stage_unlinkat_open_fd_execve_child_readback_ok_msg_len, stage_unlinkat_open_fd_execve_child_readback_ok_msg_len - 1
.set stage_unlinkat_open_fd_execve_child_write_ok_msg_len, stage_unlinkat_open_fd_execve_child_write_ok_msg_end - stage_unlinkat_open_fd_execve_child_write_ok_msg
.set stage_unlinkat_open_fd_execve_child_write_ok_msg_len, stage_unlinkat_open_fd_execve_child_write_ok_msg_len - 1
.set stage_unlinkat_open_fd_execve_parent_readback_ok_msg_len, stage_unlinkat_open_fd_execve_parent_readback_ok_msg_end - stage_unlinkat_open_fd_execve_parent_readback_ok_msg
.set stage_unlinkat_open_fd_execve_parent_readback_ok_msg_len, stage_unlinkat_open_fd_execve_parent_readback_ok_msg_len - 1
.set stage_fourth_stage_reached_msg_len, stage_fourth_stage_reached_msg_end - stage_fourth_stage_reached_msg
.set stage_fourth_stage_reached_msg_len, stage_fourth_stage_reached_msg_len - 1
.set post_init_reached_msg_len, post_init_reached_msg_end - post_init_reached_msg
.set post_init_reached_msg_len, post_init_reached_msg_len - 1
.set fourth_stage_reused_dir_cleanup_fail_msg_len, fourth_stage_reused_dir_cleanup_fail_msg_end - fourth_stage_reused_dir_cleanup_fail_msg
.set fourth_stage_reused_dir_cleanup_fail_msg_len, fourth_stage_reused_dir_cleanup_fail_msg_len - 1
.set fourth_stage_reused_dir_dirent_smoke_fail_msg_len, fourth_stage_reused_dir_dirent_smoke_fail_msg_end - fourth_stage_reused_dir_dirent_smoke_fail_msg
.set fourth_stage_reused_dir_dirent_smoke_fail_msg_len, fourth_stage_reused_dir_dirent_smoke_fail_msg_len - 1
.set fourth_stage_symlink_smoke_fail_msg_len, fourth_stage_symlink_smoke_fail_msg_end - fourth_stage_symlink_smoke_fail_msg
.set fourth_stage_symlink_smoke_fail_msg_len, fourth_stage_symlink_smoke_fail_msg_len - 1
.set fourth_stage_symlink_stat_smoke_fail_msg_len, fourth_stage_symlink_stat_smoke_fail_msg_end - fourth_stage_symlink_stat_smoke_fail_msg
.set fourth_stage_symlink_stat_smoke_fail_msg_len, fourth_stage_symlink_stat_smoke_fail_msg_len - 1
.set fourth_stage_readlinkat_smoke_fail_msg_len, fourth_stage_readlinkat_smoke_fail_msg_end - fourth_stage_readlinkat_smoke_fail_msg
.set fourth_stage_readlinkat_smoke_fail_msg_len, fourth_stage_readlinkat_smoke_fail_msg_len - 1
.set fourth_stage_symlink_cleanup_fail_msg_len, fourth_stage_symlink_cleanup_fail_msg_end - fourth_stage_symlink_cleanup_fail_msg
.set fourth_stage_symlink_cleanup_fail_msg_len, fourth_stage_symlink_cleanup_fail_msg_len - 1
.set fourth_stage_symlink_dirent_smoke_fail_msg_len, fourth_stage_symlink_dirent_smoke_fail_msg_end - fourth_stage_symlink_dirent_smoke_fail_msg
.set fourth_stage_symlink_dirent_smoke_fail_msg_len, fourth_stage_symlink_dirent_smoke_fail_msg_len - 1
.set fourth_stage_dirfd_openat_smoke_fail_msg_len, fourth_stage_dirfd_openat_smoke_fail_msg_end - fourth_stage_dirfd_openat_smoke_fail_msg
.set fourth_stage_dirfd_openat_smoke_fail_msg_len, fourth_stage_dirfd_openat_smoke_fail_msg_len - 1
.set fourth_stage_dirfd_fstatat_smoke_fail_msg_len, fourth_stage_dirfd_fstatat_smoke_fail_msg_end - fourth_stage_dirfd_fstatat_smoke_fail_msg
.set fourth_stage_dirfd_fstatat_smoke_fail_msg_len, fourth_stage_dirfd_fstatat_smoke_fail_msg_len - 1
.set fourth_stage_dirfd_linkat_smoke_fail_msg_len, fourth_stage_dirfd_linkat_smoke_fail_msg_end - fourth_stage_dirfd_linkat_smoke_fail_msg
.set fourth_stage_dirfd_linkat_smoke_fail_msg_len, fourth_stage_dirfd_linkat_smoke_fail_msg_len - 1
.set fourth_stage_dirfd_unlinkat_smoke_fail_msg_len, fourth_stage_dirfd_unlinkat_smoke_fail_msg_end - fourth_stage_dirfd_unlinkat_smoke_fail_msg
.set fourth_stage_dirfd_unlinkat_smoke_fail_msg_len, fourth_stage_dirfd_unlinkat_smoke_fail_msg_len - 1
.set fourth_stage_dirfd_reopen_smoke_fail_msg_len, fourth_stage_dirfd_reopen_smoke_fail_msg_end - fourth_stage_dirfd_reopen_smoke_fail_msg
.set fourth_stage_dirfd_reopen_smoke_fail_msg_len, fourth_stage_dirfd_reopen_smoke_fail_msg_len - 1
.set fourth_stage_dirfd_gone_fstatat_smoke_fail_msg_len, fourth_stage_dirfd_gone_fstatat_smoke_fail_msg_end - fourth_stage_dirfd_gone_fstatat_smoke_fail_msg
.set fourth_stage_dirfd_gone_fstatat_smoke_fail_msg_len, fourth_stage_dirfd_gone_fstatat_smoke_fail_msg_len - 1
.set fourth_stage_dirfd_getdents_smoke_fail_msg_len, fourth_stage_dirfd_getdents_smoke_fail_msg_end - fourth_stage_dirfd_getdents_smoke_fail_msg
.set fourth_stage_dirfd_getdents_smoke_fail_msg_len, fourth_stage_dirfd_getdents_smoke_fail_msg_len - 1
.set fourth_stage_hardlink_smoke_fail_msg_len, fourth_stage_hardlink_smoke_fail_msg_end - fourth_stage_hardlink_smoke_fail_msg
.set fourth_stage_hardlink_smoke_fail_msg_len, fourth_stage_hardlink_smoke_fail_msg_len - 1
.set fourth_stage_hardlink_stat_smoke_fail_msg_len, fourth_stage_hardlink_stat_smoke_fail_msg_end - fourth_stage_hardlink_stat_smoke_fail_msg
.set fourth_stage_hardlink_stat_smoke_fail_msg_len, fourth_stage_hardlink_stat_smoke_fail_msg_len - 1
.set fourth_stage_hardlink_survival_smoke_fail_msg_len, fourth_stage_hardlink_survival_smoke_fail_msg_end - fourth_stage_hardlink_survival_smoke_fail_msg
.set fourth_stage_hardlink_survival_smoke_fail_msg_len, fourth_stage_hardlink_survival_smoke_fail_msg_len - 1
.set fourth_stage_hardlink_nlink_smoke_fail_msg_len, fourth_stage_hardlink_nlink_smoke_fail_msg_end - fourth_stage_hardlink_nlink_smoke_fail_msg
.set fourth_stage_hardlink_nlink_smoke_fail_msg_len, fourth_stage_hardlink_nlink_smoke_fail_msg_len - 1
.set fourth_stage_hardlink_cleanup_fail_msg_len, fourth_stage_hardlink_cleanup_fail_msg_end - fourth_stage_hardlink_cleanup_fail_msg
.set fourth_stage_hardlink_cleanup_fail_msg_len, fourth_stage_hardlink_cleanup_fail_msg_len - 1
.set fourth_stage_hardlink_dirent_smoke_fail_msg_len, fourth_stage_hardlink_dirent_smoke_fail_msg_end - fourth_stage_hardlink_dirent_smoke_fail_msg
.set fourth_stage_hardlink_dirent_smoke_fail_msg_len, fourth_stage_hardlink_dirent_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_fstat_smoke_fail_msg_len, fourth_stage_open_fd_fstat_smoke_fail_msg_end - fourth_stage_open_fd_fstat_smoke_fail_msg
.set fourth_stage_open_fd_fstat_smoke_fail_msg_len, fourth_stage_open_fd_fstat_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_survival_smoke_fail_msg_len, fourth_stage_open_fd_survival_smoke_fail_msg_end - fourth_stage_open_fd_survival_smoke_fail_msg
.set fourth_stage_open_fd_survival_smoke_fail_msg_len, fourth_stage_open_fd_survival_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_anon_append_fail_msg_len, fourth_stage_open_fd_anon_append_fail_msg_end - fourth_stage_open_fd_anon_append_fail_msg
.set fourth_stage_open_fd_anon_append_fail_msg_len, fourth_stage_open_fd_anon_append_fail_msg_len - 1
.set fourth_stage_open_fd_anon_stat_fail_msg_len, fourth_stage_open_fd_anon_stat_fail_msg_end - fourth_stage_open_fd_anon_stat_fail_msg
.set fourth_stage_open_fd_anon_stat_fail_msg_len, fourth_stage_open_fd_anon_stat_fail_msg_len - 1
.set fourth_stage_open_fd_anon_write_smoke_fail_msg_len, fourth_stage_open_fd_anon_write_smoke_fail_msg_end - fourth_stage_open_fd_anon_write_smoke_fail_msg
.set fourth_stage_open_fd_anon_write_smoke_fail_msg_len, fourth_stage_open_fd_anon_write_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_anon_readback_smoke_fail_msg_len, fourth_stage_open_fd_anon_readback_smoke_fail_msg_end - fourth_stage_open_fd_anon_readback_smoke_fail_msg
.set fourth_stage_open_fd_anon_readback_smoke_fail_msg_len, fourth_stage_open_fd_anon_readback_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_dup_smoke_fail_msg_len, fourth_stage_open_fd_dup_smoke_fail_msg_end - fourth_stage_open_fd_dup_smoke_fail_msg
.set fourth_stage_open_fd_dup_smoke_fail_msg_len, fourth_stage_open_fd_dup_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_dup_write_smoke_fail_msg_len, fourth_stage_open_fd_dup_write_smoke_fail_msg_end - fourth_stage_open_fd_dup_write_smoke_fail_msg
.set fourth_stage_open_fd_dup_write_smoke_fail_msg_len, fourth_stage_open_fd_dup_write_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_dup_readback_smoke_fail_msg_len, fourth_stage_open_fd_dup_readback_smoke_fail_msg_end - fourth_stage_open_fd_dup_readback_smoke_fail_msg
.set fourth_stage_open_fd_dup_readback_smoke_fail_msg_len, fourth_stage_open_fd_dup_readback_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_dup_truncate_smoke_fail_msg_len, fourth_stage_open_fd_dup_truncate_smoke_fail_msg_end - fourth_stage_open_fd_dup_truncate_smoke_fail_msg
.set fourth_stage_open_fd_dup_truncate_smoke_fail_msg_len, fourth_stage_open_fd_dup_truncate_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_dup_truncate_roundtrip_fail_msg_len, fourth_stage_open_fd_dup_truncate_roundtrip_fail_msg_end - fourth_stage_open_fd_dup_truncate_roundtrip_fail_msg
.set fourth_stage_open_fd_dup_truncate_roundtrip_fail_msg_len, fourth_stage_open_fd_dup_truncate_roundtrip_fail_msg_len - 1
.set fourth_stage_open_fd_dup_append_fail_msg_len, fourth_stage_open_fd_dup_append_fail_msg_end - fourth_stage_open_fd_dup_append_fail_msg
.set fourth_stage_open_fd_dup_append_fail_msg_len, fourth_stage_open_fd_dup_append_fail_msg_len - 1
.set fourth_stage_open_fd_dup_append_truncate_fail_msg_len, fourth_stage_open_fd_dup_append_truncate_fail_msg_end - fourth_stage_open_fd_dup_append_truncate_fail_msg
.set fourth_stage_open_fd_dup_append_truncate_fail_msg_len, fourth_stage_open_fd_dup_append_truncate_fail_msg_len - 1
.set fourth_stage_open_fd_dup_grow_zero_fill_fail_msg_len, fourth_stage_open_fd_dup_grow_zero_fill_fail_msg_end - fourth_stage_open_fd_dup_grow_zero_fill_fail_msg
.set fourth_stage_open_fd_dup_grow_zero_fill_fail_msg_len, fourth_stage_open_fd_dup_grow_zero_fill_fail_msg_len - 1
.set fourth_stage_open_fd_dup_grow_tail_write_fail_msg_len, fourth_stage_open_fd_dup_grow_tail_write_fail_msg_end - fourth_stage_open_fd_dup_grow_tail_write_fail_msg
.set fourth_stage_open_fd_dup_grow_tail_write_fail_msg_len, fourth_stage_open_fd_dup_grow_tail_write_fail_msg_len - 1
.set fourth_stage_open_fd_dup_fork_smoke_fail_msg_len, fourth_stage_open_fd_dup_fork_smoke_fail_msg_end - fourth_stage_open_fd_dup_fork_smoke_fail_msg
.set fourth_stage_open_fd_dup_fork_smoke_fail_msg_len, fourth_stage_open_fd_dup_fork_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_procfd_smoke_fail_msg_len, fourth_stage_open_fd_procfd_smoke_fail_msg_end - fourth_stage_open_fd_procfd_smoke_fail_msg
.set fourth_stage_open_fd_procfd_smoke_fail_msg_len, fourth_stage_open_fd_procfd_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_execve_smoke_fail_msg_len, fourth_stage_open_fd_execve_smoke_fail_msg_end - fourth_stage_open_fd_execve_smoke_fail_msg
.set fourth_stage_open_fd_execve_smoke_fail_msg_len, fourth_stage_open_fd_execve_smoke_fail_msg_len - 1
.set fourth_stage_console_fail_msg_len, fourth_stage_console_fail_msg_end - fourth_stage_console_fail_msg
.set fourth_stage_console_fail_msg_len, fourth_stage_console_fail_msg_len - 1
.set fourth_stage_write_fail_msg_len, fourth_stage_write_fail_msg_end - fourth_stage_write_fail_msg
.set fourth_stage_write_fail_msg_len, fourth_stage_write_fail_msg_len - 1
.set fourth_stage_write_short_msg_len, fourth_stage_write_short_msg_end - fourth_stage_write_short_msg
.set fourth_stage_write_short_msg_len, fourth_stage_write_short_msg_len - 1
.set clone3_args_size, 88
.set fourth_stage_dirent_buf_len, 1024
.set fourth_stage_post_init_data_buf_len, post_init_data_dup_append_expected_len

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
sleep_spec:
    .dword 1
    .dword 0

    .section .data
execve_open_fd_argv:
    .dword execve_open_fd_self_path
    .dword execve_open_fd_mode_arg
    .dword execve_open_fd_fd_arg
    .dword 0
execve_open_fd_envp:
    .dword 0
proc_self_fd_path:
    .ascii "/proc/self/fd/"
proc_self_fd_digit:
    .byte '0'
proc_self_fd_path_end:
    .byte 0
execve_open_fd_fd_arg:
    .byte '0'
    .byte 0

    .section .bss
fourth_stage_dirent_buf:
    .space fourth_stage_dirent_buf_len
fourth_stage_post_init_data_buf:
    .space fourth_stage_post_init_data_buf_len
fourth_stage_readlink_buf:
    .space post_init_data_path_len
fourth_stage_stat_buf:
    .space fourth_stage_stat_buf_len
fourth_stage_stat_buf_alt:
    .space fourth_stage_stat_buf_len
    .balign 8
fourth_stage_wait_status:
    .word 0

    .section .text
    .globl _start
_start:
    ld t0, 0(sp)
    li t1, 3
    blt t0, t1, fourth_stage_default_start
    addi t1, sp, 8
    ld t2, 8(t1)
    beqz t2, fourth_stage_default_start
    mv a0, t2
    la a1, execve_open_fd_mode_arg
    jal ra, compare_cstr
    bnez a0, fourth_stage_default_start
    addi t1, sp, 8
    ld t3, 16(t1)
    beqz t3, open_fd_execve_smoke_failed_entry
    lbu t0, 0(t3)
    addi t0, t0, -'0'
    bltz t0, open_fd_execve_smoke_failed_entry
    li t1, 9
    blt t1, t0, open_fd_execve_smoke_failed_entry
    lbu t2, 1(t3)
    bnez t2, open_fd_execve_smoke_failed_entry
    mv s4, t0
    j open_fd_execve_child_entry

fourth_stage_default_start:
    li s0, INHERITED_CONSOLE_FD

    mv a0, s0
    la a1, stage_fourth_stage_entered_msg
    li a2, stage_fourth_stage_entered_msg_len
    jal ra, write_exact

    li a0, AT_FDCWD
    la a1, console_path
    li a2, O_RDWR
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, console_open_fallback
    mv s0, a0
    li s1, 0

    mv a0, s0
    la a1, stage_fourth_stage_console_opened_msg
    li a2, stage_fourth_stage_console_opened_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1
    j fourth_stage_start

console_open_fallback:
    li s1, 0

    mv a0, s0
    la a1, stage_fourth_stage_console_fallback_msg
    li a2, stage_fourth_stage_console_fallback_msg_len
    jal ra, write_exact
    bltz a0, console_open_failed
    or s1, s1, a1

fourth_stage_start:

    la a0, root_dir_path
    li a7, SYS_CHDIR
    ecall
    bltz a0, reused_dir_cleanup_failed

    mv a0, s0
    la a1, stage_fourth_stage_root_chdir_ok_msg
    li a2, stage_fourth_stage_root_chdir_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, AT_FDCWD
    la a1, reused_dir_path
    li a2, AT_REMOVEDIR
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, reused_dir_cleanup_failed

    mv a0, s0
    la a1, stage_fourth_stage_unlinkat_reused_dir_ok_msg
    li a2, stage_fourth_stage_unlinkat_reused_dir_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    la a0, reused_dir_name
    jal ra, dir_contains_name
    bnez a0, reused_dir_dirent_smoke_failed

    mv a0, s0
    la a1, stage_unlinkat_reused_dirent_gone_msg
    li a2, stage_unlinkat_reused_dirent_gone_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    la a0, post_init_data_path
    li a1, AT_FDCWD
    la a2, post_init_data_link_path
    li a7, SYS_SYMLINKAT
    ecall
    bltz a0, symlink_smoke_failed

    li a0, AT_FDCWD
    la a1, post_init_data_link_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, symlink_smoke_failed
    mv t0, a0

    mv a0, t0
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_expected_len
    bne a0, t1, symlink_smoke_close_error

    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    bltz a0, symlink_smoke_failed

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_expected
    li a2, post_init_data_expected_len
    jal ra, compare_bytes
    bltz a0, symlink_smoke_failed

    mv a0, s0
    la a1, stage_symlinkat_target_readable_msg
    li a2, stage_symlinkat_target_readable_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, AT_FDCWD
    la a1, post_init_data_link_path
    la a2, fourth_stage_stat_buf
    li a3, AT_SYMLINK_NOFOLLOW
    li a7, SYS_NEWFSTATAT
    ecall
    bltz a0, symlink_stat_smoke_failed

    la t0, fourth_stage_stat_buf
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_symlink
    bne t1, t2, symlink_stat_smoke_failed
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_path_len
    bne t1, t2, symlink_stat_smoke_failed

    mv a0, s0
    la a1, stage_fstatat_symlink_nofollow_ok_msg
    li a2, stage_fstatat_symlink_nofollow_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, AT_FDCWD
    la a1, post_init_data_link_path
    la a2, fourth_stage_readlink_buf
    li a3, post_init_data_path_len
    li a7, SYS_READLINKAT
    ecall
    li t1, post_init_data_path_len
    bne a0, t1, readlinkat_smoke_failed

    la a0, fourth_stage_readlink_buf
    la a1, post_init_data_path
    li a2, post_init_data_path_len
    jal ra, compare_bytes
    bltz a0, readlinkat_smoke_failed

    mv a0, s0
    la a1, stage_readlinkat_target_ok_msg
    li a2, stage_readlinkat_target_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, AT_FDCWD
    la a1, post_init_data_link_path
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, symlink_cleanup_failed

    la a0, post_init_data_link_name
    jal ra, dir_contains_name
    bnez a0, symlink_dirent_smoke_failed

    mv a0, s0
    la a1, stage_unlinkat_symlink_dirent_gone_msg
    li a2, stage_unlinkat_symlink_dirent_gone_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, AT_FDCWD
    la a1, root_dir_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, dirfd_openat_smoke_failed
    mv s2, a0

    mv a0, s2
    la a1, post_init_data_name
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, dirfd_openat_root_close_error
    mv s3, a0

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_expected_len
    bne a0, t1, dirfd_openat_data_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, dirfd_openat_root_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_expected
    li a2, post_init_data_expected_len
    jal ra, compare_bytes
    bltz a0, dirfd_openat_smoke_failed

    mv a0, s0
    la a1, stage_dirfd_relative_openat_ok_msg
    li a2, stage_dirfd_relative_openat_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s2
    la a1, post_init_data_name
    la a2, fourth_stage_stat_buf
    li a3, 0
    li a7, SYS_NEWFSTATAT
    ecall
    bltz a0, dirfd_fstatat_root_close_error

    la t0, fourth_stage_stat_buf
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, dirfd_fstatat_root_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_expected_len
    bne t1, t2, dirfd_fstatat_root_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    li t2, 1
    bne t1, t2, dirfd_fstatat_root_close_error

    mv a0, s0
    la a1, stage_dirfd_relative_fstatat_ok_msg
    li a2, stage_dirfd_relative_fstatat_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s2
    la a1, post_init_data_name
    mv a2, s2
    la a3, post_init_data_hardlink_name
    li a4, 0
    li a7, SYS_LINKAT
    ecall
    bltz a0, dirfd_linkat_root_close_error

    mv a0, s2
    la a1, post_init_data_hardlink_name
    li a2, O_RDWR
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, dirfd_linkat_root_close_error
    mv s3, a0

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_expected_len
    bne a0, t1, dirfd_linkat_data_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, dirfd_linkat_root_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_expected
    li a2, post_init_data_expected_len
    jal ra, compare_bytes
    bltz a0, dirfd_linkat_root_close_error

    mv a0, s0
    la a1, stage_dirfd_relative_linkat_ok_msg
    li a2, stage_dirfd_relative_linkat_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s0
    la a1, stage_linkat_target_readable_msg
    li a2, stage_linkat_target_readable_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, AT_FDCWD
    la a1, post_init_data_path
    la a2, fourth_stage_stat_buf
    li a3, 0
    li a7, SYS_NEWFSTATAT
    ecall
    bltz a0, hardlink_stat_smoke_failed

    la t0, fourth_stage_stat_buf
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, hardlink_stat_smoke_failed
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_expected_len
    bne t1, t2, hardlink_stat_smoke_failed
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    li t2, 2
    bne t1, t2, hardlink_stat_smoke_failed

    li a0, AT_FDCWD
    la a1, post_init_data_hardlink_path
    la a2, fourth_stage_stat_buf_alt
    li a3, 0
    li a7, SYS_NEWFSTATAT
    ecall
    bltz a0, hardlink_stat_smoke_failed

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, hardlink_stat_smoke_failed
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_expected_len
    bne t1, t2, hardlink_stat_smoke_failed
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    li t2, 2
    bne t1, t2, hardlink_stat_smoke_failed
    ld t1, fourth_stage_stat_st_ino_offset(t0)
    la t0, fourth_stage_stat_buf
    ld t2, fourth_stage_stat_st_ino_offset(t0)
    bne t1, t2, hardlink_stat_smoke_failed

    mv a0, s0
    la a1, stage_linkat_shared_inode_ok_msg
    li a2, stage_linkat_shared_inode_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, AT_FDCWD
    la a1, post_init_data_path
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, hardlink_survival_smoke_failed

    li a0, AT_FDCWD
    la a1, post_init_data_hardlink_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, hardlink_survival_smoke_failed
    mv t0, a0

    mv a0, t0
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_expected_len
    bne a0, t1, hardlink_survival_close_error

    mv a0, t0
    li a7, SYS_CLOSE
    ecall
    bltz a0, hardlink_survival_smoke_failed

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_expected
    li a2, post_init_data_expected_len
    jal ra, compare_bytes
    bltz a0, hardlink_survival_smoke_failed

    mv a0, s0
    la a1, stage_unlinkat_origin_hardlink_survives_msg
    li a2, stage_unlinkat_origin_hardlink_survives_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, AT_FDCWD
    la a1, post_init_data_hardlink_path
    la a2, fourth_stage_stat_buf
    li a3, 0
    li a7, SYS_NEWFSTATAT
    ecall
    bltz a0, hardlink_nlink_smoke_failed

    la t0, fourth_stage_stat_buf
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, hardlink_nlink_smoke_failed
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_expected_len
    bne t1, t2, hardlink_nlink_smoke_failed
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    li t2, 1
    bne t1, t2, hardlink_nlink_smoke_failed
    ld t1, fourth_stage_stat_st_ino_offset(t0)
    la t0, fourth_stage_stat_buf_alt
    ld t2, fourth_stage_stat_st_ino_offset(t0)
    bne t1, t2, hardlink_nlink_smoke_failed

    mv a0, s0
    la a1, stage_unlinkat_origin_link_count_dropped_msg
    li a2, stage_unlinkat_origin_link_count_dropped_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s2
    la a1, post_init_data_hardlink_name
    li a2, O_RDWR
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, dirfd_unlinkat_smoke_failed
    mv s3, a0

    mv a0, s2
    la a1, post_init_data_hardlink_name
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, dirfd_unlinkat_data_close_error

    la a0, post_init_data_hardlink_name
    jal ra, dir_contains_name
    bnez a0, dirfd_unlinkat_data_close_error

    mv a0, s0
    la a1, stage_dirfd_relative_unlinkat_ok_msg
    li a2, stage_dirfd_relative_unlinkat_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s2
    la a1, post_init_data_hardlink_name
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bgez a0, dirfd_reopen_close_error
    li t1, -ENOENT
    bne a0, t1, dirfd_reopen_smoke_failed

    mv a0, s0
    la a1, stage_dirfd_relative_reopen_gone_msg
    li a2, stage_dirfd_relative_reopen_gone_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s2
    la a1, post_init_data_hardlink_name
    la a2, fourth_stage_stat_buf
    li a3, 0
    li a7, SYS_NEWFSTATAT
    ecall
    bgez a0, dirfd_gone_fstatat_root_close_error
    li t1, -ENOENT
    bne a0, t1, dirfd_gone_fstatat_root_close_error

    mv a0, s0
    la a1, stage_dirfd_relative_fstatat_gone_msg
    li a2, stage_dirfd_relative_fstatat_gone_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s2
    la a1, post_init_data_hardlink_name
    jal ra, dirfd_contains_name
    bnez a0, dirfd_getdents_root_close_error

    mv a0, s0
    la a1, stage_dirfd_relative_getdents_gone_msg
    li a2, stage_dirfd_relative_getdents_gone_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s0
    la a1, stage_unlinkat_hardlink_dirent_gone_msg
    li a2, stage_unlinkat_hardlink_dirent_gone_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s3
    la a1, fourth_stage_stat_buf
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_fstat_close_error

    la t0, fourth_stage_stat_buf
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_fstat_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_expected_len
    bne t1, t2, open_fd_fstat_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_fstat_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_nlink_zero_msg
    li a2, stage_unlinkat_open_fd_nlink_zero_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_expected_len
    bne a0, t1, dirfd_unlinkat_data_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_expected
    li a2, post_init_data_expected_len
    jal ra, compare_bytes
    bltz a0, dirfd_unlinkat_root_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_survives_msg
    li a2, stage_unlinkat_open_fd_survives_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s3
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_anon_write_close_error

    mv a0, s3
    la a1, post_init_data_anonymous_expected
    li a2, post_init_data_anonymous_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, post_init_data_anonymous_expected_len
    bne a0, t1, open_fd_anon_append_close_error

    mv a0, s3
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_anon_stat_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_anon_stat_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_expected_len
    bne t1, t2, open_fd_anon_stat_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_anon_stat_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_anon_write_ok_msg
    li a2, stage_unlinkat_open_fd_anon_write_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s3
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_anon_readback_close_error

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_anonymous_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_anonymous_expected_len
    bne a0, t1, open_fd_anon_readback_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_anonymous_expected
    li a2, post_init_data_anonymous_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_anon_readback_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_anon_readback_ok_msg
    li a2, stage_unlinkat_open_fd_anon_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li s4, -1
    mv a0, s3
    li a7, SYS_DUP
    ecall
    bltz a0, open_fd_dup_close_error
    mv s4, a0

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, open_fd_dup_close_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_dup_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_dup_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_expected_len
    bne t1, t2, open_fd_dup_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_dup_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_close_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_anonymous_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_anonymous_expected_len
    bne a0, t1, open_fd_dup_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_anonymous_expected
    li a2, post_init_data_anonymous_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_dup_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_dup_survives_msg
    li a2, stage_unlinkat_open_fd_dup_survives_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_write_close_error

    mv a0, s4
    la a1, post_init_data_dup_expected
    li a2, post_init_data_dup_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, post_init_data_dup_expected_len
    bne a0, t1, open_fd_dup_write_close_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_dup_write_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_dup_write_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_dup_expected_len
    bne t1, t2, open_fd_dup_write_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_dup_write_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_dup_write_ok_msg
    li a2, stage_unlinkat_open_fd_dup_write_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_readback_close_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_dup_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_dup_expected_len
    bne a0, t1, open_fd_dup_readback_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_dup_expected
    li a2, post_init_data_dup_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_dup_readback_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_dup_readback_ok_msg
    li a2, stage_unlinkat_open_fd_dup_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a1, 0
    li a7, SYS_FTRUNCATE
    ecall
    bltz a0, open_fd_dup_truncate_close_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_dup_truncate_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_dup_truncate_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    bnez t1, open_fd_dup_truncate_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_dup_truncate_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_truncate_close_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, 1
    li a7, SYS_READ
    ecall
    bnez a0, open_fd_dup_truncate_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_dup_truncate_ok_msg
    li a2, stage_unlinkat_open_fd_dup_truncate_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_truncate_roundtrip_close_error

    mv a0, s4
    la a1, post_init_data_dup_truncate_expected
    li a2, post_init_data_dup_truncate_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, post_init_data_dup_truncate_expected_len
    bne a0, t1, open_fd_dup_truncate_roundtrip_close_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_dup_truncate_roundtrip_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_dup_truncate_roundtrip_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_dup_truncate_expected_len
    bne t1, t2, open_fd_dup_truncate_roundtrip_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_dup_truncate_roundtrip_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_truncate_roundtrip_close_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_dup_truncate_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_dup_truncate_expected_len
    bne a0, t1, open_fd_dup_truncate_roundtrip_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_dup_truncate_expected
    li a2, post_init_data_dup_truncate_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_dup_truncate_roundtrip_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_dup_truncate_roundtrip_ok_msg
    li a2, stage_unlinkat_open_fd_dup_truncate_roundtrip_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a1, 0
    li a2, SEEK_END
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_append_close_error
    li t1, post_init_data_dup_truncate_expected_len
    bne a0, t1, open_fd_dup_append_close_error

    mv a0, s4
    la a1, post_init_data_dup_append_tail
    li a2, post_init_data_dup_append_tail_len
    li a7, SYS_WRITE
    ecall
    li t1, post_init_data_dup_append_tail_len
    bne a0, t1, open_fd_dup_append_close_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_dup_append_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_dup_append_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_dup_append_expected_len
    bne t1, t2, open_fd_dup_append_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_dup_append_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_END
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_append_close_error
    li t1, post_init_data_dup_append_expected_len
    bne a0, t1, open_fd_dup_append_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_append_close_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_dup_append_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_dup_append_expected_len
    bne a0, t1, open_fd_dup_append_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_dup_append_expected
    li a2, post_init_data_dup_append_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_dup_append_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_dup_append_ok_msg
    li a2, stage_unlinkat_open_fd_dup_append_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a1, post_init_data_dup_truncate_expected_len
    li a7, SYS_FTRUNCATE
    ecall
    bltz a0, open_fd_dup_append_truncate_close_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_dup_append_truncate_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_dup_append_truncate_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_dup_truncate_expected_len
    bne t1, t2, open_fd_dup_append_truncate_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_dup_append_truncate_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_END
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_append_truncate_close_error
    li t1, post_init_data_dup_truncate_expected_len
    bne a0, t1, open_fd_dup_append_truncate_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_append_truncate_close_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_dup_truncate_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_dup_truncate_expected_len
    bne a0, t1, open_fd_dup_append_truncate_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_dup_truncate_expected
    li a2, post_init_data_dup_truncate_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_dup_append_truncate_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_dup_append_truncate_ok_msg
    li a2, stage_unlinkat_open_fd_dup_append_truncate_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a1, post_init_data_dup_append_expected_len
    li a7, SYS_FTRUNCATE
    ecall
    bltz a0, open_fd_dup_grow_zero_fill_close_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_dup_grow_zero_fill_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_dup_grow_zero_fill_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_dup_append_expected_len
    bne t1, t2, open_fd_dup_grow_zero_fill_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_dup_grow_zero_fill_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_END
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_grow_zero_fill_close_error
    li t1, post_init_data_dup_append_expected_len
    bne a0, t1, open_fd_dup_grow_zero_fill_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_grow_zero_fill_close_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_dup_append_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_dup_append_expected_len
    bne a0, t1, open_fd_dup_grow_zero_fill_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_dup_truncate_expected
    li a2, post_init_data_dup_truncate_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_dup_grow_zero_fill_close_error

    la t0, fourth_stage_post_init_data_buf
    li t1, post_init_data_dup_truncate_expected_len
    add t0, t0, t1
    li t1, post_init_data_dup_append_tail_len
open_fd_dup_grow_zero_fill_loop:
    beqz t1, open_fd_dup_grow_zero_fill_loop_done
    lbu t2, 0(t0)
    bnez t2, open_fd_dup_grow_zero_fill_close_error
    addi t0, t0, 1
    addi t1, t1, -1
    j open_fd_dup_grow_zero_fill_loop
open_fd_dup_grow_zero_fill_loop_done:

    mv a0, s0
    la a1, stage_unlinkat_open_fd_dup_grow_zero_fill_ok_msg
    li a2, stage_unlinkat_open_fd_dup_grow_zero_fill_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a1, post_init_data_dup_truncate_expected_len
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_grow_tail_write_close_error
    li t1, post_init_data_dup_truncate_expected_len
    bne a0, t1, open_fd_dup_grow_tail_write_close_error

    mv a0, s4
    la a1, post_init_data_dup_append_tail
    li a2, post_init_data_dup_append_tail_len
    li a7, SYS_WRITE
    ecall
    li t1, post_init_data_dup_append_tail_len
    bne a0, t1, open_fd_dup_grow_tail_write_close_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_dup_grow_tail_write_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_dup_grow_tail_write_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_dup_append_expected_len
    bne t1, t2, open_fd_dup_grow_tail_write_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_dup_grow_tail_write_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_END
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_grow_tail_write_close_error
    li t1, post_init_data_dup_append_expected_len
    bne a0, t1, open_fd_dup_grow_tail_write_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_grow_tail_write_close_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_dup_append_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_dup_append_expected_len
    bne a0, t1, open_fd_dup_grow_tail_write_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_dup_append_expected
    li a2, post_init_data_dup_append_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_dup_grow_tail_write_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_dup_grow_tail_write_ok_msg
    li a2, stage_unlinkat_open_fd_dup_grow_tail_write_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, open_fd_dup_fork_close_error
    beqz a0, open_fd_dup_fork_child
    mv t0, a0

    la t1, fourth_stage_wait_status
    sw zero, 0(t1)
    mv a0, t0
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, open_fd_dup_fork_close_error
    bne a0, t0, open_fd_dup_fork_close_error
    lw t2, 0(t1)
    bnez t2, open_fd_dup_fork_close_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_dup_fork_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_dup_fork_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_dup_fork_expected_len
    bne t1, t2, open_fd_dup_fork_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_dup_fork_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_fork_close_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_dup_fork_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_dup_fork_expected_len
    bne a0, t1, open_fd_dup_fork_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_dup_fork_expected
    li a2, post_init_data_dup_fork_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_dup_fork_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_dup_fork_parent_readback_ok_msg
    li a2, stage_unlinkat_open_fd_dup_fork_parent_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li t0, 9
    blt t0, s4, open_fd_procfd_primary_close_error
    addi t0, s4, '0'
    la t1, proc_self_fd_digit
    sb t0, 0(t1)

    li a0, AT_FDCWD
    la a1, proc_self_fd_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, open_fd_procfd_primary_close_error
    mv s3, a0

    mv a0, s3
    la a1, fourth_stage_stat_buf
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_procfd_close_error

    la t0, fourth_stage_stat_buf
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_procfd_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_dup_fork_expected_len
    bne t1, t2, open_fd_procfd_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_procfd_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_procfd_reopen_ok_msg
    li a2, stage_unlinkat_open_fd_procfd_reopen_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s3
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_procfd_close_error

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_dup_fork_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_dup_fork_expected_len
    bne a0, t1, open_fd_procfd_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_dup_fork_expected
    li a2, post_init_data_dup_fork_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_procfd_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, open_fd_procfd_primary_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_procfd_readback_ok_msg
    li a2, stage_unlinkat_open_fd_procfd_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li t0, 9
    blt t0, s4, open_fd_execve_close_error
    addi t0, s4, '0'
    la t1, execve_open_fd_fd_arg
    sb t0, 0(t1)

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, open_fd_execve_close_error
    beqz a0, open_fd_execve_child_exec
    mv t0, a0

    la t1, fourth_stage_wait_status
    sw zero, 0(t1)
    mv a0, t0
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, open_fd_execve_close_error
    bne a0, t0, open_fd_execve_close_error
    lw t2, 0(t1)
    bnez t2, open_fd_execve_close_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_execve_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_execve_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_execve_expected_len
    bne t1, t2, open_fd_execve_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_execve_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_execve_close_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_execve_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_execve_expected_len
    bne a0, t1, open_fd_execve_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_execve_expected
    li a2, post_init_data_execve_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_execve_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_execve_parent_readback_ok_msg
    li a2, stage_unlinkat_open_fd_execve_parent_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    bltz a0, open_fd_execve_root_close_error

    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    bltz a0, open_fd_execve_report_failed

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

open_fd_dup_fork_child:
    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_dup_fork_child_fail

    mv a0, s4
    la a1, post_init_data_dup_fork_expected
    li a2, post_init_data_dup_fork_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, post_init_data_dup_fork_expected_len
    bne a0, t1, open_fd_dup_fork_child_fail

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_dup_fork_child_fail

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_dup_fork_child_fail
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_dup_fork_expected_len
    bne t1, t2, open_fd_dup_fork_child_fail
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_dup_fork_child_fail

    mv a0, s0
    la a1, stage_unlinkat_open_fd_dup_fork_child_write_ok_msg
    li a2, stage_unlinkat_open_fd_dup_fork_child_write_ok_msg_len
    jal ra, write_exact
    bltz a0, open_fd_dup_fork_child_fail

    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

open_fd_dup_fork_child_fail:
    li a0, 1
    li a7, SYS_EXIT_GROUP
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

symlink_smoke_close_error:
    mv a0, t0
    li a7, SYS_CLOSE
    ecall

symlink_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_symlink_smoke_fail_msg
    li a2, fourth_stage_symlink_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

symlink_stat_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_symlink_stat_smoke_fail_msg
    li a2, fourth_stage_symlink_stat_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

readlinkat_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_readlinkat_smoke_fail_msg
    li a2, fourth_stage_readlinkat_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

symlink_cleanup_failed:
    mv a0, s0
    la a1, fourth_stage_symlink_cleanup_fail_msg
    li a2, fourth_stage_symlink_cleanup_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

symlink_dirent_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_symlink_dirent_smoke_fail_msg
    li a2, fourth_stage_symlink_dirent_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

dirfd_openat_data_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

dirfd_openat_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

dirfd_openat_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_dirfd_openat_smoke_fail_msg
    li a2, fourth_stage_dirfd_openat_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

dirfd_fstatat_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

dirfd_fstatat_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_dirfd_fstatat_smoke_fail_msg
    li a2, fourth_stage_dirfd_fstatat_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

dirfd_linkat_data_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

dirfd_linkat_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

dirfd_linkat_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_dirfd_linkat_smoke_fail_msg
    li a2, fourth_stage_dirfd_linkat_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

dirfd_unlinkat_data_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

dirfd_unlinkat_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

dirfd_unlinkat_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_dirfd_unlinkat_smoke_fail_msg
    li a2, fourth_stage_dirfd_unlinkat_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

dirfd_reopen_close_error:
    li a7, SYS_CLOSE
    ecall

dirfd_reopen_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_dirfd_reopen_smoke_fail_msg
    li a2, fourth_stage_dirfd_reopen_smoke_fail_msg_len
    jal ra, write_exact
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    li a0, 1
    li a7, SYS_EXIT
    ecall

dirfd_gone_fstatat_root_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

dirfd_gone_fstatat_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_dirfd_gone_fstatat_smoke_fail_msg
    li a2, fourth_stage_dirfd_gone_fstatat_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

dirfd_getdents_root_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

dirfd_getdents_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_dirfd_getdents_smoke_fail_msg
    li a2, fourth_stage_dirfd_getdents_smoke_fail_msg_len
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

hardlink_smoke_close_error:
    mv a0, t0
    li a7, SYS_CLOSE
    ecall

hardlink_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_hardlink_smoke_fail_msg
    la a2, fourth_stage_hardlink_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

hardlink_stat_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_hardlink_stat_smoke_fail_msg
    la a2, fourth_stage_hardlink_stat_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

hardlink_survival_close_error:
    mv a0, t0
    li a7, SYS_CLOSE
    ecall

hardlink_survival_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_hardlink_survival_smoke_fail_msg
    la a2, fourth_stage_hardlink_survival_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

hardlink_nlink_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_hardlink_nlink_smoke_fail_msg
    la a2, fourth_stage_hardlink_nlink_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

hardlink_cleanup_failed:
    mv a0, s0
    la a1, fourth_stage_hardlink_cleanup_fail_msg
    la a2, fourth_stage_hardlink_cleanup_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

hardlink_dirent_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_hardlink_dirent_smoke_fail_msg
    la a2, fourth_stage_hardlink_dirent_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_fstat_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_fstat_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_fstat_smoke_fail_msg
    la a2, fourth_stage_open_fd_fstat_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_survival_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_survival_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_survival_smoke_fail_msg
    la a2, fourth_stage_open_fd_survival_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_anon_write_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
open_fd_anon_write_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_anon_write_smoke_failed

open_fd_anon_append_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_anon_append_fail_msg
    la a2, fourth_stage_open_fd_anon_append_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, write_failed
    j open_fd_anon_write_smoke_failed

open_fd_anon_stat_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_anon_stat_fail_msg
    la a2, fourth_stage_open_fd_anon_stat_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, write_failed
    j open_fd_anon_write_smoke_failed

open_fd_anon_append_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_anon_append_failed

open_fd_anon_stat_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_anon_stat_failed

open_fd_anon_write_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_anon_write_smoke_fail_msg
    la a2, fourth_stage_open_fd_anon_write_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_anon_readback_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_anon_readback_smoke_failed

open_fd_anon_readback_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_anon_readback_smoke_fail_msg
    la a2, fourth_stage_open_fd_anon_readback_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_dup_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

open_fd_dup_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_dup_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_dup_smoke_fail_msg
    la a2, fourth_stage_open_fd_dup_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_dup_write_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_dup_write_smoke_failed

open_fd_dup_write_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_dup_write_smoke_fail_msg
    la a2, fourth_stage_open_fd_dup_write_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_dup_readback_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_dup_readback_smoke_failed

open_fd_dup_readback_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_dup_readback_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_dup_readback_smoke_fail_msg
    la a2, fourth_stage_open_fd_dup_readback_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_dup_truncate_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_dup_truncate_smoke_failed

open_fd_dup_truncate_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_dup_truncate_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_dup_truncate_smoke_fail_msg
    la a2, fourth_stage_open_fd_dup_truncate_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_dup_truncate_roundtrip_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_dup_truncate_roundtrip_smoke_failed

open_fd_dup_truncate_roundtrip_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_dup_truncate_roundtrip_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_dup_truncate_roundtrip_fail_msg
    la a2, fourth_stage_open_fd_dup_truncate_roundtrip_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_dup_append_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_dup_append_smoke_failed

open_fd_dup_append_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_dup_append_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_dup_append_fail_msg
    la a2, fourth_stage_open_fd_dup_append_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_dup_append_truncate_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_dup_append_truncate_smoke_failed

open_fd_dup_append_truncate_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_dup_append_truncate_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_dup_append_truncate_fail_msg
    la a2, fourth_stage_open_fd_dup_append_truncate_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_dup_grow_zero_fill_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_dup_grow_zero_fill_smoke_failed

open_fd_dup_grow_zero_fill_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_dup_grow_zero_fill_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_dup_grow_zero_fill_fail_msg
    la a2, fourth_stage_open_fd_dup_grow_zero_fill_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_dup_grow_tail_write_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_dup_grow_tail_write_smoke_failed

open_fd_dup_grow_tail_write_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_dup_grow_tail_write_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_dup_grow_tail_write_fail_msg
    la a2, fourth_stage_open_fd_dup_grow_tail_write_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_dup_fork_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    j open_fd_dup_fork_smoke_failed

open_fd_dup_fork_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_dup_fork_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_dup_fork_smoke_fail_msg
    la a2, fourth_stage_open_fd_dup_fork_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_procfd_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

open_fd_procfd_primary_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall

open_fd_procfd_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_procfd_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_procfd_smoke_fail_msg
    la a2, fourth_stage_open_fd_procfd_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_execve_child_exec:
    la a0, execve_open_fd_self_path
    la a1, execve_open_fd_argv
    la a2, execve_open_fd_envp
    li a7, SYS_EXECVE
    ecall
    j open_fd_execve_report_failed

open_fd_execve_smoke_failed_entry:
    li s0, INHERITED_CONSOLE_FD

    li a0, AT_FDCWD
    la a1, console_path
    li a2, O_RDWR
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, open_fd_execve_report_failed
    mv s0, a0
    j open_fd_execve_report_failed

open_fd_execve_child_entry:
    li s0, INHERITED_CONSOLE_FD

    li a0, AT_FDCWD
    la a1, console_path
    li a2, O_RDWR
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, open_fd_execve_child_start
    mv s0, a0

open_fd_execve_child_start:
    mv a0, s4
    la a1, fourth_stage_stat_buf
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_execve_report_failed

    la t0, fourth_stage_stat_buf
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_execve_report_failed
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_dup_fork_expected_len
    bne t1, t2, open_fd_execve_report_failed
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_execve_report_failed

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_execve_report_failed

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_dup_fork_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_dup_fork_expected_len
    bne a0, t1, open_fd_execve_report_failed

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_dup_fork_expected
    li a2, post_init_data_dup_fork_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_execve_report_failed

    mv a0, s0
    la a1, stage_unlinkat_open_fd_execve_child_readback_ok_msg
    li a2, stage_unlinkat_open_fd_execve_child_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, open_fd_execve_report_failed

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_execve_report_failed

    mv a0, s4
    la a1, post_init_data_execve_expected
    li a2, post_init_data_execve_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, post_init_data_execve_expected_len
    bne a0, t1, open_fd_execve_report_failed

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_execve_report_failed

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_execve_report_failed
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_execve_expected_len
    bne t1, t2, open_fd_execve_report_failed
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_execve_report_failed

    mv a0, s0
    la a1, stage_unlinkat_open_fd_execve_child_write_ok_msg
    li a2, stage_unlinkat_open_fd_execve_child_write_ok_msg_len
    jal ra, write_exact
    bltz a0, open_fd_execve_report_failed

    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

open_fd_execve_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall

open_fd_execve_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_execve_report_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_execve_smoke_fail_msg
    la a2, fourth_stage_open_fd_execve_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

write_failed:
    li a0, 1
    la a1, fourth_stage_write_fail_msg
    la a2, fourth_stage_write_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
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

dirfd_contains_name:
    addi sp, sp, -40
    sd ra, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    sd s6, 0(sp)

    mv s4, a0
    mv s5, a1

dirfd_contains_name_read_loop:
    mv a0, s4
    la a1, fourth_stage_dirent_buf
    li a2, fourth_stage_dirent_buf_len
    li a7, SYS_GETDENTS64
    ecall
    bltz a0, dirfd_contains_name_error
    beqz a0, dirfd_contains_name_not_found
    mv s6, a0
    la s3, fourth_stage_dirent_buf

dirfd_contains_name_parse_loop:
    beqz s6, dirfd_contains_name_read_loop
    lhu t2, 16(s3)
    li t1, 19
    bltu t2, t1, dirfd_contains_name_error
    bltu s6, t2, dirfd_contains_name_error
    addi a0, s3, 19
    mv a1, s5
    jal ra, compare_cstr
    beqz a0, dirfd_contains_name_found
    add s3, s3, t2
    sub s6, s6, t2
    j dirfd_contains_name_parse_loop

dirfd_contains_name_found:
    li a0, 1
    j dirfd_contains_name_done

dirfd_contains_name_not_found:
    li a0, 0
    j dirfd_contains_name_done

dirfd_contains_name_error:
    li a0, -1

dirfd_contains_name_done:
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
