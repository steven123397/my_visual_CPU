#define SYS_CLONE3 435
#define SYS_INOTIFY_INIT1 26
#define SYS_INOTIFY_ADD_WATCH 27
#define SYS_INOTIFY_RM_WATCH 28
#define SYS_DUP 23
#define SYS_FCNTL 25
#define SYS_EXECVE 221
#define SYS_EXECVEAT 281
#define SYS_MMAP 222
#define SYS_MUNMAP 215
#define SYS_WAITID 95
#define SYS_WAIT4 260
#define SYS_EXIT_GROUP 94
#define SYS_RT_SIGPROCMASK 135
#define SYS_SIGNALFD4 74
#define SYS_UNLINKAT 35
#define SYS_SYMLINKAT 36
#define SYS_LINKAT 37
#define SYS_FTRUNCATE 46
#define SYS_CHDIR 49
#define SYS_OPENAT 56
#define SYS_CLOSE 57
#define SYS_PIPE2 59
#define SYS_GETDENTS64 61
#define SYS_LSEEK 62
#define SYS_READ 63
#define SYS_WRITE 64
#define SYS_EVENTFD2 19
#define SYS_EPOLL_CREATE1 20
#define SYS_EPOLL_CTL 21
#define SYS_EPOLL_PWAIT 22
#define SYS_PPOLL 73
#define SYS_SPLICE 76
#define SYS_READLINKAT 78
#define SYS_NEWFSTATAT 79
#define SYS_FSTAT 80
#define SYS_TIMERFD_CREATE 85
#define SYS_TIMERFD_SETTIME 86
#define SYS_EXIT 93
#define SYS_FUTEX 98
#define SYS_NANOSLEEP 101
#define SYS_SOCKETPAIR 199
#define SYS_SENDMSG 211
#define SYS_RECVMSG 212
#define SYS_PIDFD_OPEN 434
#define SYS_OPENAT2 437
#define SYS_RENAMEAT2 276
#define SYS_COPY_FILE_RANGE 285
#define SYS_STATX 291

#define ENOENT 2
#define EBADF 9
#define EAGAIN 11
#define SIGCHLD 17

#define AT_FDCWD -100
#define AT_EMPTY_PATH 4096
#define AT_REMOVEDIR 512
#define AT_SYMLINK_NOFOLLOW 256
#define AF_UNIX 1
#define SOL_SOCKET 1
#define SCM_RIGHTS 1
#define IN_CLOSE_WRITE 8
#define EPOLL_CTL_ADD 1
#define POLLIN 1
#define CLD_EXITED 1
#define CLOCK_MONOTONIC 1
#define O_RDONLY 0
#define O_RDWR 2
#define O_CREAT 64
#define O_TRUNC 512
#define P_PIDFD 3
#define SOCK_STREAM 1
#define SEEK_SET 0
#define SEEK_END 2
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_READ_WRITE 3
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 32
#define MAP_SHARED_ANONYMOUS 33
#define F_GETFD 1
#define F_SETFD 2
#define FD_CLOEXEC 1
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define RESOLVE_NO_SYMLINKS 4
#define RESOLVE_BENEATH 8
#define RENAME_EXCHANGE 2
#define SIG_BLOCK 0
#define WEXITED 4

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
post_init_data_mmap_expected:
    .asciz "post-mmap-dup-ok\nsyncok\n"
post_init_data_mmap_expected_end:
post_init_data_mmap_private_expected:
    .asciz "post-cowp-dup-ok\nsyncok\n"
post_init_data_mmap_private_expected_end:
post_init_data_mmap_shared_fork_expected:
    .asciz "post-mshf-dup-ok\nsyncok\n"
post_init_data_mmap_shared_fork_expected_end:
pipe2_roundtrip_expected:
    .asciz "pipe2-fork-ok\n"
pipe2_roundtrip_expected_end:
futex_shared_expected:
    .asciz "futex-shared-ok\n"
futex_shared_expected_end:
socketpair_roundtrip_expected:
    .asciz "socketpair-fork-ok\n"
socketpair_roundtrip_expected_end:
self_elf_magic_expected:
    .byte 0x7f, 'E', 'L', 'F'
self_elf_magic_expected_end:
renameat2_src_name:
    .asciz "post-init-renameat2-src"
renameat2_dst_name:
    .asciz "post-init-renameat2-dst"
renameat2_src_expected:
    .asciz "renameat2-src\n"
renameat2_src_expected_end:
renameat2_dst_expected:
    .asciz "renameat2-dst\n"
renameat2_dst_expected_end:
copy_file_range_src_name:
    .asciz "post-init-copyfr-src"
copy_file_range_dst_name:
    .asciz "post-init-copyfr-dst"
copy_file_range_expected:
    .asciz "copy-file-range-ok\n"
copy_file_range_expected_end:
splice_src_name:
    .asciz "post-init-splice-src"
splice_dst_name:
    .asciz "post-init-splice-dst"
splice_expected:
    .asciz "splice-pipe-file-ok\n"
splice_expected_end:
statx_name:
    .asciz "post-init-statx"
statx_expected:
    .asciz "statx-size-mode-ok\n"
statx_expected_end:
inotify_name:
    .asciz "post-init-inotify"
inotify_expected:
    .asciz "inotify-close-write-ok\n"
inotify_expected_end:
timerfd_expected:
    .quad 1
execve_open_fd_self_path:
    .asciz "/post-init-cleanup-smoke"
execve_open_fd_self_name:
    .asciz "post-init-cleanup-smoke"
execve_open_fd_mode_arg:
    .asciz "--exec-open-fd"
execve_cloexec_fd_mode_arg:
    .asciz "--exec-cloexec-fd"
execveat_self_fd_mode_arg:
    .asciz "--execveat-self-fd"
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
stage_unlinkat_open_fd_cloexec_child_closed_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-cloexec-child-closed-ok\n"
stage_unlinkat_open_fd_cloexec_child_closed_ok_msg_end:
stage_unlinkat_open_fd_cloexec_parent_readback_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-cloexec-parent-readback-ok\n"
stage_unlinkat_open_fd_cloexec_parent_readback_ok_msg_end:
stage_execveat_self_fd_child_entered_ok_msg:
    .asciz "mycpu linux userland: stage=execveat-self-fd-child-entered-ok\n"
stage_execveat_self_fd_child_entered_ok_msg_end:
stage_execveat_self_fd_parent_wait_ok_msg:
    .asciz "mycpu linux userland: stage=execveat-self-fd-parent-wait-ok\n"
stage_execveat_self_fd_parent_wait_ok_msg_end:
stage_unlinkat_open_fd_mmap_shared_write_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-write-ok\n"
stage_unlinkat_open_fd_mmap_shared_write_ok_msg_end:
stage_unlinkat_open_fd_mmap_shared_readback_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-readback-ok\n"
stage_unlinkat_open_fd_mmap_shared_readback_ok_msg_end:
stage_unlinkat_open_fd_mmap_private_cow_write_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-mmap-private-cow-write-ok\n"
stage_unlinkat_open_fd_mmap_private_cow_write_ok_msg_end:
stage_unlinkat_open_fd_mmap_private_fd_unchanged_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-mmap-private-fd-unchanged-ok\n"
stage_unlinkat_open_fd_mmap_private_fd_unchanged_ok_msg_end:
stage_unlinkat_open_fd_mmap_shared_fork_child_write_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-fork-child-write-ok\n"
stage_unlinkat_open_fd_mmap_shared_fork_child_write_ok_msg_end:
stage_unlinkat_open_fd_mmap_shared_fork_parent_readback_ok_msg:
    .asciz "mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-fork-parent-readback-ok\n"
stage_unlinkat_open_fd_mmap_shared_fork_parent_readback_ok_msg_end:
stage_pipe2_fork_child_write_ok_msg:
    .asciz "mycpu linux userland: stage=pipe2-fork-child-write-ok\n"
stage_pipe2_fork_child_write_ok_msg_end:
stage_pipe2_fork_parent_readback_ok_msg:
    .asciz "mycpu linux userland: stage=pipe2-fork-parent-readback-ok\n"
stage_pipe2_fork_parent_readback_ok_msg_end:
stage_futex_shared_child_wake_ok_msg:
    .asciz "mycpu linux userland: stage=futex-shared-child-wake-ok\n"
stage_futex_shared_child_wake_ok_msg_end:
stage_futex_shared_parent_readback_ok_msg:
    .asciz "mycpu linux userland: stage=futex-shared-parent-readback-ok\n"
stage_futex_shared_parent_readback_ok_msg_end:
stage_socketpair_fork_child_write_ok_msg:
    .asciz "mycpu linux userland: stage=socketpair-fork-child-write-ok\n"
stage_socketpair_fork_child_write_ok_msg_end:
stage_socketpair_fork_parent_readback_ok_msg:
    .asciz "mycpu linux userland: stage=socketpair-fork-parent-readback-ok\n"
stage_socketpair_fork_parent_readback_ok_msg_end:
stage_openat2_self_beneath_readback_ok_msg:
    .asciz "mycpu linux userland: stage=openat2-self-beneath-readback-ok\n"
stage_openat2_self_beneath_readback_ok_msg_end:
stage_pidfd_open_ppoll_waitid_ok_msg:
    .asciz "mycpu linux userland: stage=pidfd-open-ppoll-waitid-ok\n"
stage_pidfd_open_ppoll_waitid_ok_msg_end:
stage_signalfd_sigchld_readback_ok_msg:
    .asciz "mycpu linux userland: stage=signalfd-sigchld-readback-ok\n"
stage_signalfd_sigchld_readback_ok_msg_end:
stage_renameat2_exchange_readback_ok_msg:
    .asciz "mycpu linux userland: stage=renameat2-exchange-readback-ok\n"
stage_renameat2_exchange_readback_ok_msg_end:
stage_eventfd_epoll_readback_ok_msg:
    .asciz "mycpu linux userland: stage=eventfd-epoll-readback-ok\n"
stage_eventfd_epoll_readback_ok_msg_end:
stage_scm_rights_parent_readback_ok_msg:
    .asciz "mycpu linux userland: stage=scm-rights-parent-readback-ok\n"
stage_scm_rights_parent_readback_ok_msg_end:
stage_copy_file_range_readback_ok_msg:
    .asciz "mycpu linux userland: stage=copy-file-range-readback-ok\n"
stage_copy_file_range_readback_ok_msg_end:
stage_splice_pipe_file_readback_ok_msg:
    .asciz "mycpu linux userland: stage=splice-pipe-file-readback-ok\n"
stage_splice_pipe_file_readback_ok_msg_end:
stage_statx_readback_ok_msg:
    .asciz "mycpu linux userland: stage=statx-size-mode-readback-ok\n"
stage_statx_readback_ok_msg_end:
stage_inotify_close_write_event_ok_msg:
    .asciz "mycpu linux userland: stage=inotify-close-write-event-ok\n"
stage_inotify_close_write_event_ok_msg_end:
stage_timerfd_one_shot_readback_ok_msg:
    .asciz "mycpu linux userland: stage=timerfd-one-shot-readback-ok\n"
stage_timerfd_one_shot_readback_ok_msg_end:
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
fourth_stage_open_fd_cloexec_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd cloexec smoke failed\n"
fourth_stage_open_fd_cloexec_smoke_fail_msg_end:
fourth_stage_execveat_self_fd_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage execveat self-fd smoke failed\n"
fourth_stage_execveat_self_fd_smoke_fail_msg_end:
fourth_stage_open_fd_mmap_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd mmap smoke failed\n"
fourth_stage_open_fd_mmap_smoke_fail_msg_end:
fourth_stage_open_fd_mmap_private_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd mmap private smoke failed\n"
fourth_stage_open_fd_mmap_private_smoke_fail_msg_end:
fourth_stage_open_fd_mmap_fork_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage open fd mmap fork smoke failed\n"
fourth_stage_open_fd_mmap_fork_smoke_fail_msg_end:
fourth_stage_pipe2_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage pipe2 smoke failed\n"
fourth_stage_pipe2_smoke_fail_msg_end:
fourth_stage_futex_shared_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage futex shared smoke failed\n"
fourth_stage_futex_shared_smoke_fail_msg_end:
fourth_stage_socketpair_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage socketpair smoke failed\n"
fourth_stage_socketpair_smoke_fail_msg_end:
fourth_stage_openat2_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage openat2 smoke failed\n"
fourth_stage_openat2_smoke_fail_msg_end:
fourth_stage_pidfd_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage pidfd smoke failed\n"
fourth_stage_pidfd_smoke_fail_msg_end:
fourth_stage_signalfd_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage signalfd smoke failed\n"
fourth_stage_signalfd_smoke_fail_msg_end:
fourth_stage_renameat2_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage renameat2 smoke failed\n"
fourth_stage_renameat2_smoke_fail_msg_end:
fourth_stage_eventfd_epoll_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage eventfd epoll smoke failed\n"
fourth_stage_eventfd_epoll_smoke_fail_msg_end:
fourth_stage_scm_rights_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage scm-rights smoke failed\n"
fourth_stage_scm_rights_smoke_fail_msg_end:
fourth_stage_copy_file_range_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage copy-file-range smoke failed\n"
fourth_stage_copy_file_range_smoke_fail_msg_end:
fourth_stage_splice_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage splice smoke failed\n"
fourth_stage_splice_smoke_fail_msg_end:
fourth_stage_statx_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage statx smoke failed\n"
fourth_stage_statx_smoke_fail_msg_end:
fourth_stage_inotify_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage inotify smoke failed\n"
fourth_stage_inotify_smoke_fail_msg_end:
fourth_stage_timerfd_smoke_fail_msg:
    .asciz "mycpu linux userland: fourth-stage timerfd smoke failed\n"
fourth_stage_timerfd_smoke_fail_msg_end:
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
.set post_init_data_mmap_expected_len, post_init_data_mmap_expected_end - post_init_data_mmap_expected
.set post_init_data_mmap_expected_len, post_init_data_mmap_expected_len - 1
.set post_init_data_mmap_private_expected_len, post_init_data_mmap_private_expected_end - post_init_data_mmap_private_expected
.set post_init_data_mmap_private_expected_len, post_init_data_mmap_private_expected_len - 1
.set post_init_data_mmap_shared_fork_expected_len, post_init_data_mmap_shared_fork_expected_end - post_init_data_mmap_shared_fork_expected
.set post_init_data_mmap_shared_fork_expected_len, post_init_data_mmap_shared_fork_expected_len - 1
.set pipe2_roundtrip_expected_len, pipe2_roundtrip_expected_end - pipe2_roundtrip_expected
.set pipe2_roundtrip_expected_len, pipe2_roundtrip_expected_len - 1
.set futex_shared_expected_len, futex_shared_expected_end - futex_shared_expected
.set futex_shared_expected_len, futex_shared_expected_len - 1
.set socketpair_roundtrip_expected_len, socketpair_roundtrip_expected_end - socketpair_roundtrip_expected
.set socketpair_roundtrip_expected_len, socketpair_roundtrip_expected_len - 1
.set renameat2_src_expected_len, renameat2_src_expected_end - renameat2_src_expected
.set renameat2_src_expected_len, renameat2_src_expected_len - 1
.set renameat2_dst_expected_len, renameat2_dst_expected_end - renameat2_dst_expected
.set renameat2_dst_expected_len, renameat2_dst_expected_len - 1
.set copy_file_range_expected_len, copy_file_range_expected_end - copy_file_range_expected
.set copy_file_range_expected_len, copy_file_range_expected_len - 1
.set splice_expected_len, splice_expected_end - splice_expected
.set splice_expected_len, splice_expected_len - 1
.set statx_expected_len, statx_expected_end - statx_expected
.set statx_expected_len, statx_expected_len - 1
.set inotify_expected_len, inotify_expected_end - inotify_expected
.set inotify_expected_len, inotify_expected_len - 1
.set self_elf_magic_expected_len, self_elf_magic_expected_end - self_elf_magic_expected
.set futex_shared_state_offset, 0
.set futex_shared_data_offset, 8
.set futex_shared_map_len, futex_shared_data_offset + futex_shared_expected_len
.set fourth_stage_siginfo_buf_len, 128
.set fourth_stage_siginfo_signo_offset, 0
.set fourth_stage_siginfo_code_offset, 8
.set fourth_stage_siginfo_pid_offset, 16
.set fourth_stage_siginfo_status_offset, 24
.set fourth_stage_signalfd_siginfo_len, 128
.set fourth_stage_signalfd_ssi_signo_offset, 0
.set fourth_stage_signalfd_ssi_code_offset, 8
.set fourth_stage_signalfd_ssi_pid_offset, 12
.set fourth_stage_signalfd_ssi_status_offset, 40
.set fourth_stage_rt_sigset_len, 8
.set fourth_stage_pollfd_fd_offset, 0
.set fourth_stage_pollfd_events_offset, 4
.set fourth_stage_pollfd_revents_offset, 6
.set fourth_stage_pollfd_len, 8
.set fourth_stage_epoll_event_events_offset, 0
.set fourth_stage_epoll_event_data_offset, 8
.set fourth_stage_epoll_event_len, 16
.set fourth_stage_eventfd_counter_len, 8
.set fourth_stage_scm_iovec_base_offset, 0
.set fourth_stage_scm_iovec_len_offset, 8
.set fourth_stage_scm_iovec_len, 16
.set fourth_stage_scm_msghdr_name_offset, 0
.set fourth_stage_scm_msghdr_namelen_offset, 8
.set fourth_stage_scm_msghdr_iov_offset, 16
.set fourth_stage_scm_msghdr_iovlen_offset, 24
.set fourth_stage_scm_msghdr_control_offset, 32
.set fourth_stage_scm_msghdr_controllen_offset, 40
.set fourth_stage_scm_msghdr_flags_offset, 48
.set fourth_stage_scm_msghdr_len, 56
.set fourth_stage_scm_cmsg_len_offset, 0
.set fourth_stage_scm_cmsg_level_offset, 8
.set fourth_stage_scm_cmsg_type_offset, 12
.set fourth_stage_scm_cmsg_data_offset, 16
.set fourth_stage_scm_cmsg_fd_len, 20
.set fourth_stage_scm_cmsg_buf_len, 24
.set fourth_stage_scm_data_len, 1
.set fourth_stage_stat_buf_len, 128
.set fourth_stage_statx_buf_len, 256
.set fourth_stage_stat_st_ino_offset, 8
.set fourth_stage_stat_st_mode_offset, 16
.set fourth_stage_stat_st_nlink_offset, 20
.set fourth_stage_stat_st_size_offset, 48
.set fourth_stage_stat_mode_mask, 61440
.set fourth_stage_stat_mode_regular, 32768
.set fourth_stage_stat_mode_symlink, 40960
.set fourth_stage_statx_mask_offset, 0
.set fourth_stage_statx_mode_offset, 28
.set fourth_stage_statx_size_offset, 40
.set fourth_stage_statx_mask_basic, 515
.set fourth_stage_timerfd_spec_len, 32
.set fourth_stage_timerfd_interval_sec_offset, 0
.set fourth_stage_timerfd_interval_nsec_offset, 8
.set fourth_stage_timerfd_value_sec_offset, 16
.set fourth_stage_timerfd_value_nsec_offset, 24
.set fourth_stage_timerfd_one_shot_nsec, 10000000
.set fourth_stage_inotify_event_wd_offset, 0
.set fourth_stage_inotify_event_mask_offset, 4
.set fourth_stage_inotify_event_cookie_offset, 8
.set fourth_stage_inotify_event_name_len_offset, 12
.set fourth_stage_inotify_event_len, 16
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
.set stage_unlinkat_open_fd_cloexec_child_closed_ok_msg_len, stage_unlinkat_open_fd_cloexec_child_closed_ok_msg_end - stage_unlinkat_open_fd_cloexec_child_closed_ok_msg
.set stage_unlinkat_open_fd_cloexec_child_closed_ok_msg_len, stage_unlinkat_open_fd_cloexec_child_closed_ok_msg_len - 1
.set stage_unlinkat_open_fd_cloexec_parent_readback_ok_msg_len, stage_unlinkat_open_fd_cloexec_parent_readback_ok_msg_end - stage_unlinkat_open_fd_cloexec_parent_readback_ok_msg
.set stage_unlinkat_open_fd_cloexec_parent_readback_ok_msg_len, stage_unlinkat_open_fd_cloexec_parent_readback_ok_msg_len - 1
.set stage_execveat_self_fd_child_entered_ok_msg_len, stage_execveat_self_fd_child_entered_ok_msg_end - stage_execveat_self_fd_child_entered_ok_msg
.set stage_execveat_self_fd_child_entered_ok_msg_len, stage_execveat_self_fd_child_entered_ok_msg_len - 1
.set stage_execveat_self_fd_parent_wait_ok_msg_len, stage_execveat_self_fd_parent_wait_ok_msg_end - stage_execveat_self_fd_parent_wait_ok_msg
.set stage_execveat_self_fd_parent_wait_ok_msg_len, stage_execveat_self_fd_parent_wait_ok_msg_len - 1
.set stage_unlinkat_open_fd_mmap_shared_write_ok_msg_len, stage_unlinkat_open_fd_mmap_shared_write_ok_msg_end - stage_unlinkat_open_fd_mmap_shared_write_ok_msg
.set stage_unlinkat_open_fd_mmap_shared_write_ok_msg_len, stage_unlinkat_open_fd_mmap_shared_write_ok_msg_len - 1
.set stage_unlinkat_open_fd_mmap_shared_readback_ok_msg_len, stage_unlinkat_open_fd_mmap_shared_readback_ok_msg_end - stage_unlinkat_open_fd_mmap_shared_readback_ok_msg
.set stage_unlinkat_open_fd_mmap_shared_readback_ok_msg_len, stage_unlinkat_open_fd_mmap_shared_readback_ok_msg_len - 1
.set stage_unlinkat_open_fd_mmap_private_cow_write_ok_msg_len, stage_unlinkat_open_fd_mmap_private_cow_write_ok_msg_end - stage_unlinkat_open_fd_mmap_private_cow_write_ok_msg
.set stage_unlinkat_open_fd_mmap_private_cow_write_ok_msg_len, stage_unlinkat_open_fd_mmap_private_cow_write_ok_msg_len - 1
.set stage_unlinkat_open_fd_mmap_private_fd_unchanged_ok_msg_len, stage_unlinkat_open_fd_mmap_private_fd_unchanged_ok_msg_end - stage_unlinkat_open_fd_mmap_private_fd_unchanged_ok_msg
.set stage_unlinkat_open_fd_mmap_private_fd_unchanged_ok_msg_len, stage_unlinkat_open_fd_mmap_private_fd_unchanged_ok_msg_len - 1
.set stage_unlinkat_open_fd_mmap_shared_fork_child_write_ok_msg_len, stage_unlinkat_open_fd_mmap_shared_fork_child_write_ok_msg_end - stage_unlinkat_open_fd_mmap_shared_fork_child_write_ok_msg
.set stage_unlinkat_open_fd_mmap_shared_fork_child_write_ok_msg_len, stage_unlinkat_open_fd_mmap_shared_fork_child_write_ok_msg_len - 1
.set stage_unlinkat_open_fd_mmap_shared_fork_parent_readback_ok_msg_len, stage_unlinkat_open_fd_mmap_shared_fork_parent_readback_ok_msg_end - stage_unlinkat_open_fd_mmap_shared_fork_parent_readback_ok_msg
.set stage_unlinkat_open_fd_mmap_shared_fork_parent_readback_ok_msg_len, stage_unlinkat_open_fd_mmap_shared_fork_parent_readback_ok_msg_len - 1
.set stage_pipe2_fork_child_write_ok_msg_len, stage_pipe2_fork_child_write_ok_msg_end - stage_pipe2_fork_child_write_ok_msg
.set stage_pipe2_fork_child_write_ok_msg_len, stage_pipe2_fork_child_write_ok_msg_len - 1
.set stage_pipe2_fork_parent_readback_ok_msg_len, stage_pipe2_fork_parent_readback_ok_msg_end - stage_pipe2_fork_parent_readback_ok_msg
.set stage_pipe2_fork_parent_readback_ok_msg_len, stage_pipe2_fork_parent_readback_ok_msg_len - 1
.set stage_futex_shared_child_wake_ok_msg_len, stage_futex_shared_child_wake_ok_msg_end - stage_futex_shared_child_wake_ok_msg
.set stage_futex_shared_child_wake_ok_msg_len, stage_futex_shared_child_wake_ok_msg_len - 1
.set stage_futex_shared_parent_readback_ok_msg_len, stage_futex_shared_parent_readback_ok_msg_end - stage_futex_shared_parent_readback_ok_msg
.set stage_futex_shared_parent_readback_ok_msg_len, stage_futex_shared_parent_readback_ok_msg_len - 1
.set stage_socketpair_fork_child_write_ok_msg_len, stage_socketpair_fork_child_write_ok_msg_end - stage_socketpair_fork_child_write_ok_msg
.set stage_socketpair_fork_child_write_ok_msg_len, stage_socketpair_fork_child_write_ok_msg_len - 1
.set stage_socketpair_fork_parent_readback_ok_msg_len, stage_socketpair_fork_parent_readback_ok_msg_end - stage_socketpair_fork_parent_readback_ok_msg
.set stage_socketpair_fork_parent_readback_ok_msg_len, stage_socketpair_fork_parent_readback_ok_msg_len - 1
.set stage_openat2_self_beneath_readback_ok_msg_len, stage_openat2_self_beneath_readback_ok_msg_end - stage_openat2_self_beneath_readback_ok_msg
.set stage_openat2_self_beneath_readback_ok_msg_len, stage_openat2_self_beneath_readback_ok_msg_len - 1
.set stage_pidfd_open_ppoll_waitid_ok_msg_len, stage_pidfd_open_ppoll_waitid_ok_msg_end - stage_pidfd_open_ppoll_waitid_ok_msg
.set stage_pidfd_open_ppoll_waitid_ok_msg_len, stage_pidfd_open_ppoll_waitid_ok_msg_len - 1
.set stage_signalfd_sigchld_readback_ok_msg_len, stage_signalfd_sigchld_readback_ok_msg_end - stage_signalfd_sigchld_readback_ok_msg
.set stage_signalfd_sigchld_readback_ok_msg_len, stage_signalfd_sigchld_readback_ok_msg_len - 1
.set stage_renameat2_exchange_readback_ok_msg_len, stage_renameat2_exchange_readback_ok_msg_end - stage_renameat2_exchange_readback_ok_msg
.set stage_renameat2_exchange_readback_ok_msg_len, stage_renameat2_exchange_readback_ok_msg_len - 1
.set stage_eventfd_epoll_readback_ok_msg_len, stage_eventfd_epoll_readback_ok_msg_end - stage_eventfd_epoll_readback_ok_msg
.set stage_eventfd_epoll_readback_ok_msg_len, stage_eventfd_epoll_readback_ok_msg_len - 1
.set stage_scm_rights_parent_readback_ok_msg_len, stage_scm_rights_parent_readback_ok_msg_end - stage_scm_rights_parent_readback_ok_msg
.set stage_scm_rights_parent_readback_ok_msg_len, stage_scm_rights_parent_readback_ok_msg_len - 1
.set stage_copy_file_range_readback_ok_msg_len, stage_copy_file_range_readback_ok_msg_end - stage_copy_file_range_readback_ok_msg
.set stage_copy_file_range_readback_ok_msg_len, stage_copy_file_range_readback_ok_msg_len - 1
.set stage_splice_pipe_file_readback_ok_msg_len, stage_splice_pipe_file_readback_ok_msg_end - stage_splice_pipe_file_readback_ok_msg
.set stage_splice_pipe_file_readback_ok_msg_len, stage_splice_pipe_file_readback_ok_msg_len - 1
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
.set fourth_stage_open_fd_cloexec_smoke_fail_msg_len, fourth_stage_open_fd_cloexec_smoke_fail_msg_end - fourth_stage_open_fd_cloexec_smoke_fail_msg
.set fourth_stage_open_fd_cloexec_smoke_fail_msg_len, fourth_stage_open_fd_cloexec_smoke_fail_msg_len - 1
.set fourth_stage_execveat_self_fd_smoke_fail_msg_len, fourth_stage_execveat_self_fd_smoke_fail_msg_end - fourth_stage_execveat_self_fd_smoke_fail_msg
.set fourth_stage_execveat_self_fd_smoke_fail_msg_len, fourth_stage_execveat_self_fd_smoke_fail_msg_len - 1
.set fourth_stage_open_fd_mmap_smoke_fail_msg_len, fourth_stage_open_fd_mmap_smoke_fail_msg_end - fourth_stage_open_fd_mmap_smoke_fail_msg
.set fourth_stage_open_fd_mmap_smoke_fail_msg_len, fourth_stage_open_fd_mmap_smoke_fail_msg_len - 1
.set fourth_stage_pipe2_smoke_fail_msg_len, fourth_stage_pipe2_smoke_fail_msg_end - fourth_stage_pipe2_smoke_fail_msg
.set fourth_stage_pipe2_smoke_fail_msg_len, fourth_stage_pipe2_smoke_fail_msg_len - 1
.set fourth_stage_futex_shared_smoke_fail_msg_len, fourth_stage_futex_shared_smoke_fail_msg_end - fourth_stage_futex_shared_smoke_fail_msg
.set fourth_stage_futex_shared_smoke_fail_msg_len, fourth_stage_futex_shared_smoke_fail_msg_len - 1
.set fourth_stage_socketpair_smoke_fail_msg_len, fourth_stage_socketpair_smoke_fail_msg_end - fourth_stage_socketpair_smoke_fail_msg
.set fourth_stage_socketpair_smoke_fail_msg_len, fourth_stage_socketpair_smoke_fail_msg_len - 1
.set fourth_stage_openat2_smoke_fail_msg_len, fourth_stage_openat2_smoke_fail_msg_end - fourth_stage_openat2_smoke_fail_msg
.set fourth_stage_openat2_smoke_fail_msg_len, fourth_stage_openat2_smoke_fail_msg_len - 1
.set fourth_stage_pidfd_smoke_fail_msg_len, fourth_stage_pidfd_smoke_fail_msg_end - fourth_stage_pidfd_smoke_fail_msg
.set fourth_stage_pidfd_smoke_fail_msg_len, fourth_stage_pidfd_smoke_fail_msg_len - 1
.set fourth_stage_signalfd_smoke_fail_msg_len, fourth_stage_signalfd_smoke_fail_msg_end - fourth_stage_signalfd_smoke_fail_msg
.set fourth_stage_signalfd_smoke_fail_msg_len, fourth_stage_signalfd_smoke_fail_msg_len - 1
.set fourth_stage_renameat2_smoke_fail_msg_len, fourth_stage_renameat2_smoke_fail_msg_end - fourth_stage_renameat2_smoke_fail_msg
.set fourth_stage_renameat2_smoke_fail_msg_len, fourth_stage_renameat2_smoke_fail_msg_len - 1
.set fourth_stage_eventfd_epoll_smoke_fail_msg_len, fourth_stage_eventfd_epoll_smoke_fail_msg_end - fourth_stage_eventfd_epoll_smoke_fail_msg
.set fourth_stage_eventfd_epoll_smoke_fail_msg_len, fourth_stage_eventfd_epoll_smoke_fail_msg_len - 1
.set fourth_stage_scm_rights_smoke_fail_msg_len, fourth_stage_scm_rights_smoke_fail_msg_end - fourth_stage_scm_rights_smoke_fail_msg
.set fourth_stage_scm_rights_smoke_fail_msg_len, fourth_stage_scm_rights_smoke_fail_msg_len - 1
.set fourth_stage_copy_file_range_smoke_fail_msg_len, fourth_stage_copy_file_range_smoke_fail_msg_end - fourth_stage_copy_file_range_smoke_fail_msg
.set fourth_stage_copy_file_range_smoke_fail_msg_len, fourth_stage_copy_file_range_smoke_fail_msg_len - 1
.set fourth_stage_splice_smoke_fail_msg_len, fourth_stage_splice_smoke_fail_msg_end - fourth_stage_splice_smoke_fail_msg
.set fourth_stage_splice_smoke_fail_msg_len, fourth_stage_splice_smoke_fail_msg_len - 1
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
execve_cloexec_fd_argv:
    .dword execve_open_fd_self_path
    .dword execve_cloexec_fd_mode_arg
    .dword execve_open_fd_fd_arg
    .dword 0
execveat_self_fd_argv:
    .dword execve_open_fd_self_path
    .dword execveat_self_fd_mode_arg
    .dword execve_open_fd_fd_arg
    .dword 0
fourth_stage_openat2_self_how:
    .dword O_RDONLY
    .dword 0
    .dword RESOLVE_NO_SYMLINKS | RESOLVE_BENEATH
fourth_stage_openat2_self_how_end:
fourth_stage_sigchld_rt_mask:
    .dword 65536
execve_open_fd_envp:
    .dword 0
empty_path:
    .byte 0
proc_self_fd_path:
    .ascii "/proc/self/fd/"
proc_self_fd_digit:
    .byte '0'
proc_self_fd_path_end:
    .byte 0
execve_open_fd_fd_arg:
    .byte '0'
    .byte 0
.set fourth_stage_openat2_self_how_size, fourth_stage_openat2_self_how_end - fourth_stage_openat2_self_how

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
fourth_stage_statx_buf:
    .space fourth_stage_statx_buf_len
    .balign 8
fourth_stage_timerfd_spec:
    .space fourth_stage_timerfd_spec_len
    .balign 8
fourth_stage_wait_status:
    .word 0
    .balign 4
fourth_stage_pipe_fds:
    .space 8
fourth_stage_socketpair_fds:
    .space 8
fourth_stage_pollfd:
    .space fourth_stage_pollfd_len
fourth_stage_epoll_event:
    .space fourth_stage_epoll_event_len
fourth_stage_siginfo_buf:
    .space fourth_stage_siginfo_buf_len
fourth_stage_eventfd_counter:
    .space fourth_stage_eventfd_counter_len
    .balign 8
fourth_stage_scm_iovec:
    .space fourth_stage_scm_iovec_len
fourth_stage_scm_msghdr:
    .space fourth_stage_scm_msghdr_len
fourth_stage_scm_cmsg_buf:
    .space fourth_stage_scm_cmsg_buf_len
fourth_stage_scm_data_byte:
    .space fourth_stage_scm_data_len

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
    beqz a0, open_fd_execve_mode_matched
    addi t1, sp, 8
    ld t2, 8(t1)
    mv a0, t2
    la a1, execve_cloexec_fd_mode_arg
    jal ra, compare_cstr
    beqz a0, open_fd_cloexec_mode_matched
    addi t1, sp, 8
    ld t2, 8(t1)
    mv a0, t2
    la a1, execveat_self_fd_mode_arg
    jal ra, compare_cstr
    bnez a0, fourth_stage_default_start
    li t4, 2
    j open_fd_execve_mode_parse

open_fd_cloexec_mode_matched:
    li t4, 1
    j open_fd_execve_mode_parse

open_fd_execve_mode_matched:
    li t4, 0

open_fd_execve_mode_parse:
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
    beqz t4, open_fd_execve_child_entry
    li t5, 1
    beq t4, t5, open_fd_cloexec_child_entry
    j execveat_self_fd_child_entry

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
    li a1, F_GETFD
    li a2, 0
    li a7, SYS_FCNTL
    ecall
    bnez a0, open_fd_cloexec_close_error

    mv a0, s4
    li a1, F_SETFD
    li a2, FD_CLOEXEC
    li a7, SYS_FCNTL
    ecall
    bltz a0, open_fd_cloexec_close_error

    mv a0, s4
    li a1, F_GETFD
    li a2, 0
    li a7, SYS_FCNTL
    ecall
    li t1, FD_CLOEXEC
    bne a0, t1, open_fd_cloexec_close_error

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, open_fd_cloexec_close_error
    beqz a0, open_fd_cloexec_child_exec
    mv t0, a0

    la t1, fourth_stage_wait_status
    sw zero, 0(t1)
    mv a0, t0
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, open_fd_cloexec_close_error
    bne a0, t0, open_fd_cloexec_close_error
    lw t2, 0(t1)
    bnez t2, open_fd_cloexec_close_error

    mv a0, s4
    li a1, F_GETFD
    li a2, 0
    li a7, SYS_FCNTL
    ecall
    li t1, FD_CLOEXEC
    bne a0, t1, open_fd_cloexec_close_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_cloexec_close_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_cloexec_close_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_execve_expected_len
    bne t1, t2, open_fd_cloexec_close_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_cloexec_close_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_cloexec_close_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_execve_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_execve_expected_len
    bne a0, t1, open_fd_cloexec_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_execve_expected
    li a2, post_init_data_execve_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_cloexec_close_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_cloexec_parent_readback_ok_msg
    li a2, stage_unlinkat_open_fd_cloexec_parent_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, AT_FDCWD
    la a1, execve_open_fd_self_path
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, execveat_self_fd_close_error
    mv s3, a0

    li t0, 9
    blt t0, s3, execveat_self_fd_primary_close_error
    addi t0, s3, '0'
    la t1, execve_open_fd_fd_arg
    sb t0, 0(t1)

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, execveat_self_fd_primary_close_error
    beqz a0, execveat_self_fd_child_exec
    mv t0, a0

    la t1, fourth_stage_wait_status
    sw zero, 0(t1)
    mv a0, t0
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, execveat_self_fd_primary_close_error
    bne a0, t0, execveat_self_fd_primary_close_error
    lw t2, 0(t1)
    bnez t2, execveat_self_fd_primary_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, execveat_self_fd_close_error

    mv a0, s0
    la a1, stage_execveat_self_fd_parent_wait_ok_msg
    li a2, stage_execveat_self_fd_parent_wait_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, 0
    li a1, post_init_data_mmap_expected_len
    li a2, PROT_READ_WRITE
    li a3, MAP_SHARED
    mv a4, s4
    li a5, 0
    li a7, SYS_MMAP
    ecall
    bltz a0, open_fd_mmap_close_error
    mv s5, a0

    mv a0, s5
    la a1, post_init_data_execve_expected
    li a2, post_init_data_execve_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_mmap_unmap_error

    mv a0, s5
    la a1, post_init_data_mmap_expected
    li a2, post_init_data_mmap_expected_len
    jal ra, copy_bytes

    mv a0, s5
    la a1, post_init_data_mmap_expected
    li a2, post_init_data_mmap_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_mmap_unmap_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_mmap_unmap_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_mmap_unmap_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_mmap_expected_len
    bne t1, t2, open_fd_mmap_unmap_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_mmap_unmap_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_mmap_shared_write_ok_msg
    li a2, stage_unlinkat_open_fd_mmap_shared_write_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_mmap_unmap_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_mmap_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_mmap_expected_len
    bne a0, t1, open_fd_mmap_unmap_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_mmap_expected
    li a2, post_init_data_mmap_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_mmap_unmap_error

    mv a0, s5
    li a1, post_init_data_mmap_expected_len
    li a7, SYS_MUNMAP
    ecall
    bltz a0, open_fd_mmap_close_error
    mv s5, zero

    mv a0, s0
    la a1, stage_unlinkat_open_fd_mmap_shared_readback_ok_msg
    li a2, stage_unlinkat_open_fd_mmap_shared_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, 0
    li a1, post_init_data_mmap_private_expected_len
    li a2, PROT_READ_WRITE
    li a3, MAP_PRIVATE
    mv a4, s4
    li a5, 0
    li a7, SYS_MMAP
    ecall
    bltz a0, open_fd_mmap_private_close_error
    mv s5, a0

    mv a0, s5
    la a1, post_init_data_mmap_expected
    li a2, post_init_data_mmap_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_mmap_private_unmap_error

    mv a0, s5
    la a1, post_init_data_mmap_private_expected
    li a2, post_init_data_mmap_private_expected_len
    jal ra, copy_bytes

    mv a0, s5
    la a1, post_init_data_mmap_private_expected
    li a2, post_init_data_mmap_private_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_mmap_private_unmap_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_mmap_private_unmap_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_mmap_private_unmap_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_mmap_private_expected_len
    bne t1, t2, open_fd_mmap_private_unmap_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_mmap_private_unmap_error

    mv a0, s0
    la a1, stage_unlinkat_open_fd_mmap_private_cow_write_ok_msg
    li a2, stage_unlinkat_open_fd_mmap_private_cow_write_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_mmap_private_unmap_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_mmap_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_mmap_expected_len
    bne a0, t1, open_fd_mmap_private_unmap_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_mmap_expected
    li a2, post_init_data_mmap_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_mmap_private_unmap_error

    mv a0, s5
    li a1, post_init_data_mmap_private_expected_len
    li a7, SYS_MUNMAP
    ecall
    bltz a0, open_fd_mmap_private_close_error
    mv s5, zero

    mv a0, s0
    la a1, stage_unlinkat_open_fd_mmap_private_fd_unchanged_ok_msg
    la a2, stage_unlinkat_open_fd_mmap_private_fd_unchanged_ok_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, 0
    li a1, post_init_data_mmap_shared_fork_expected_len
    li a2, PROT_READ_WRITE
    li a3, MAP_SHARED
    mv a4, s4
    li a5, 0
    li a7, SYS_MMAP
    ecall
    bltz a0, open_fd_mmap_fork_close_error
    mv s5, a0

    mv a0, s5
    la a1, post_init_data_mmap_expected
    li a2, post_init_data_mmap_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_mmap_fork_unmap_error

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, open_fd_mmap_fork_unmap_error
    beqz a0, open_fd_mmap_fork_child
    mv t0, a0

    la t1, fourth_stage_wait_status
    sw zero, 0(t1)
    mv a0, t0
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, open_fd_mmap_fork_unmap_error
    bne a0, t0, open_fd_mmap_fork_unmap_error
    lw t2, 0(t1)
    bnez t2, open_fd_mmap_fork_unmap_error

    mv a0, s5
    la a1, post_init_data_mmap_shared_fork_expected
    li a2, post_init_data_mmap_shared_fork_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_mmap_fork_unmap_error

    mv a0, s4
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, open_fd_mmap_fork_unmap_error

    la t0, fourth_stage_stat_buf_alt
    lw t1, fourth_stage_stat_st_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, open_fd_mmap_fork_unmap_error
    ld t1, fourth_stage_stat_st_size_offset(t0)
    li t2, post_init_data_mmap_shared_fork_expected_len
    bne t1, t2, open_fd_mmap_fork_unmap_error
    lw t1, fourth_stage_stat_st_nlink_offset(t0)
    bnez t1, open_fd_mmap_fork_unmap_error

    mv a0, s4
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, open_fd_mmap_fork_unmap_error

    mv a0, s4
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_mmap_shared_fork_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_mmap_shared_fork_expected_len
    bne a0, t1, open_fd_mmap_fork_unmap_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_mmap_shared_fork_expected
    li a2, post_init_data_mmap_shared_fork_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_mmap_fork_unmap_error

    mv a0, s5
    li a1, post_init_data_mmap_shared_fork_expected_len
    li a7, SYS_MUNMAP
    ecall
    bltz a0, open_fd_mmap_fork_close_error
    mv s5, zero

    mv a0, s0
    la a1, stage_unlinkat_open_fd_mmap_shared_fork_parent_readback_ok_msg
    la a2, stage_unlinkat_open_fd_mmap_shared_fork_parent_readback_ok_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    la a0, fourth_stage_pipe_fds
    li a1, 0
    li a7, SYS_PIPE2
    ecall
    bltz a0, pipe2_smoke_failed

    la t0, fourth_stage_pipe_fds
    lw s3, 0(t0)
    lw s5, 4(t0)

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, pipe2_close_error
    beqz a0, pipe2_fork_child
    mv t0, a0

    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    bltz a0, pipe2_read_close_error
    li s5, -1

    la t1, fourth_stage_wait_status
    sw zero, 0(t1)
    mv a0, t0
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, pipe2_read_close_error
    bne a0, t0, pipe2_read_close_error
    lw t2, 0(t1)
    bnez t2, pipe2_read_close_error

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, pipe2_roundtrip_expected_len
    li a7, SYS_READ
    ecall
    li t1, pipe2_roundtrip_expected_len
    bne a0, t1, pipe2_read_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, pipe2_roundtrip_expected
    li a2, pipe2_roundtrip_expected_len
    jal ra, compare_bytes
    bltz a0, pipe2_read_close_error

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, 1
    li a7, SYS_READ
    ecall
    bnez a0, pipe2_read_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, pipe2_root_close_error
    li s3, -1

    mv a0, s0
    la a1, stage_pipe2_fork_parent_readback_ok_msg
    li a2, stage_pipe2_fork_parent_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, 0
    li a1, futex_shared_map_len
    li a2, PROT_READ_WRITE
    li a3, MAP_SHARED_ANONYMOUS
    li a4, -1
    li a5, 0
    li a7, SYS_MMAP
    ecall
    bltz a0, futex_shared_smoke_failed
    mv s6, a0

    sw zero, futex_shared_state_offset(s6)

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, futex_shared_unmap_error
    beqz a0, futex_shared_child
    mv t0, a0

futex_shared_parent_wait_loop:
    lw t1, futex_shared_state_offset(s6)
    li t2, 1
    beq t1, t2, futex_shared_parent_wait_done

    mv a0, s6
    li a1, FUTEX_WAIT
    li a2, 0
    li a3, 0
    li a4, 0
    li a5, 0
    li a7, SYS_FUTEX
    ecall
    beqz a0, futex_shared_parent_wait_loop
    li t3, -EAGAIN
    beq a0, t3, futex_shared_parent_wait_loop
    bltz a0, futex_shared_unmap_error
    j futex_shared_unmap_error

futex_shared_parent_wait_done:
    fence rw, rw

    la t1, fourth_stage_wait_status
    sw zero, 0(t1)
    mv a0, t0
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, futex_shared_unmap_error
    bne a0, t0, futex_shared_unmap_error
    lw t2, 0(t1)
    bnez t2, futex_shared_unmap_error

    addi a0, s6, futex_shared_data_offset
    la a1, futex_shared_expected
    li a2, futex_shared_expected_len
    jal ra, compare_bytes
    bltz a0, futex_shared_unmap_error

    mv a0, s6
    li a1, futex_shared_map_len
    li a7, SYS_MUNMAP
    ecall
    bltz a0, futex_shared_root_close_error
    li s6, 0

    mv a0, s0
    la a1, stage_futex_shared_parent_readback_ok_msg
    la a2, stage_futex_shared_parent_readback_ok_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, AF_UNIX
    li a1, SOCK_STREAM
    li a2, 0
    la a3, fourth_stage_socketpair_fds
    li a7, SYS_SOCKETPAIR
    ecall
    bltz a0, socketpair_smoke_failed

    la t0, fourth_stage_socketpair_fds
    lw s3, 0(t0)
    lw s5, 4(t0)

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, socketpair_close_error
    beqz a0, socketpair_fork_child
    mv t0, a0

    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    bltz a0, socketpair_read_close_error
    li s5, -1

    la t1, fourth_stage_wait_status
    sw zero, 0(t1)
    mv a0, t0
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, socketpair_read_close_error
    bne a0, t0, socketpair_read_close_error
    lw t2, 0(t1)
    bnez t2, socketpair_read_close_error

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, socketpair_roundtrip_expected_len
    li a7, SYS_READ
    ecall
    li t1, socketpair_roundtrip_expected_len
    bne a0, t1, socketpair_read_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, socketpair_roundtrip_expected
    li a2, socketpair_roundtrip_expected_len
    jal ra, compare_bytes
    bltz a0, socketpair_read_close_error

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, 1
    li a7, SYS_READ
    ecall
    bnez a0, socketpair_read_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, socketpair_root_close_error
    li s3, -1

    mv a0, s0
    la a1, stage_socketpair_fork_parent_readback_ok_msg
    la a2, stage_socketpair_fork_parent_readback_ok_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s2
    la a1, execve_open_fd_self_name
    la a2, fourth_stage_openat2_self_how
    li a3, fourth_stage_openat2_self_how_size
    li a7, SYS_OPENAT2
    ecall
    bltz a0, openat2_smoke_failed
    mv s3, a0

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, self_elf_magic_expected_len
    li a7, SYS_READ
    ecall
    li t1, self_elf_magic_expected_len
    bne a0, t1, openat2_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, self_elf_magic_expected
    li a2, self_elf_magic_expected_len
    jal ra, compare_bytes
    bltz a0, openat2_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, openat2_root_close_error
    li s3, -1

    mv a0, s0
    la a1, stage_openat2_self_beneath_readback_ok_msg
    li a2, stage_openat2_self_beneath_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, pidfd_smoke_failed
    beqz a0, pidfd_waitid_child
    mv s5, a0

    mv a0, s5
    li a1, 0
    li a7, SYS_PIDFD_OPEN
    ecall
    bltz a0, pidfd_smoke_failed
    mv s3, a0

    la t0, fourth_stage_pollfd
    sw s3, fourth_stage_pollfd_fd_offset(t0)
    li t1, POLLIN
    sh t1, fourth_stage_pollfd_events_offset(t0)
    sh zero, fourth_stage_pollfd_revents_offset(t0)

    la a0, fourth_stage_pollfd
    li a1, 1
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, SYS_PPOLL
    ecall
    li t1, 1
    bne a0, t1, pidfd_close_error

    la t0, fourth_stage_pollfd
    lhu t1, fourth_stage_pollfd_revents_offset(t0)
    li t2, POLLIN
    and t1, t1, t2
    beqz t1, pidfd_close_error

    li a0, P_PIDFD
    mv a1, s3
    la a2, fourth_stage_siginfo_buf
    li a3, WEXITED
    li a4, 0
    li a7, SYS_WAITID
    ecall
    bltz a0, pidfd_close_error

    la t0, fourth_stage_siginfo_buf
    lw t1, fourth_stage_siginfo_signo_offset(t0)
    li t2, SIGCHLD
    bne t1, t2, pidfd_close_error
    lw t1, fourth_stage_siginfo_code_offset(t0)
    li t2, CLD_EXITED
    bne t1, t2, pidfd_close_error
    lw t1, fourth_stage_siginfo_pid_offset(t0)
    bne t1, s5, pidfd_close_error
    lw t1, fourth_stage_siginfo_status_offset(t0)
    bnez t1, pidfd_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, pidfd_root_close_error
    li s3, -1

    mv a0, s0
    la a1, stage_pidfd_open_ppoll_waitid_ok_msg
    li a2, stage_pidfd_open_ppoll_waitid_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, SIG_BLOCK
    la a1, fourth_stage_sigchld_rt_mask
    li a2, 0
    li a3, fourth_stage_rt_sigset_len
    li a7, SYS_RT_SIGPROCMASK
    ecall
    bltz a0, signalfd_smoke_failed

    li a0, -1
    la a1, fourth_stage_sigchld_rt_mask
    li a2, fourth_stage_rt_sigset_len
    li a3, 0
    li a7, SYS_SIGNALFD4
    ecall
    bltz a0, signalfd_smoke_failed
    mv s3, a0

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, signalfd_close_error
    beqz a0, signalfd_sigchld_child
    mv s5, a0

    mv a0, s3
    la a1, fourth_stage_siginfo_buf
    li a2, fourth_stage_signalfd_siginfo_len
    li a7, SYS_READ
    ecall
    li t1, fourth_stage_signalfd_siginfo_len
    bne a0, t1, signalfd_close_error

    la t0, fourth_stage_siginfo_buf
    lw t1, fourth_stage_signalfd_ssi_signo_offset(t0)
    li t2, SIGCHLD
    bne t1, t2, signalfd_close_error
    lw t1, fourth_stage_signalfd_ssi_code_offset(t0)
    li t2, CLD_EXITED
    bne t1, t2, signalfd_close_error
    lw t1, fourth_stage_signalfd_ssi_pid_offset(t0)
    bne t1, s5, signalfd_close_error
    lw t1, fourth_stage_signalfd_ssi_status_offset(t0)
    bnez t1, signalfd_close_error

    la t1, fourth_stage_wait_status
    sw zero, 0(t1)
    mv a0, s5
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, signalfd_close_error
    bne a0, s5, signalfd_close_error
    lw t2, 0(t1)
    bnez t2, signalfd_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, signalfd_root_close_error
    li s3, -1

    mv a0, s0
    la a1, stage_signalfd_sigchld_readback_ok_msg
    li a2, stage_signalfd_sigchld_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s2
    la a1, renameat2_src_name
    li a2, O_RDWR | O_CREAT | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, renameat2_smoke_failed
    mv s3, a0

    mv a0, s3
    la a1, renameat2_src_expected
    li a2, renameat2_src_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, renameat2_src_expected_len
    bne a0, t1, renameat2_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, renameat2_smoke_failed
    li s3, -1

    mv a0, s2
    la a1, renameat2_dst_name
    li a2, O_RDWR | O_CREAT | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, renameat2_smoke_failed
    mv s3, a0

    mv a0, s3
    la a1, renameat2_dst_expected
    li a2, renameat2_dst_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, renameat2_dst_expected_len
    bne a0, t1, renameat2_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, renameat2_smoke_failed
    li s3, -1

    mv a0, s2
    la a1, renameat2_src_name
    mv a2, s2
    la a3, renameat2_dst_name
    li a4, RENAME_EXCHANGE
    li a7, SYS_RENAMEAT2
    ecall
    bltz a0, renameat2_smoke_failed

    mv a0, s2
    la a1, renameat2_src_name
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, renameat2_smoke_failed
    mv s3, a0

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, renameat2_dst_expected_len
    li a7, SYS_READ
    ecall
    li t1, renameat2_dst_expected_len
    bne a0, t1, renameat2_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, renameat2_dst_expected
    li a2, renameat2_dst_expected_len
    jal ra, compare_bytes
    bltz a0, renameat2_close_error

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, 1
    li a7, SYS_READ
    ecall
    bnez a0, renameat2_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, renameat2_smoke_failed
    li s3, -1

    mv a0, s2
    la a1, renameat2_dst_name
    li a2, O_RDONLY
    li a3, 0
    li a7, SYS_OPENAT
    ecall
    bltz a0, renameat2_smoke_failed
    mv s3, a0

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, renameat2_src_expected_len
    li a7, SYS_READ
    ecall
    li t1, renameat2_src_expected_len
    bne a0, t1, renameat2_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, renameat2_src_expected
    li a2, renameat2_src_expected_len
    jal ra, compare_bytes
    bltz a0, renameat2_close_error

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, 1
    li a7, SYS_READ
    ecall
    bnez a0, renameat2_close_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, renameat2_smoke_failed
    li s3, -1

    mv a0, s2
    la a1, renameat2_src_name
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, renameat2_smoke_failed

    mv a0, s2
    la a1, renameat2_dst_name
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, renameat2_smoke_failed

    mv a0, s0
    la a1, stage_renameat2_exchange_readback_ok_msg
    li a2, stage_renameat2_exchange_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li a0, 0
    li a1, 0
    li a7, SYS_EVENTFD2
    ecall
    bltz a0, eventfd_epoll_smoke_failed
    mv s3, a0

    li a0, 0
    li a7, SYS_EPOLL_CREATE1
    ecall
    bltz a0, eventfd_epoll_close_error
    mv s5, a0

    la t0, fourth_stage_epoll_event
    li t1, POLLIN
    sw t1, fourth_stage_epoll_event_events_offset(t0)
    sd s3, fourth_stage_epoll_event_data_offset(t0)

    mv a0, s5
    li a1, EPOLL_CTL_ADD
    mv a2, s3
    la a3, fourth_stage_epoll_event
    li a7, SYS_EPOLL_CTL
    ecall
    bltz a0, eventfd_epoll_root_close_error

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, eventfd_epoll_root_close_error
    beqz a0, eventfd_epoll_child
    mv t0, a0

    mv a0, s5
    la a1, fourth_stage_epoll_event
    li a2, 1
    li a3, -1
    li a4, 0
    li a5, 0
    li a7, SYS_EPOLL_PWAIT
    ecall
    li t1, 1
    bne a0, t1, eventfd_epoll_root_close_error

    la t1, fourth_stage_epoll_event
    lw t2, fourth_stage_epoll_event_events_offset(t1)
    li t3, POLLIN
    and t2, t2, t3
    beqz t2, eventfd_epoll_root_close_error
    ld t2, fourth_stage_epoll_event_data_offset(t1)
    bne t2, s3, eventfd_epoll_root_close_error

    mv a0, s3
    la a1, fourth_stage_eventfd_counter
    li a2, fourth_stage_eventfd_counter_len
    li a7, SYS_READ
    ecall
    li t1, fourth_stage_eventfd_counter_len
    bne a0, t1, eventfd_epoll_root_close_error

    la t3, fourth_stage_eventfd_counter
    ld t1, 0(t3)
    li t2, 1
    bne t1, t2, eventfd_epoll_root_close_error

    la t1, fourth_stage_wait_status
    sw zero, 0(t1)
    mv a0, t0
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, eventfd_epoll_root_close_error
    bne a0, t0, eventfd_epoll_root_close_error
    lw t2, 0(t1)
    bnez t2, eventfd_epoll_root_close_error

    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    bltz a0, eventfd_epoll_close_error
    li s5, -1

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, eventfd_epoll_smoke_failed
    li s3, -1

    mv a0, s0
    la a1, stage_eventfd_epoll_readback_ok_msg
    li a2, stage_eventfd_epoll_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li s6, -1

    li a0, AF_UNIX
    li a1, SOCK_STREAM
    li a2, 0
    la a3, fourth_stage_socketpair_fds
    li a7, SYS_SOCKETPAIR
    ecall
    bltz a0, scm_rights_smoke_failed

    la t1, fourth_stage_socketpair_fds
    lw s3, 0(t1)
    lw s5, 4(t1)

    la a0, clone3_args
    li a1, clone3_args_size
    li a7, SYS_CLONE3
    ecall
    bltz a0, scm_rights_cleanup_error
    beqz a0, scm_rights_child
    mv t0, a0

    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    bltz a0, scm_rights_cleanup_error
    li s5, -1

    la t1, fourth_stage_scm_data_byte
    sb zero, 0(t1)

    la t1, fourth_stage_scm_iovec
    la t2, fourth_stage_scm_data_byte
    sd t2, fourth_stage_scm_iovec_base_offset(t1)
    li t2, fourth_stage_scm_data_len
    sd t2, fourth_stage_scm_iovec_len_offset(t1)

    la t1, fourth_stage_scm_cmsg_buf
    sd zero, 0(t1)
    sd zero, 8(t1)
    sd zero, 16(t1)

    la t1, fourth_stage_scm_msghdr
    sd zero, fourth_stage_scm_msghdr_name_offset(t1)
    sw zero, fourth_stage_scm_msghdr_namelen_offset(t1)
    la t2, fourth_stage_scm_iovec
    sd t2, fourth_stage_scm_msghdr_iov_offset(t1)
    li t2, 1
    sd t2, fourth_stage_scm_msghdr_iovlen_offset(t1)
    la t2, fourth_stage_scm_cmsg_buf
    sd t2, fourth_stage_scm_msghdr_control_offset(t1)
    li t2, fourth_stage_scm_cmsg_buf_len
    sd t2, fourth_stage_scm_msghdr_controllen_offset(t1)
    sw zero, fourth_stage_scm_msghdr_flags_offset(t1)

    mv a0, s3
    la a1, fourth_stage_scm_msghdr
    li a2, 0
    li a7, SYS_RECVMSG
    ecall
    li t1, fourth_stage_scm_data_len
    bne a0, t1, scm_rights_cleanup_error

    la t1, fourth_stage_scm_data_byte
    lbu t2, 0(t1)
    li t3, 'R'
    bne t2, t3, scm_rights_cleanup_error

    la t1, fourth_stage_scm_cmsg_buf
    ld t2, fourth_stage_scm_cmsg_len_offset(t1)
    li t3, fourth_stage_scm_cmsg_fd_len
    bne t2, t3, scm_rights_cleanup_error
    lw t2, fourth_stage_scm_cmsg_level_offset(t1)
    li t3, SOL_SOCKET
    bne t2, t3, scm_rights_cleanup_error
    lw t2, fourth_stage_scm_cmsg_type_offset(t1)
    li t3, SCM_RIGHTS
    bne t2, t3, scm_rights_cleanup_error
    lw s6, fourth_stage_scm_cmsg_data_offset(t1)
    bltz s6, scm_rights_cleanup_error

    la t1, fourth_stage_wait_status
    sw zero, 0(t1)
    mv a0, t0
    mv a1, t1
    li a2, 0
    li a3, 0
    li a7, SYS_WAIT4
    ecall
    bltz a0, scm_rights_cleanup_error
    bne a0, t0, scm_rights_cleanup_error
    lw t2, 0(t1)
    bnez t2, scm_rights_cleanup_error

    mv a0, s6
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, scm_rights_cleanup_error

    la t1, fourth_stage_stat_buf_alt
    lw t2, fourth_stage_stat_st_mode_offset(t1)
    li t3, fourth_stage_stat_mode_mask
    and t2, t2, t3
    li t3, fourth_stage_stat_mode_regular
    bne t2, t3, scm_rights_cleanup_error
    ld t2, fourth_stage_stat_st_size_offset(t1)
    li t3, post_init_data_mmap_shared_fork_expected_len
    bne t2, t3, scm_rights_cleanup_error
    lw t2, fourth_stage_stat_st_nlink_offset(t1)
    bnez t2, scm_rights_cleanup_error

    mv a0, s6
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, scm_rights_cleanup_error

    mv a0, s6
    la a1, fourth_stage_post_init_data_buf
    li a2, post_init_data_mmap_shared_fork_expected_len
    li a7, SYS_READ
    ecall
    li t1, post_init_data_mmap_shared_fork_expected_len
    bne a0, t1, scm_rights_cleanup_error

    la a0, fourth_stage_post_init_data_buf
    la a1, post_init_data_mmap_shared_fork_expected
    li a2, post_init_data_mmap_shared_fork_expected_len
    jal ra, compare_bytes
    bltz a0, scm_rights_cleanup_error

    mv a0, s6
    la a1, fourth_stage_post_init_data_buf
    li a2, 1
    li a7, SYS_READ
    ecall
    bnez a0, scm_rights_cleanup_error

    mv a0, s6
    li a7, SYS_CLOSE
    ecall
    bltz a0, scm_rights_cleanup_error
    li s6, -1

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, scm_rights_smoke_failed
    li s3, -1

    mv a0, s0
    la a1, stage_scm_rights_parent_readback_ok_msg
    li a2, stage_scm_rights_parent_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s2
    la a1, copy_file_range_src_name
    li a2, O_RDWR | O_CREAT | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, copy_file_range_smoke_failed
    mv s3, a0

    mv a0, s2
    la a1, copy_file_range_dst_name
    li a2, O_RDWR | O_CREAT | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, copy_file_range_close_error
    mv s5, a0

    mv a0, s3
    la a1, copy_file_range_expected
    li a2, copy_file_range_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, copy_file_range_expected_len
    bne a0, t1, copy_file_range_close_error

    mv a0, s3
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, copy_file_range_close_error

    mv a0, s3
    li a1, 0
    mv a2, s5
    li a3, 0
    li a4, copy_file_range_expected_len
    li a5, 0
    li a7, SYS_COPY_FILE_RANGE
    ecall
    li t1, copy_file_range_expected_len
    bne a0, t1, copy_file_range_close_error

    mv a0, s5
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, copy_file_range_close_error

    la t1, fourth_stage_stat_buf_alt
    lw t2, fourth_stage_stat_st_mode_offset(t1)
    li t3, fourth_stage_stat_mode_mask
    and t2, t2, t3
    li t3, fourth_stage_stat_mode_regular
    bne t2, t3, copy_file_range_close_error
    ld t2, fourth_stage_stat_st_size_offset(t1)
    li t3, copy_file_range_expected_len
    bne t2, t3, copy_file_range_close_error

    mv a0, s5
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, copy_file_range_close_error

    mv a0, s5
    la a1, fourth_stage_post_init_data_buf
    li a2, copy_file_range_expected_len
    li a7, SYS_READ
    ecall
    li t1, copy_file_range_expected_len
    bne a0, t1, copy_file_range_close_error

    la a0, fourth_stage_post_init_data_buf
    la a1, copy_file_range_expected
    li a2, copy_file_range_expected_len
    jal ra, compare_bytes
    bltz a0, copy_file_range_close_error

    mv a0, s5
    la a1, fourth_stage_post_init_data_buf
    li a2, 1
    li a7, SYS_READ
    ecall
    bnez a0, copy_file_range_close_error

    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    bltz a0, copy_file_range_cleanup_error
    li s5, -1

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, copy_file_range_cleanup_error
    li s3, -1

    mv a0, s2
    la a1, copy_file_range_src_name
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, copy_file_range_smoke_failed

    mv a0, s2
    la a1, copy_file_range_dst_name
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, copy_file_range_smoke_failed

    mv a0, s0
    la a1, stage_copy_file_range_readback_ok_msg
    li a2, stage_copy_file_range_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li s3, -1
    li s5, -1
    li s6, -1

    la t0, fourth_stage_pipe_fds
    li t1, -1
    sw t1, 0(t0)
    sw t1, 4(t0)

    mv a0, s2
    la a1, splice_src_name
    li a2, O_RDWR | O_CREAT | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, splice_smoke_failed
    mv s3, a0

    mv a0, s2
    la a1, splice_dst_name
    li a2, O_RDWR | O_CREAT | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, splice_cleanup_error
    mv s5, a0

    mv a0, s3
    la a1, splice_expected
    li a2, splice_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, splice_expected_len
    bne a0, t1, splice_cleanup_error

    mv a0, s3
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, splice_cleanup_error

    la a0, fourth_stage_pipe_fds
    li a1, 0
    li a7, SYS_PIPE2
    ecall
    bltz a0, splice_cleanup_error

    la t0, fourth_stage_pipe_fds
    lw s6, 0(t0)
    lw t2, 4(t0)

    mv a0, s3
    li a1, 0
    mv a2, t2
    li a3, 0
    li a4, splice_expected_len
    li a5, 0
    li a7, SYS_SPLICE
    ecall
    li t1, splice_expected_len
    bne a0, t1, splice_cleanup_error

    mv a0, t2
    li a7, SYS_CLOSE
    ecall
    bltz a0, splice_cleanup_error
    la t0, fourth_stage_pipe_fds
    li t1, -1
    sw t1, 4(t0)

    mv a0, s6
    li a1, 0
    mv a2, s5
    li a3, 0
    li a4, splice_expected_len
    li a5, 0
    li a7, SYS_SPLICE
    ecall
    li t1, splice_expected_len
    bne a0, t1, splice_cleanup_error

    mv a0, s6
    li a7, SYS_CLOSE
    ecall
    bltz a0, splice_cleanup_error
    li s6, -1
    la t0, fourth_stage_pipe_fds
    li t1, -1
    sw t1, 0(t0)

    mv a0, s5
    la a1, fourth_stage_stat_buf_alt
    li a7, SYS_FSTAT
    ecall
    bltz a0, splice_cleanup_error

    la t1, fourth_stage_stat_buf_alt
    lw t2, fourth_stage_stat_st_mode_offset(t1)
    li t3, fourth_stage_stat_mode_mask
    and t2, t2, t3
    li t3, fourth_stage_stat_mode_regular
    bne t2, t3, splice_cleanup_error
    ld t2, fourth_stage_stat_st_size_offset(t1)
    li t3, splice_expected_len
    bne t2, t3, splice_cleanup_error

    mv a0, s5
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, splice_cleanup_error

    mv a0, s5
    la a1, fourth_stage_post_init_data_buf
    li a2, splice_expected_len
    li a7, SYS_READ
    ecall
    li t1, splice_expected_len
    bne a0, t1, splice_cleanup_error

    la a0, fourth_stage_post_init_data_buf
    la a1, splice_expected
    li a2, splice_expected_len
    jal ra, compare_bytes
    bltz a0, splice_cleanup_error

    mv a0, s5
    la a1, fourth_stage_post_init_data_buf
    li a2, 1
    li a7, SYS_READ
    ecall
    bnez a0, splice_cleanup_error

    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    bltz a0, splice_cleanup_error
    li s5, -1

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, splice_cleanup_error
    li s3, -1

    mv a0, s2
    la a1, splice_src_name
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, splice_smoke_failed

    mv a0, s2
    la a1, splice_dst_name
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, splice_smoke_failed

    mv a0, s0
    la a1, stage_splice_pipe_file_readback_ok_msg
    li a2, stage_splice_pipe_file_readback_ok_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li s3, -1

    mv a0, s2
    la a1, statx_name
    li a2, O_RDWR | O_CREAT | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, statx_smoke_failed
    mv s3, a0

    mv a0, s3
    la a1, statx_expected
    li a2, statx_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, statx_expected_len
    bne a0, t1, statx_cleanup_error

    mv a0, s2
    la a1, statx_name
    li a2, 0
    li a3, fourth_stage_statx_mask_basic
    la a4, fourth_stage_statx_buf
    li a7, SYS_STATX
    ecall
    bltz a0, statx_cleanup_error

    la t0, fourth_stage_statx_buf
    lw t1, fourth_stage_statx_mask_offset(t0)
    li t2, fourth_stage_statx_mask_basic
    and t1, t1, t2
    bne t1, t2, statx_cleanup_error
    lhu t1, fourth_stage_statx_mode_offset(t0)
    li t2, fourth_stage_stat_mode_mask
    and t1, t1, t2
    li t2, fourth_stage_stat_mode_regular
    bne t1, t2, statx_cleanup_error
    ld t1, fourth_stage_statx_size_offset(t0)
    li t2, statx_expected_len
    bne t1, t2, statx_cleanup_error

    mv a0, s3
    li a1, 0
    li a2, SEEK_SET
    li a7, SYS_LSEEK
    ecall
    bltz a0, statx_cleanup_error

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, statx_expected_len
    li a7, SYS_READ
    ecall
    li t1, statx_expected_len
    bne a0, t1, statx_cleanup_error

    la a0, fourth_stage_post_init_data_buf
    la a1, statx_expected
    li a2, statx_expected_len
    jal ra, compare_bytes
    bltz a0, statx_cleanup_error

    mv a0, s3
    la a1, fourth_stage_post_init_data_buf
    li a2, 1
    li a7, SYS_READ
    ecall
    bnez a0, statx_cleanup_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, statx_cleanup_error
    li s3, -1

    mv a0, s2
    la a1, statx_name
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, statx_smoke_failed

    mv a0, s0
    la a1, stage_statx_readback_ok_msg
    la a2, stage_statx_readback_ok_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li s3, -1
    li s5, -1
    li s6, -1

    mv a0, s2
    la a1, inotify_name
    li a2, O_RDWR | O_CREAT | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, inotify_smoke_failed
    mv s3, a0

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, inotify_cleanup_error
    li s3, -1

    li a0, 0
    li a7, SYS_INOTIFY_INIT1
    ecall
    bltz a0, inotify_smoke_failed
    mv s5, a0

    mv a0, s5
    la a1, inotify_name
    li a2, IN_CLOSE_WRITE
    li a7, SYS_INOTIFY_ADD_WATCH
    ecall
    bltz a0, inotify_cleanup_error
    mv s6, a0

    mv a0, s2
    la a1, inotify_name
    li a2, O_RDWR | O_TRUNC
    li a3, 420
    li a7, SYS_OPENAT
    ecall
    bltz a0, inotify_cleanup_error
    mv s3, a0

    mv a0, s3
    la a1, inotify_expected
    li a2, inotify_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, inotify_expected_len
    bne a0, t1, inotify_cleanup_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, inotify_cleanup_error
    li s3, -1

    mv a0, s5
    la a1, fourth_stage_post_init_data_buf
    li a2, fourth_stage_inotify_event_len
    li a7, SYS_READ
    ecall
    li t1, fourth_stage_inotify_event_len
    bne a0, t1, inotify_cleanup_error

    la t0, fourth_stage_post_init_data_buf
    lw t1, fourth_stage_inotify_event_wd_offset(t0)
    bne t1, s6, inotify_cleanup_error
    lw t1, fourth_stage_inotify_event_mask_offset(t0)
    li t2, IN_CLOSE_WRITE
    and t1, t1, t2
    bne t1, t2, inotify_cleanup_error
    lw t1, fourth_stage_inotify_event_name_len_offset(t0)
    bnez t1, inotify_cleanup_error

    mv a0, s5
    mv a1, s6
    li a7, SYS_INOTIFY_RM_WATCH
    ecall
    bltz a0, inotify_cleanup_error
    li s6, -1

    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    bltz a0, inotify_cleanup_error
    li s5, -1

    mv a0, s2
    la a1, inotify_name
    li a2, 0
    li a7, SYS_UNLINKAT
    ecall
    bltz a0, inotify_smoke_failed

    mv a0, s0
    la a1, stage_inotify_close_write_event_ok_msg
    la a2, stage_inotify_close_write_event_ok_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    li s3, -1

    li a0, CLOCK_MONOTONIC
    li a1, 0
    li a7, SYS_TIMERFD_CREATE
    ecall
    bltz a0, timerfd_smoke_failed
    mv s3, a0

    la t0, fourth_stage_timerfd_spec
    sd zero, fourth_stage_timerfd_interval_sec_offset(t0)
    sd zero, fourth_stage_timerfd_interval_nsec_offset(t0)
    sd zero, fourth_stage_timerfd_value_sec_offset(t0)
    li t1, fourth_stage_timerfd_one_shot_nsec
    sd t1, fourth_stage_timerfd_value_nsec_offset(t0)

    mv a0, s3
    li a1, 0
    la a2, fourth_stage_timerfd_spec
    li a3, 0
    li a7, SYS_TIMERFD_SETTIME
    ecall
    bltz a0, timerfd_cleanup_error

    mv a0, s3
    la a1, fourth_stage_eventfd_counter
    li a2, fourth_stage_eventfd_counter_len
    li a7, SYS_READ
    ecall
    li t1, fourth_stage_eventfd_counter_len
    bne a0, t1, timerfd_cleanup_error

    la a0, fourth_stage_eventfd_counter
    la a1, timerfd_expected
    li a2, fourth_stage_eventfd_counter_len
    jal ra, compare_bytes
    bltz a0, timerfd_cleanup_error

    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, timerfd_cleanup_error
    li s3, -1

    mv a0, s0
    la a1, stage_timerfd_one_shot_readback_ok_msg
    la a2, stage_timerfd_one_shot_readback_ok_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    bltz a0, open_fd_cloexec_root_close_error

    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    bltz a0, open_fd_cloexec_report_failed

    mv a0, s0
    la a1, stage_fourth_stage_reached_msg
    li a2, stage_fourth_stage_reached_msg_len
    jal ra, write_exact
    bltz a0, write_failed
    or s1, s1, a1

    mv a0, s0
    la a1, post_init_reached_msg
    la a2, post_init_reached_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
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
    la a2, fourth_stage_reused_dir_cleanup_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

reused_dir_dirent_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_reused_dir_dirent_smoke_fail_msg
    la a2, fourth_stage_reused_dir_dirent_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
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
    la a2, fourth_stage_symlink_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

symlink_stat_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_symlink_stat_smoke_fail_msg
    la a2, fourth_stage_symlink_stat_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

readlinkat_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_readlinkat_smoke_fail_msg
    la a2, fourth_stage_readlinkat_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

symlink_cleanup_failed:
    mv a0, s0
    la a1, fourth_stage_symlink_cleanup_fail_msg
    la a2, fourth_stage_symlink_cleanup_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

symlink_dirent_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_symlink_dirent_smoke_fail_msg
    la a2, fourth_stage_symlink_dirent_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
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
    la a2, fourth_stage_dirfd_openat_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
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
    la a2, fourth_stage_dirfd_fstatat_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
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
    la a2, fourth_stage_dirfd_linkat_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
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
    la a2, fourth_stage_dirfd_unlinkat_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
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
    la a2, fourth_stage_dirfd_reopen_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
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
    la a2, fourth_stage_dirfd_gone_fstatat_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
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
    la a2, fourth_stage_dirfd_getdents_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
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

open_fd_cloexec_child_exec:
    la a0, execve_open_fd_self_path
    la a1, execve_cloexec_fd_argv
    la a2, execve_open_fd_envp
    li a7, SYS_EXECVE
    ecall
    j open_fd_cloexec_report_failed

execveat_self_fd_child_exec:
    mv a0, s3
    la a1, empty_path
    la a2, execveat_self_fd_argv
    la a3, execve_open_fd_envp
    li a4, AT_EMPTY_PATH
    li a7, SYS_EXECVEAT
    ecall
    j execveat_self_fd_report_failed

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

open_fd_cloexec_child_entry:
    li s0, INHERITED_CONSOLE_FD

    mv a0, s4
    la a1, fourth_stage_stat_buf
    li a7, SYS_FSTAT
    ecall
    li t0, -EBADF
    bne a0, t0, open_fd_cloexec_report_failed

    mv a0, s0
    la a1, stage_unlinkat_open_fd_cloexec_child_closed_ok_msg
    li a2, stage_unlinkat_open_fd_cloexec_child_closed_ok_msg_len
    jal ra, write_exact
    bltz a0, open_fd_cloexec_report_failed

    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

execveat_self_fd_child_entry:
    li s0, INHERITED_CONSOLE_FD

    mv a0, s0
    la a1, stage_execveat_self_fd_child_entered_ok_msg
    li a2, stage_execveat_self_fd_child_entered_ok_msg_len
    jal ra, write_exact
    bltz a0, execveat_self_fd_report_failed

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

open_fd_cloexec_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall

open_fd_cloexec_root_close_error:
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

open_fd_cloexec_report_failed:
    mv a0, s0
    la a1, fourth_stage_open_fd_cloexec_smoke_fail_msg
    la a2, fourth_stage_open_fd_cloexec_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

execveat_self_fd_primary_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

execveat_self_fd_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

execveat_self_fd_report_failed:
    mv a0, s0
    la a1, fourth_stage_execveat_self_fd_smoke_fail_msg
    la a2, fourth_stage_execveat_self_fd_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_mmap_unmap_error:
    mv a0, s5
    li a1, post_init_data_mmap_expected_len
    li a7, SYS_MUNMAP
    ecall

open_fd_mmap_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    mv a0, s0
    la a1, fourth_stage_open_fd_mmap_smoke_fail_msg
    la a2, fourth_stage_open_fd_mmap_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_mmap_private_unmap_error:
    mv a0, s5
    li a1, post_init_data_mmap_private_expected_len
    li a7, SYS_MUNMAP
    ecall

open_fd_mmap_private_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    mv a0, s0
    la a1, fourth_stage_open_fd_mmap_private_smoke_fail_msg
    la a2, fourth_stage_open_fd_mmap_private_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

open_fd_mmap_fork_child:
    mv a0, s5
    la a1, post_init_data_mmap_shared_fork_expected
    li a2, post_init_data_mmap_shared_fork_expected_len
    jal ra, copy_bytes

    mv a0, s5
    la a1, post_init_data_mmap_shared_fork_expected
    li a2, post_init_data_mmap_shared_fork_expected_len
    jal ra, compare_bytes
    bltz a0, open_fd_mmap_fork_child_fail

    mv a0, s0
    la a1, stage_unlinkat_open_fd_mmap_shared_fork_child_write_ok_msg
    la a2, stage_unlinkat_open_fd_mmap_shared_fork_child_write_ok_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, open_fd_mmap_fork_child_fail

    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

open_fd_mmap_fork_child_fail:
    li a0, 1
    li a7, SYS_EXIT_GROUP
    ecall

open_fd_mmap_fork_unmap_error:
    mv a0, s5
    li a1, post_init_data_mmap_shared_fork_expected_len
    li a7, SYS_MUNMAP
    ecall

open_fd_mmap_fork_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall
    mv a0, s0
    la a1, fourth_stage_open_fd_mmap_fork_smoke_fail_msg
    la a2, fourth_stage_open_fd_mmap_fork_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

pipe2_fork_child:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, pipe2_fork_child_fail

    mv a0, s5
    la a1, pipe2_roundtrip_expected
    li a2, pipe2_roundtrip_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, pipe2_roundtrip_expected_len
    bne a0, t1, pipe2_fork_child_fail

    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    bltz a0, pipe2_fork_child_fail

    mv a0, s0
    la a1, stage_pipe2_fork_child_write_ok_msg
    la a2, stage_pipe2_fork_child_write_ok_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, pipe2_fork_child_fail

    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

pipe2_fork_child_fail:
    li a0, 1
    li a7, SYS_EXIT_GROUP
    ecall

futex_shared_child:
    addi a0, s6, futex_shared_data_offset
    la a1, futex_shared_expected
    li a2, futex_shared_expected_len
    jal ra, copy_bytes
    fence rw, rw

    li t0, 1
    sw t0, futex_shared_state_offset(s6)
    fence rw, rw

    mv a0, s6
    li a1, FUTEX_WAKE
    li a2, 1
    li a3, 0
    li a4, 0
    li a5, 0
    li a7, SYS_FUTEX
    ecall
    bltz a0, futex_shared_child_fail

    mv a0, s0
    la a1, stage_futex_shared_child_wake_ok_msg
    la a2, stage_futex_shared_child_wake_ok_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, futex_shared_child_fail

    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

futex_shared_child_fail:
    li a0, 1
    li a7, SYS_EXIT_GROUP
    ecall

socketpair_fork_child:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, socketpair_fork_child_fail

    mv a0, s5
    la a1, socketpair_roundtrip_expected
    li a2, socketpair_roundtrip_expected_len
    li a7, SYS_WRITE
    ecall
    li t1, socketpair_roundtrip_expected_len
    bne a0, t1, socketpair_fork_child_fail

    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    bltz a0, socketpair_fork_child_fail

    mv a0, s0
    la a1, stage_socketpair_fork_child_write_ok_msg
    la a2, stage_socketpair_fork_child_write_ok_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    bltz a0, socketpair_fork_child_fail

    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

socketpair_fork_child_fail:
    li a0, 1
    li a7, SYS_EXIT_GROUP
    ecall

pidfd_waitid_child:
    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

signalfd_sigchld_child:
    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

eventfd_epoll_child:
    li t0, 1
    la t2, fourth_stage_eventfd_counter
    sd t0, 0(t2)

    mv a0, s3
    la a1, fourth_stage_eventfd_counter
    li a2, fourth_stage_eventfd_counter_len
    li a7, SYS_WRITE
    ecall
    li t1, fourth_stage_eventfd_counter_len
    bne a0, t1, eventfd_epoll_child_fail

    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

eventfd_epoll_child_fail:
    li a0, 1
    li a7, SYS_EXIT_GROUP
    ecall

scm_rights_child:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    bltz a0, scm_rights_child_fail

    la t1, fourth_stage_scm_data_byte
    li t2, 'R'
    sb t2, 0(t1)

    la t1, fourth_stage_scm_iovec
    la t2, fourth_stage_scm_data_byte
    sd t2, fourth_stage_scm_iovec_base_offset(t1)
    li t2, fourth_stage_scm_data_len
    sd t2, fourth_stage_scm_iovec_len_offset(t1)

    la t1, fourth_stage_scm_cmsg_buf
    li t2, fourth_stage_scm_cmsg_fd_len
    sd t2, fourth_stage_scm_cmsg_len_offset(t1)
    li t2, SOL_SOCKET
    sw t2, fourth_stage_scm_cmsg_level_offset(t1)
    li t2, SCM_RIGHTS
    sw t2, fourth_stage_scm_cmsg_type_offset(t1)
    sd zero, 16(t1)
    sw s4, fourth_stage_scm_cmsg_data_offset(t1)

    la t1, fourth_stage_scm_msghdr
    sd zero, fourth_stage_scm_msghdr_name_offset(t1)
    sw zero, fourth_stage_scm_msghdr_namelen_offset(t1)
    la t2, fourth_stage_scm_iovec
    sd t2, fourth_stage_scm_msghdr_iov_offset(t1)
    li t2, 1
    sd t2, fourth_stage_scm_msghdr_iovlen_offset(t1)
    la t2, fourth_stage_scm_cmsg_buf
    sd t2, fourth_stage_scm_msghdr_control_offset(t1)
    li t2, fourth_stage_scm_cmsg_buf_len
    sd t2, fourth_stage_scm_msghdr_controllen_offset(t1)
    sw zero, fourth_stage_scm_msghdr_flags_offset(t1)

    mv a0, s5
    la a1, fourth_stage_scm_msghdr
    li a2, 0
    li a7, SYS_SENDMSG
    ecall
    li t1, fourth_stage_scm_data_len
    bne a0, t1, scm_rights_child_fail

    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    bltz a0, scm_rights_child_fail

    li a0, 0
    li a7, SYS_EXIT_GROUP
    ecall

scm_rights_child_fail:
    li a0, 1
    li a7, SYS_EXIT_GROUP
    ecall

pipe2_close_error:
    mv a0, s5
    li a7, SYS_CLOSE
    ecall

pipe2_read_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

pipe2_root_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

futex_shared_unmap_error:
    mv a0, s6
    li a1, futex_shared_map_len
    li a7, SYS_MUNMAP
    ecall

futex_shared_root_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

socketpair_close_error:
    mv a0, s5
    li a7, SYS_CLOSE
    ecall

socketpair_read_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

socketpair_root_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

openat2_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

openat2_root_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

openat2_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_openat2_smoke_fail_msg
    la a2, fourth_stage_openat2_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

pidfd_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

pidfd_root_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

pidfd_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_pidfd_smoke_fail_msg
    la a2, fourth_stage_pidfd_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

signalfd_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

signalfd_root_close_error:
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

signalfd_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_signalfd_smoke_fail_msg
    la a2, fourth_stage_signalfd_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

renameat2_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

renameat2_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_renameat2_smoke_fail_msg
    la a2, fourth_stage_renameat2_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

eventfd_epoll_close_error:
    mv a0, s5
    li a7, SYS_CLOSE
    ecall

eventfd_epoll_root_close_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall

eventfd_epoll_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_eventfd_epoll_smoke_fail_msg
    la a2, fourth_stage_eventfd_epoll_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

scm_rights_cleanup_error:
    mv a0, s6
    li a7, SYS_CLOSE
    ecall
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

scm_rights_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_scm_rights_smoke_fail_msg
    la a2, fourth_stage_scm_rights_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

copy_file_range_close_error:
    mv a0, s5
    li a7, SYS_CLOSE
    ecall

copy_file_range_cleanup_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

copy_file_range_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_copy_file_range_smoke_fail_msg
    la a2, fourth_stage_copy_file_range_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

splice_cleanup_error:
    mv a0, s6
    li a7, SYS_CLOSE
    ecall
    la t0, fourth_stage_pipe_fds
    lw a0, 0(t0)
    li a7, SYS_CLOSE
    ecall
    lw a0, 4(t0)
    li a7, SYS_CLOSE
    ecall
    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

splice_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_splice_smoke_fail_msg
    la a2, fourth_stage_splice_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

statx_cleanup_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

statx_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_statx_smoke_fail_msg
    la a2, fourth_stage_statx_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

inotify_cleanup_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s5
    li a7, SYS_CLOSE
    ecall
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

inotify_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_inotify_smoke_fail_msg
    la a2, fourth_stage_inotify_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

timerfd_cleanup_error:
    mv a0, s3
    li a7, SYS_CLOSE
    ecall
    mv a0, s4
    li a7, SYS_CLOSE
    ecall
    mv a0, s2
    li a7, SYS_CLOSE
    ecall

timerfd_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_timerfd_smoke_fail_msg
    la a2, fourth_stage_timerfd_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

socketpair_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_socketpair_smoke_fail_msg
    la a2, fourth_stage_socketpair_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

futex_shared_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_futex_shared_smoke_fail_msg
    la a2, fourth_stage_futex_shared_smoke_fail_msg_end
    sub a2, a2, a1
    addi a2, a2, -1
    jal ra, write_exact
    li a0, 1
    li a7, SYS_EXIT
    ecall

pipe2_smoke_failed:
    mv a0, s0
    la a1, fourth_stage_pipe2_smoke_fail_msg
    la a2, fourth_stage_pipe2_smoke_fail_msg_end
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

copy_bytes:
    beqz a2, copy_bytes_done

copy_bytes_loop:
    lbu t0, 0(a1)
    sb t0, 0(a0)
    addi a0, a0, 1
    addi a1, a1, 1
    addi a2, a2, -1
    bnez a2, copy_bytes_loop

copy_bytes_done:
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
