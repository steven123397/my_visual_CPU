CC       = gcc
CXX      = g++
CFLAGS   = -O2 -Wall -Wextra -g -std=c11
CXXFLAGS = -O2 -Wall -Wextra -g -std=c++17
C_SRCS   = src/memory.c src/decode.c
CPP_SRCS = src/cpu.cpp src/trap.cpp src/main.cpp src/arch/core_state.cpp src/arch/csr_file.cpp src/arch/vector_state.cpp src/debug/debug_session.cpp src/debug/debug_protocol.cpp src/debug/debug_protocol_command.cpp src/debug/debug_protocol_response.cpp src/exec/system_ops.cpp src/exec/memory_ops.cpp src/exec/control_flow_ops.cpp src/exec/integer_ops.cpp src/exec/floating_ops.cpp src/exec/vector_ops.cpp src/exec/branch_predictor.cpp src/exec/execution_profile.cpp src/exec/dbt_block_plan.cpp src/exec/dbt_ir.cpp src/exec/dbt_ir_eval.cpp src/exec/dbt_ir_lowering.cpp src/exec/dbt_block_cache.cpp src/exec/dbt_executable_cache.cpp src/exec/dbt_executable_memory.cpp src/exec/dbt_helper_execution_bridge.cpp src/exec/dbt_helper_replay.cpp src/exec/dbt_host_emitter.cpp src/exec/dbt_jit_engine.cpp src/exec/dbt_reference_fallback.cpp src/exec/dbt_reference_fallback_execution.cpp src/exec/dbt_runtime_dispatch.cpp src/exec/dbt_runtime_harness.cpp src/exec/dbt_runtime_invalidation.cpp src/exec/dbt_translator.cpp src/exec/interpreter_dbt_prototype.cpp src/exec/pipeline_sequence.cpp src/exec/pipeline_commit_boundary.cpp src/exec/pipeline_core_state.cpp src/exec/pipeline_hazards.cpp src/exec/physical_register_file.cpp src/exec/rename_map.cpp src/exec/reorder_buffer.cpp src/exec/load_store_queue.cpp src/exec/functional_backend.cpp src/exec/pipeline_backend.cpp src/exec/pipeline_backend_cycle.cpp src/exec/pipeline_backend_execute.cpp src/exec/pipeline_backend_frontend.cpp src/isa/atomic_contract.cpp src/isa/execution_context.cpp src/isa/instruction_semantics.cpp src/mem/ram.cpp src/mem/bus.cpp src/mem/dma_transaction.cpp src/mem/simple_l1_cache.cpp src/mem/address_space.cpp src/devices/ai_graph_package.cpp src/devices/tensor_golden_ops.cpp src/devices/ai_submission_queue.cpp src/devices/ai_scratchpad.cpp src/devices/ai_dma_engine.cpp src/devices/ai_graph_scheduler.cpp src/devices/ai_compute_gemm.cpp src/devices/ai_compute_conv.cpp src/devices/ai_compute_elementwise.cpp src/devices/ai_accelerator.cpp src/devices/uart16550.cpp src/devices/clint.cpp src/devices/plic.cpp src/devices/simple_storage.cpp src/devices/virtqueue.cpp src/devices/virtio_device.cpp src/devices/virtio_blk.cpp src/devices/virtio_mmio.cpp src/loader/elf_loader.cpp src/loader/binary_loader.cpp src/platform/machine.cpp
OBJS     = $(C_SRCS:.c=.o) $(CPP_SRCS:.cpp=.o)
CORE_OBJS = $(filter-out src/main.o,$(OBJS))
TARGET   = mycpu
HOST_TEST_BINS = tests/host/instruction_semantics_smoke tests/host/rvc_semantics_smoke tests/host/atomic_semantics_smoke tests/host/address_space_faults_smoke tests/host/predictor_smoke tests/host/pipeline_backend_smoke tests/host/pipeline_core_state_smoke tests/host/pipeline_forwarding_smoke tests/host/pipeline_commit_trace_smoke tests/host/pipeline_rename_commit_smoke tests/host/pipeline_speculation_contracts_smoke tests/host/backend_differential_smoke tests/host/debug_cli_smoke tests/host/debug_protocol_command_smoke tests/host/execution_profile_smoke tests/host/dbt_block_plan_smoke tests/host/dbt_translator_smoke tests/host/dbt_ir_eval_smoke tests/host/dbt_ir_lowering_smoke tests/host/dbt_block_cache_smoke tests/host/dbt_executable_cache_smoke tests/host/dbt_executable_memory_smoke tests/host/dbt_helper_execution_bridge_smoke tests/host/dbt_helper_replay_smoke tests/host/dbt_host_emitter_smoke tests/host/dbt_jit_engine_smoke tests/host/dbt_reference_fallback_smoke tests/host/dbt_reference_fallback_execution_smoke tests/host/dbt_runtime_dispatch_smoke tests/host/dbt_runtime_harness_smoke tests/host/dbt_runtime_invalidation_smoke tests/host/interpreter_dbt_prototype_smoke tests/host/interactive_terminal_smoke tests/host/course_os_shell_terminal_smoke tests/host/physical_register_file_smoke tests/host/rename_map_smoke tests/host/reorder_buffer_smoke tests/host/load_store_queue_smoke tests/host/vector_vlite_smoke tests/host/vector_backend_smoke tests/host/vector_operator_smoke tests/host/vector_cnn_smoke tests/host/vector_pipeline_smoke tests/host/virtio_blk_smoke tests/host/xv6_boot_smoke tests/host/xv6_shell_smoke tests/host/ai_tensor_golden_ops_smoke tests/host/ai_accelerator_submit_smoke tests/host/ai_accelerator_dma_smoke tests/host/ai_accelerator_cnn_smoke tests/host/ai_accelerator_gemm_smoke tests/host/ai_accelerator_profile_smoke tests/host/ai_accel_guest_smoke
INDEPENDENT_HOST_TEST_BINS = tests/host/spike_differential_smoke
HOST_TEST_SPLIT_BINS = tests/host/instruction_semantics_smoke_fp_compare_convert tests/host/instruction_semantics_smoke_fp_fma_flags tests/host/pipeline_backend_smoke_fp_arith_fma tests/host/pipeline_backend_smoke_fp_convert_tail
HOST_TEST_BINS += $(HOST_TEST_SPLIT_BINS)
ASM_TESTS = hello sum control_flow csr_trap timer_interrupt mtvec_modes trap_state exception_traps access_faults mmio_access_faults loads_signed_unsigned alu_word branches_signed_unsigned muldiv muldiv_edge_cases illegal_integer_encodings fence_noop privilege_transitions sret_transitions supervisor_exception_delegation supervisor_timer_interrupt csr_access_control csr_illegal_matrix counteren_access_control instret_counting machine_counter_csrs interrupt_csr_views delegation_edge_cases plic_machine_external_interrupt plic_supervisor_external_interrupt storage_device_basic supervisor_platform_smoke sv39_basic sv39_page_fault sv39_edge_faults sv39_sum_mxr sv39_mprv sv39_pagewalk_contracts sv39_exec_privilege sv39_tlb_flush sv39_tlb_ad_bits csr_semantic_consistency clint_split_access
ASM_ELFS  = $(ASM_TESTS:%=tests/asm/%.elf)
ASM_DEPS  = $(ASM_ELFS:.elf=.d)
ATOMIC_ASM_TESTS = atomic_basic atomic_ordering_smoke
ATOMIC_ASM_ELFS = $(ATOMIC_ASM_TESTS:%=tests/asm/%.elf)
ATOMIC_ASM_DEPS = $(ATOMIC_ASM_ELFS:.elf=.d)
FLAT_BIN_TESTS = hello
UNIT_TEST_NAMES = elf_loader_bss elf_loader_segments elf_loader_rejects elf_loader_header_rejects binary_loader machine_loader_reset bus_device_guards bus_region_contract dma_transaction_contract simple_l1_cache ai_graph_package ai_scratchpad ai_dma_engine ai_accelerator_mmio_contract mmio_contract_matrix uart_rx_contract storage_readiness virtio_mmio_contract virtqueue_smoke ai_accel_queue supervisor_runtime kernel_runtime kernel_bringup vm_address_space vm_process vm_object vm_fault trap_runtime trap_dispatch user_task user_task_bootstrap user_program user_program_smoke supervisor_demo_smoke monitor_commands course_os_stage1 course_os_stage2_syscall course_os_stage2_process course_os_stage2_fd_fs course_os_stage2_shell course_os_stage2_cow_crash course_os_stage2 course_os_stage3_elf course_os_stage3_sched_sync course_os_stage3_vm course_os_stage3_fs_shell course_os_stage3_proc course_os_stage3 kernel_alpha_common kernel_alpha_storage kernel_alpha_interrupt
UNIT_TEST_BINS = $(UNIT_TEST_NAMES:%=tests/unit/%)
AI_ACCEL_QUEUE_UNIT_OBJS = tests/unit/ai_accel_queue_test.o tests/unit/ai_accel_queue_guest.o
AI_ACCEL_QUEUE_UNIT_DEPS = $(AI_ACCEL_QUEUE_UNIT_OBJS:.o=.d)
SUPERVISOR_RUNTIME_UNIT_OBJS = tests/unit/supervisor_runtime_test.o tests/unit/supervisor_runtime_guest.o
SUPERVISOR_RUNTIME_UNIT_DEPS = $(SUPERVISOR_RUNTIME_UNIT_OBJS:.o=.d)
KERNEL_RUNTIME_UNIT_OBJS = tests/unit/kernel_runtime_test.o tests/unit/kernel_runtime_guest.o
KERNEL_RUNTIME_UNIT_DEPS = $(KERNEL_RUNTIME_UNIT_OBJS:.o=.d)
KERNEL_BRINGUP_UNIT_OBJS = tests/unit/kernel_bringup_test.o tests/unit/kernel_bringup_guest.o
KERNEL_BRINGUP_UNIT_DEPS = $(KERNEL_BRINGUP_UNIT_OBJS:.o=.d)
VM_ADDRESS_SPACE_UNIT_OBJS = tests/unit/vm_address_space_test.o tests/unit/vm_address_space_guest.o
VM_ADDRESS_SPACE_UNIT_DEPS = $(VM_ADDRESS_SPACE_UNIT_OBJS:.o=.d)
VM_PROCESS_UNIT_OBJS = tests/unit/vm_process_test.o tests/unit/vm_process_guest.o
VM_PROCESS_UNIT_DEPS = $(VM_PROCESS_UNIT_OBJS:.o=.d)
VM_OBJECT_UNIT_OBJS = tests/unit/vm_object_test.o tests/unit/vm_object_guest.o
VM_OBJECT_UNIT_DEPS = $(VM_OBJECT_UNIT_OBJS:.o=.d)
VM_FAULT_UNIT_OBJS = tests/unit/vm_fault_test.o tests/unit/vm_fault_guest.o
VM_FAULT_UNIT_DEPS = $(VM_FAULT_UNIT_OBJS:.o=.d)
TRAP_RUNTIME_UNIT_OBJS = tests/unit/trap_runtime_test.o tests/unit/trap_runtime_guest.o
TRAP_RUNTIME_UNIT_DEPS = $(TRAP_RUNTIME_UNIT_OBJS:.o=.d)
TRAP_DISPATCH_UNIT_OBJS = tests/unit/trap_dispatch_test.o tests/unit/trap_dispatch_guest.o
TRAP_DISPATCH_UNIT_DEPS = $(TRAP_DISPATCH_UNIT_OBJS:.o=.d)
USER_TASK_UNIT_OBJS = tests/unit/user_task_test.o tests/unit/user_task_guest.o
USER_TASK_UNIT_DEPS = $(USER_TASK_UNIT_OBJS:.o=.d)
USER_TASK_BOOTSTRAP_UNIT_OBJS = tests/unit/user_task_bootstrap_test.o tests/unit/user_task_bootstrap_guest.o
USER_TASK_BOOTSTRAP_UNIT_DEPS = $(USER_TASK_BOOTSTRAP_UNIT_OBJS:.o=.d)
USER_PROGRAM_UNIT_OBJS = tests/unit/user_program_test.o tests/unit/user_program_guest.o
USER_PROGRAM_UNIT_DEPS = $(USER_PROGRAM_UNIT_OBJS:.o=.d)
USER_PROGRAM_SMOKE_UNIT_OBJS = tests/unit/user_program_smoke_test.o tests/unit/user_program_smoke_guest.o
USER_PROGRAM_SMOKE_UNIT_DEPS = $(USER_PROGRAM_SMOKE_UNIT_OBJS:.o=.d)
SUPERVISOR_DEMO_SMOKE_UNIT_OBJS = tests/unit/supervisor_demo_smoke_test.o tests/unit/supervisor_demo_smoke_guest.o
SUPERVISOR_DEMO_SMOKE_UNIT_DEPS = $(SUPERVISOR_DEMO_SMOKE_UNIT_OBJS:.o=.d)
MONITOR_COMMANDS_UNIT_OBJS = tests/unit/monitor_commands_test.o tests/unit/monitor_commands_guest.o tests/unit/vm_debug_guest.o
MONITOR_COMMANDS_UNIT_DEPS = $(MONITOR_COMMANDS_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE1_UNIT_OBJS = tests/unit/course_os_stage1_test.o tests/unit/course_elf_loader_guest.o tests/unit/course_fd_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_fs_guest.o tests/unit/course_syscall_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE1_UNIT_DEPS = $(COURSE_OS_STAGE1_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE2_SYSCALL_UNIT_OBJS = tests/unit/course_os_stage2_syscall_test.o tests/unit/course_elf_loader_guest.o tests/unit/course_fd_guest.o tests/unit/course_syscall_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_fs_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE2_SYSCALL_UNIT_DEPS = $(COURSE_OS_STAGE2_SYSCALL_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE2_PROCESS_UNIT_OBJS = tests/unit/course_os_stage2_process_test.o tests/unit/course_elf_loader_guest.o tests/unit/course_fd_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_fs_guest.o tests/unit/course_syscall_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE2_PROCESS_UNIT_DEPS = $(COURSE_OS_STAGE2_PROCESS_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE2_FD_FS_UNIT_OBJS = tests/unit/course_os_stage2_fd_fs_test.o tests/unit/course_elf_loader_guest.o tests/unit/course_fd_guest.o tests/unit/course_fs_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_syscall_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE2_FD_FS_UNIT_DEPS = $(COURSE_OS_STAGE2_FD_FS_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE2_SHELL_UNIT_OBJS = tests/unit/course_os_stage2_shell_test.o tests/unit/course_elf_loader_guest.o tests/unit/course_shell_guest.o tests/unit/course_libc_guest.o tests/unit/course_fd_guest.o tests/unit/course_fs_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_syscall_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE2_SHELL_UNIT_DEPS = $(COURSE_OS_STAGE2_SHELL_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE2_COW_CRASH_UNIT_OBJS = tests/unit/course_os_stage2_cow_crash_test.o tests/unit/course_elf_loader_guest.o tests/unit/course_shell_guest.o tests/unit/course_libc_guest.o tests/unit/course_fd_guest.o tests/unit/course_fs_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_syscall_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE2_COW_CRASH_UNIT_DEPS = $(COURSE_OS_STAGE2_COW_CRASH_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE2_UNIT_OBJS = tests/unit/course_os_stage2_test.o tests/unit/course_os_stage2_guest.o tests/unit/course_elf_loader_guest.o tests/unit/course_shell_guest.o tests/unit/course_libc_guest.o tests/unit/course_fd_guest.o tests/unit/course_fs_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_syscall_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE2_UNIT_DEPS = $(COURSE_OS_STAGE2_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE3_ELF_UNIT_OBJS = tests/unit/course_os_stage3_elf_test.o tests/unit/course_elf_loader_guest.o tests/unit/course_libc_guest.o tests/unit/course_fd_guest.o tests/unit/course_fs_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_syscall_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE3_ELF_UNIT_DEPS = $(COURSE_OS_STAGE3_ELF_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE3_SCHED_SYNC_UNIT_OBJS = tests/unit/course_os_stage3_sched_sync_test.o tests/unit/course_sync_guest.o tests/unit/course_elf_loader_guest.o tests/unit/course_fd_guest.o tests/unit/course_fs_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_syscall_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE3_SCHED_SYNC_UNIT_DEPS = $(COURSE_OS_STAGE3_SCHED_SYNC_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE3_VM_UNIT_OBJS = tests/unit/course_os_stage3_vm_test.o tests/unit/course_elf_loader_guest.o tests/unit/course_fd_guest.o tests/unit/course_fs_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_syscall_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE3_VM_UNIT_DEPS = $(COURSE_OS_STAGE3_VM_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE3_FS_SHELL_UNIT_OBJS = tests/unit/course_os_stage3_fs_shell_test.o tests/unit/course_elf_loader_guest.o tests/unit/course_shell_guest.o tests/unit/course_libc_guest.o tests/unit/course_fd_guest.o tests/unit/course_fs_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_syscall_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE3_FS_SHELL_UNIT_DEPS = $(COURSE_OS_STAGE3_FS_SHELL_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE3_PROC_UNIT_OBJS = tests/unit/course_os_stage3_proc_test.o tests/unit/course_elf_loader_guest.o tests/unit/course_fd_guest.o tests/unit/course_fs_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_syscall_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE3_PROC_UNIT_DEPS = $(COURSE_OS_STAGE3_PROC_UNIT_OBJS:.o=.d)
COURSE_OS_STAGE3_UNIT_OBJS = tests/unit/course_os_stage3_test.o tests/unit/course_os_stage3_guest.o tests/unit/course_sync_guest.o tests/unit/course_elf_loader_guest.o tests/unit/course_shell_guest.o tests/unit/course_libc_guest.o tests/unit/course_fd_guest.o tests/unit/course_fs_guest.o tests/unit/course_scheduler_guest.o tests/unit/course_memory_guest.o tests/unit/course_syscall_guest.o tests/unit/course_process_guest.o tests/unit/course_user_programs_guest.o tests/unit/procfs_guest.o
COURSE_OS_STAGE3_UNIT_DEPS = $(COURSE_OS_STAGE3_UNIT_OBJS:.o=.d)
KERNEL_ALPHA_COMMON_UNIT_OBJS = tests/unit/kernel_alpha_common_test.o tests/unit/kernel_alpha_common_guest.o
KERNEL_ALPHA_COMMON_UNIT_DEPS = $(KERNEL_ALPHA_COMMON_UNIT_OBJS:.o=.d)
KERNEL_ALPHA_STORAGE_UNIT_OBJS = tests/unit/kernel_alpha_storage_test.o tests/unit/kernel_alpha_storage_guest.o
KERNEL_ALPHA_STORAGE_UNIT_DEPS = $(KERNEL_ALPHA_STORAGE_UNIT_OBJS:.o=.d)
KERNEL_ALPHA_INTERRUPT_UNIT_OBJS = tests/unit/kernel_alpha_interrupt_test.o tests/unit/kernel_alpha_interrupt_guest.o
KERNEL_ALPHA_INTERRUPT_UNIT_DEPS = $(KERNEL_ALPHA_INTERRUPT_UNIT_OBJS:.o=.d)
GUEST_SUPERVISOR_DEMO = guest/supervisor_demo.elf
GUEST_KERNEL_ALPHA_DEMO = guest/kernel_alpha_demo.elf
GUEST_KERNEL_ALPHA_FAULT_DEMO = guest/kernel_alpha_fault_demo.elf
GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO = guest/kernel_alpha_storage_no_media_demo.elf
GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO = guest/kernel_alpha_storage_not_ready_demo.elf
GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO = guest/kernel_alpha_storage_bad_magic_demo.elf
GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO = guest/kernel_alpha_storage_bad_block_count_demo.elf
GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO = guest/kernel_alpha_storage_lba_range_demo.elf
GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO = guest/kernel_alpha_storage_bad_command_demo.elf
GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO = guest/kernel_alpha_plic_not_ready_demo.elf
GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO = guest/kernel_alpha_timer_not_ready_demo.elf
GUEST_INTERACTIVE_OS_DEMO = guest/interactive_os.elf
GUEST_COURSE_OS_SHELL_DEMO = guest/course_os_shell.elf
GUEST_AI_ACCEL_DEMO = guest/ai_accel_demo.elf
GUEST_VECTOR_DEMO = guest/vector_demo.elf
GUEST_VECTOR_CNN_DEMO = guest/vector_cnn_demo.elf
LINUX_PROTO_SBI_SHIM_ELF = workloads/linux_proto/linux_sbi_shim.elf
LINUX_PROTO_SBI_SHIM_BIN = workloads/linux_proto/linux_sbi_shim.bin
LINUX_PROTO_SBI_SHIM_DEP = workloads/linux_proto/linux_sbi_shim.d
LINUX_PROTO_MININIT_ELF = workloads/linux_proto/linux_mininit.elf
LINUX_PROTO_MININIT_DEP = workloads/linux_proto/linux_mininit.d
LINUX_PROTO_POSTINIT_ELF = workloads/linux_proto/linux_postinit_smoke.elf
LINUX_PROTO_POSTINIT_DEP = workloads/linux_proto/linux_postinit_smoke.d
LINUX_PROTO_POSTINIT_EXEC_ELF = workloads/linux_proto/linux_postinit_exec_smoke.elf
LINUX_PROTO_POSTINIT_EXEC_DEP = workloads/linux_proto/linux_postinit_exec_smoke.d
LINUX_PROTO_POSTINIT_CLEANUP_ELF = workloads/linux_proto/linux_postinit_cleanup_smoke.elf
LINUX_PROTO_POSTINIT_CLEANUP_DEP = workloads/linux_proto/linux_postinit_cleanup_smoke.d
LINUX_PROTO_POSTINIT_DATA = workloads/linux_proto/post_init_data.txt
LINUX_PROTO_GENERATED_INITRD = workloads/linux_proto/rootfs.cpio
LINUX_PROTO_GENERATED_INITRD_DIR = workloads/linux_proto/rootfs_staging
LINUX_PROTO_GENERATED_DISK = workloads/linux_proto/rootfs.ext4
LINUX_PROTO_GENERATED_DISK_DIR = workloads/linux_proto/rootfs_disk_staging
LINUX_PROTO_GENERATED_DISK_BLOCKS ?= 8192
LINUX_DISTRO_REJECT_REPO_ROOTFS = MYCPU_CHECK_ROOTFS="$$rootfs" python3 -c 'import os,sys; p=os.environ["MYCPU_CHECK_ROOTFS"]; base=os.path.realpath("$(LINUX_PROTO_GENERATED_DISK)"); cand=os.path.realpath(p if os.path.isabs(p) else os.path.join(os.getcwd(), p)); sys.exit(1 if cand == base else 0)'
REQUIRE_EXTERNAL_LINUX_ROOTFS = rootfs="$(1)"; if [ -z "$$rootfs" ]; then echo "$(2)" >&2; exit 1; fi; $(LINUX_DISTRO_REJECT_REPO_ROOTFS) || { echo "$(2)" >&2; exit 1; }
GUEST_SRCS = guest/lib/platform.S guest/lib/trap_runtime.S guest/kernel/ai_accel.c guest/kernel/console.c guest/kernel/course_elf_loader.c guest/kernel/course_fd.c guest/kernel/course_fs.c guest/kernel/course_libc.c guest/kernel/course_memory.c guest/kernel/course_os_stage1.c guest/kernel/course_os_stage2.c guest/kernel/course_os_stage3.c guest/kernel/course_process.c guest/kernel/course_scheduler.c guest/kernel/course_shell.c guest/kernel/course_sync.c guest/kernel/course_syscall.c guest/kernel/course_user_programs.c guest/kernel/kernel_bringup.c guest/kernel/kernel_runtime.c guest/kernel/memory.c guest/kernel/panic.c guest/kernel/pmm.c guest/kernel/procfs.c guest/kernel/runtime_context.c guest/kernel/storage.c guest/kernel/supervisor_demo_smoke.c guest/kernel/supervisor_runtime.c guest/kernel/timer.c guest/kernel/trap.c guest/kernel/trap_dispatch.c guest/kernel/user_program.c guest/kernel/user_program_smoke.c guest/kernel/user_task.c guest/kernel/user_task_bootstrap.c guest/kernel/vm.c guest/kernel/vm_address_space.c guest/kernel/vm_process.c guest/kernel/vm_object.c guest/kernel/vm_fault.c guest/supervisor_demo/start.S guest/supervisor_demo/main.c
GUEST_OBJS = $(GUEST_SRCS:.S=.o)
GUEST_OBJS := $(GUEST_OBJS:.c=.o)
GUEST_KERNEL_ALPHA_COMMON_SRCS = guest/kernel_alpha/common.c guest/kernel_alpha/interrupt_contract.c guest/kernel_alpha/storage_contract.c
GUEST_KERNEL_ALPHA_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) $(GUEST_KERNEL_ALPHA_COMMON_SRCS) guest/kernel_alpha/main.c
GUEST_KERNEL_ALPHA_OBJS = $(GUEST_KERNEL_ALPHA_SRCS:.S=.o)
GUEST_KERNEL_ALPHA_OBJS := $(GUEST_KERNEL_ALPHA_OBJS:.c=.o)
GUEST_KERNEL_ALPHA_FAULT_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) $(GUEST_KERNEL_ALPHA_COMMON_SRCS) guest/kernel_alpha/fault_main.c
GUEST_KERNEL_ALPHA_FAULT_OBJS = $(GUEST_KERNEL_ALPHA_FAULT_SRCS:.S=.o)
GUEST_KERNEL_ALPHA_FAULT_OBJS := $(GUEST_KERNEL_ALPHA_FAULT_OBJS:.c=.o)
GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) $(GUEST_KERNEL_ALPHA_COMMON_SRCS) guest/kernel_alpha/storage_no_media_main.c
GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_OBJS = $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_SRCS:.S=.o)
GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_OBJS := $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_OBJS:.c=.o)
GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) $(GUEST_KERNEL_ALPHA_COMMON_SRCS) guest/kernel_alpha/storage_not_ready_main.c
GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_OBJS = $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_SRCS:.S=.o)
GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_OBJS := $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_OBJS:.c=.o)
GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) $(GUEST_KERNEL_ALPHA_COMMON_SRCS) guest/kernel_alpha/storage_bad_magic_main.c
GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_OBJS = $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_SRCS:.S=.o)
GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_OBJS := $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_OBJS:.c=.o)
GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) $(GUEST_KERNEL_ALPHA_COMMON_SRCS) guest/kernel_alpha/storage_bad_block_count_main.c
GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_OBJS = $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_SRCS:.S=.o)
GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_OBJS := $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_OBJS:.c=.o)
GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) $(GUEST_KERNEL_ALPHA_COMMON_SRCS) guest/kernel_alpha/storage_lba_range_main.c
GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_OBJS = $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_SRCS:.S=.o)
GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_OBJS := $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_OBJS:.c=.o)
GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) $(GUEST_KERNEL_ALPHA_COMMON_SRCS) guest/kernel_alpha/storage_bad_command_main.c
GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_OBJS = $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_SRCS:.S=.o)
GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_OBJS := $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_OBJS:.c=.o)
GUEST_KERNEL_ALPHA_PLIC_NOT_READY_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) $(GUEST_KERNEL_ALPHA_COMMON_SRCS) guest/kernel_alpha/plic_not_ready_main.c
GUEST_KERNEL_ALPHA_PLIC_NOT_READY_OBJS = $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_SRCS:.S=.o)
GUEST_KERNEL_ALPHA_PLIC_NOT_READY_OBJS := $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_OBJS:.c=.o)
GUEST_KERNEL_ALPHA_TIMER_NOT_READY_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) $(GUEST_KERNEL_ALPHA_COMMON_SRCS) guest/kernel_alpha/timer_not_ready_main.c
GUEST_KERNEL_ALPHA_TIMER_NOT_READY_OBJS = $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_SRCS:.S=.o)
GUEST_KERNEL_ALPHA_TIMER_NOT_READY_OBJS := $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_OBJS:.c=.o)
GUEST_INTERACTIVE_OS_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) guest/kernel/console_input.c guest/kernel/monitor.c guest/kernel/monitor_commands.c guest/kernel/monitor_format.c guest/kernel/vm_debug.c guest/interactive_os/main.c
GUEST_INTERACTIVE_OS_OBJS = $(GUEST_INTERACTIVE_OS_SRCS:.S=.o)
GUEST_INTERACTIVE_OS_OBJS := $(GUEST_INTERACTIVE_OS_OBJS:.c=.o)
GUEST_COURSE_OS_SHELL_SRCS = $(filter-out guest/supervisor_demo/main.c,$(GUEST_SRCS)) guest/kernel/console_input.c guest/course_os_shell/main.c
GUEST_COURSE_OS_SHELL_OBJS = $(GUEST_COURSE_OS_SHELL_SRCS:.S=.o)
GUEST_COURSE_OS_SHELL_OBJS := $(GUEST_COURSE_OS_SHELL_OBJS:.c=.o)
GUEST_AI_ACCEL_DEMO_SRCS = $(filter-out guest/supervisor_demo/start.S guest/supervisor_demo/main.c,$(GUEST_SRCS)) guest/ai_accel_demo/start.S guest/ai_accel_demo/main.c
GUEST_AI_ACCEL_DEMO_OBJS = $(GUEST_AI_ACCEL_DEMO_SRCS:.S=.o)
GUEST_AI_ACCEL_DEMO_OBJS := $(GUEST_AI_ACCEL_DEMO_OBJS:.c=.o)
GUEST_VECTOR_DEMO_SRCS = guest/lib/platform.S guest/vector_demo/start.S guest/vector_demo/main.S
GUEST_VECTOR_DEMO_OBJS = $(GUEST_VECTOR_DEMO_SRCS:.S=.o)
GUEST_VECTOR_DEMO_OBJS := $(GUEST_VECTOR_DEMO_OBJS:.c=.o)
GUEST_VECTOR_CNN_DEMO_SRCS = guest/lib/platform.S guest/vector_cnn_demo/start.S guest/vector_cnn_demo/main.S
GUEST_VECTOR_CNN_DEMO_OBJS = $(GUEST_VECTOR_CNN_DEMO_SRCS:.S=.o)
GUEST_VECTOR_CNN_DEMO_OBJS := $(GUEST_VECTOR_CNN_DEMO_OBJS:.c=.o)
DEPS     = $(OBJS:.o=.d) $(GUEST_OBJS:.o=.d) $(GUEST_KERNEL_ALPHA_OBJS:.o=.d) $(GUEST_KERNEL_ALPHA_FAULT_OBJS:.o=.d) $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_OBJS:.o=.d)
DEPS     += $(GUEST_INTERACTIVE_OS_OBJS:.o=.d)
DEPS     += $(GUEST_COURSE_OS_SHELL_OBJS:.o=.d)
DEPS     += $(GUEST_AI_ACCEL_DEMO_OBJS:.o=.d)
DEPS     += $(GUEST_VECTOR_DEMO_OBJS:.o=.d)
DEPS     += $(GUEST_VECTOR_CNN_DEMO_OBJS:.o=.d)
DEPS     += $(AI_ACCEL_QUEUE_UNIT_DEPS)
DEPS     += $(SUPERVISOR_RUNTIME_UNIT_DEPS)
DEPS     += $(KERNEL_RUNTIME_UNIT_DEPS)
DEPS     += $(KERNEL_BRINGUP_UNIT_DEPS)
DEPS     += $(VM_ADDRESS_SPACE_UNIT_DEPS)
DEPS     += $(VM_PROCESS_UNIT_DEPS)
DEPS     += $(VM_OBJECT_UNIT_DEPS)
DEPS     += $(VM_FAULT_UNIT_DEPS)
DEPS     += $(TRAP_RUNTIME_UNIT_DEPS)
DEPS     += $(TRAP_DISPATCH_UNIT_DEPS)
DEPS     += $(USER_TASK_UNIT_DEPS)
DEPS     += $(USER_TASK_BOOTSTRAP_UNIT_DEPS)
DEPS     += $(USER_PROGRAM_UNIT_DEPS)
DEPS     += $(USER_PROGRAM_SMOKE_UNIT_DEPS)
DEPS     += $(MONITOR_COMMANDS_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE1_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE2_SYSCALL_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE2_PROCESS_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE2_FD_FS_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE2_SHELL_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE2_COW_CRASH_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE2_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE3_ELF_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE3_SCHED_SYNC_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE3_VM_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE3_FS_SHELL_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE3_PROC_UNIT_DEPS)
DEPS     += $(COURSE_OS_STAGE3_UNIT_DEPS)
DEPS     += $(KERNEL_ALPHA_COMMON_UNIT_DEPS)
DEPS     += $(KERNEL_ALPHA_STORAGE_UNIT_DEPS)
DEPS     += $(KERNEL_ALPHA_INTERRUPT_UNIT_DEPS)
DEPS     += $(SUPERVISOR_DEMO_SMOKE_UNIT_DEPS)
DEPS     += $(ASM_DEPS) $(ATOMIC_ASM_DEPS)
DEPS     += $(LINUX_PROTO_SBI_SHIM_DEP) $(LINUX_PROTO_MININIT_DEP) $(LINUX_PROTO_POSTINIT_DEP) $(LINUX_PROTO_POSTINIT_EXEC_DEP)
TEST_TIMEOUT ?= 3s
AI_ACCEL_PROFILE_SMOKE_TIMEOUT ?= 10s
XV6_SHELL_SMOKE_TIMEOUT ?= 300s
SUPERVISOR_GUEST_TEST_TIMEOUT ?= 7s
KERNEL_ALPHA_GUEST_TEST_TIMEOUT ?= 7s
DEBUG_CLI_SMOKE_TIMEOUT ?= 12s
AI_ACCEL_GUEST_SMOKE_TIMEOUT ?= 10s
# Pipeline guest demos execute the full guest runtime under the pipeline
# backend and need a wider budget than the functional path on debug builds.
PIPELINE_GUEST_TEST_TIMEOUT ?= 12s
# The pipeline supervisor demo exercises the full early runtime and VM bring-up
# path and still runs noticeably slower than the other guest demos.
PIPELINE_SUPERVISOR_GUEST_TEST_TIMEOUT ?= 24s

# RISC-V toolchain (adjust prefix if needed)
RV_CC   = riscv64-unknown-elf-gcc
RV_OBJCOPY = riscv64-unknown-elf-objcopy
DTC = dtc
RV_FLAGS = -march=rv64im -mabi=lp64
RV_ELF_LDFLAGS = -nostdlib -Wl,-Ttext-segment=0x80000000
GUEST_RV_CFLAGS = $(RV_FLAGS) -mcmodel=medany -mno-relax -msmall-data-limit=0 -ffreestanding -fno-builtin -fno-stack-protector -fno-pie -MMD -MP -g -O2 -Wall -Wextra
GUEST_RV_LDFLAGS = -nostdlib -nostartfiles -static -no-pie -Wl,-T,guest/supervisor_demo/link.ld
RV_REQUIRED_TOOLS = $(RV_CC) $(RV_OBJCOPY)

include workloads/common.mk

ifeq ($(WORKLOAD_BUILD_NEEDS_RISCV),1)
WORKLOAD_BUILD_DEPS := check-riscv-toolchain
else ifeq ($(WORKLOAD_BUILD_NEEDS_RISCV),0)
WORKLOAD_BUILD_DEPS :=
else
$(error unsupported WORKLOAD_BUILD_NEEDS_RISCV: $(WORKLOAD_BUILD_NEEDS_RISCV))
endif

ifeq ($(WORKLOAD_RUN_MODE),debug-cli)
WORKLOAD_PROBE_LOAD_ARGS :=
ifneq ($(strip $(WORKLOAD_DISK)),)
WORKLOAD_PROBE_LOAD_ARGS += --disk $(WORKLOAD_DISK)
endif
ifneq ($(strip $(BOARD_BLOCK_TRANSPORT)),)
WORKLOAD_PROBE_LOAD_ARGS += --block-transport $(BOARD_BLOCK_TRANSPORT)
endif
ifeq ($(WORKLOAD_IMAGE_FORMAT),flat)
WORKLOAD_PROBE_LOAD_ARGS += --flat
ifneq ($(strip $(WORKLOAD_LOAD_ADDR)),)
WORKLOAD_PROBE_LOAD_ARGS += --addr $(WORKLOAD_LOAD_ADDR)
else
$(error WORKLOAD_IMAGE_FORMAT=flat requires WORKLOAD_LOAD_ADDR)
endif
else ifneq ($(WORKLOAD_IMAGE_FORMAT),elf)
$(error unsupported WORKLOAD_IMAGE_FORMAT: $(WORKLOAD_IMAGE_FORMAT))
endif
WORKLOAD_RUN_COMMAND = python3 $(WORKLOAD_PROBE_SCRIPT) --target ./$(TARGET) --image $(WORKLOAD_IMAGE) $(WORKLOAD_PROBE_LOAD_ARGS) $(WORKLOAD_AUTO_PAYLOAD_ARGS) $(WORKLOAD_PAYLOAD_ARGS) $(WORKLOAD_AUTO_GPR_SEED_ARGS) $(WORKLOAD_GPR_SEED_ARGS) --step-cycles $(WORKLOAD_PROBE_CYCLES)
else ifeq ($(WORKLOAD_RUN_MODE),ai-profile)
ifeq ($(strip $(WORKLOAD_AI_MANIFEST)),)
$(error WORKLOAD_RUN_MODE=ai-profile requires WORKLOAD_AI_MANIFEST)
endif
WORKLOAD_RUN_COMMAND = ./$(TARGET) --ai-profile-manifest $(WORKLOAD_AI_MANIFEST)
else
$(error unsupported WORKLOAD_RUN_MODE: $(WORKLOAD_RUN_MODE))
endif

.PHONY: all clean test test-pipeline test-fast-smoke test-standard-regression test-slow-guest test-opt-in-external check-riscv-toolchain check-dtc-tool FORCE build-workload build-workload-xv6 run-workload run-workload-xv6 smoke-workload smoke-workload-xv6

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

# Build ELF test binaries
tests/asm/%.elf: tests/asm/%.S | check-riscv-toolchain
	$(RV_CC) $(RV_FLAGS) -MMD -MP -MF $(@:.elf=.d) $(RV_ELF_LDFLAGS) -o $@ $<

guest/%.o: guest/%.S | check-riscv-toolchain
	$(RV_CC) $(GUEST_RV_CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

guest/%.o: guest/%.c | check-riscv-toolchain
	$(RV_CC) $(GUEST_RV_CFLAGS) -std=c11 -Iguest/include -Iinclude -c -o $@ $<

$(GUEST_SUPERVISOR_DEMO): $(GUEST_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_KERNEL_ALPHA_DEMO): $(GUEST_KERNEL_ALPHA_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_KERNEL_ALPHA_FAULT_DEMO): $(GUEST_KERNEL_ALPHA_FAULT_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO): $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO): $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO): $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO): $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO): $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO): $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO): $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO): $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_INTERACTIVE_OS_DEMO): $(GUEST_INTERACTIVE_OS_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_COURSE_OS_SHELL_DEMO): $(GUEST_COURSE_OS_SHELL_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_AI_ACCEL_DEMO): $(GUEST_AI_ACCEL_DEMO_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) $(GUEST_RV_LDFLAGS) -o $@ $^

$(GUEST_VECTOR_DEMO): $(GUEST_VECTOR_DEMO_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) -nostdlib -nostartfiles -static -no-pie -Wl,-T,guest/vector_demo/link.ld -o $@ $^

$(GUEST_VECTOR_CNN_DEMO): $(GUEST_VECTOR_CNN_DEMO_OBJS)
	$(RV_CC) $(GUEST_RV_CFLAGS) -nostdlib -nostartfiles -static -no-pie -Wl,-T,guest/vector_cnn_demo/link.ld -o $@ $^

# Build flat binary from ELF
tests/asm/%.bin: tests/asm/%.elf | check-riscv-toolchain
	$(RV_OBJCOPY) -O binary $< $@

$(LINUX_PROTO_SBI_SHIM_ELF): workloads/linux_proto/linux_sbi_shim.S workloads/linux_proto/linux_sbi_shim.ld | check-riscv-toolchain
	$(RV_CC) $(GUEST_RV_CFLAGS) -Iinclude -nostdlib -nostartfiles -static -no-pie -MMD -MP -MF $(LINUX_PROTO_SBI_SHIM_DEP) -Wl,-T,workloads/linux_proto/linux_sbi_shim.ld -o $@ workloads/linux_proto/linux_sbi_shim.S

$(LINUX_PROTO_SBI_SHIM_BIN): $(LINUX_PROTO_SBI_SHIM_ELF) | check-riscv-toolchain
	$(RV_OBJCOPY) -O binary $< $@

$(LINUX_PROTO_MININIT_ELF): workloads/linux_proto/linux_mininit.S | check-riscv-toolchain
	$(RV_CC) $(RV_FLAGS) -mcmodel=medany -mno-relax -nostdlib -nostartfiles -static -no-pie -MMD -MP -MF $(LINUX_PROTO_MININIT_DEP) -o $@ workloads/linux_proto/linux_mininit.S

$(LINUX_PROTO_POSTINIT_ELF): workloads/linux_proto/linux_postinit_smoke.S | check-riscv-toolchain
	$(RV_CC) $(RV_FLAGS) -mcmodel=medany -mno-relax -nostdlib -nostartfiles -static -no-pie -MMD -MP -MF $(LINUX_PROTO_POSTINIT_DEP) -o $@ workloads/linux_proto/linux_postinit_smoke.S

$(LINUX_PROTO_POSTINIT_EXEC_ELF): workloads/linux_proto/linux_postinit_exec_smoke.S | check-riscv-toolchain
	$(RV_CC) $(RV_FLAGS) -mcmodel=medany -mno-relax -nostdlib -nostartfiles -static -no-pie -MMD -MP -MF $(LINUX_PROTO_POSTINIT_EXEC_DEP) -o $@ workloads/linux_proto/linux_postinit_exec_smoke.S

$(LINUX_PROTO_POSTINIT_CLEANUP_ELF): workloads/linux_proto/linux_postinit_cleanup_smoke.S | check-riscv-toolchain
	$(RV_CC) $(RV_FLAGS) -mcmodel=medany -mno-relax -nostdlib -nostartfiles -static -no-pie -MMD -MP -MF $(LINUX_PROTO_POSTINIT_CLEANUP_DEP) -o $@ workloads/linux_proto/linux_postinit_cleanup_smoke.S

$(LINUX_PROTO_GENERATED_INITRD): $(LINUX_PROTO_MININIT_ELF) $(LINUX_PROTO_POSTINIT_ELF) $(LINUX_PROTO_POSTINIT_EXEC_ELF) $(LINUX_PROTO_POSTINIT_CLEANUP_ELF) $(LINUX_PROTO_POSTINIT_DATA)
	@rm -rf $(LINUX_PROTO_GENERATED_INITRD_DIR)
	@mkdir -p $(LINUX_PROTO_GENERATED_INITRD_DIR)
	@install -m 755 $(LINUX_PROTO_MININIT_ELF) $(LINUX_PROTO_GENERATED_INITRD_DIR)/init
	@install -m 755 $(LINUX_PROTO_POSTINIT_ELF) $(LINUX_PROTO_GENERATED_INITRD_DIR)/post-init-smoke
	@install -m 755 $(LINUX_PROTO_POSTINIT_EXEC_ELF) $(LINUX_PROTO_GENERATED_INITRD_DIR)/post-init-exec-smoke
	@install -m 755 $(LINUX_PROTO_POSTINIT_CLEANUP_ELF) $(LINUX_PROTO_GENERATED_INITRD_DIR)/post-init-cleanup-smoke
	@install -m 644 $(LINUX_PROTO_POSTINIT_DATA) $(LINUX_PROTO_GENERATED_INITRD_DIR)/post-init-data.txt
	@(cd $(LINUX_PROTO_GENERATED_INITRD_DIR) && find . | LC_ALL=C sort | cpio -o -H newc --quiet > ../$(notdir $@))

$(LINUX_PROTO_GENERATED_DISK): $(LINUX_PROTO_MININIT_ELF) $(LINUX_PROTO_POSTINIT_ELF) $(LINUX_PROTO_POSTINIT_EXEC_ELF) $(LINUX_PROTO_POSTINIT_CLEANUP_ELF) $(LINUX_PROTO_POSTINIT_DATA)
	@rm -rf $(LINUX_PROTO_GENERATED_DISK_DIR)
	@mkdir -p $(LINUX_PROTO_GENERATED_DISK_DIR)/dev
	@install -m 755 $(LINUX_PROTO_MININIT_ELF) $(LINUX_PROTO_GENERATED_DISK_DIR)/init
	@install -m 755 $(LINUX_PROTO_POSTINIT_ELF) $(LINUX_PROTO_GENERATED_DISK_DIR)/post-init-smoke
	@install -m 755 $(LINUX_PROTO_POSTINIT_EXEC_ELF) $(LINUX_PROTO_GENERATED_DISK_DIR)/post-init-exec-smoke
	@install -m 755 $(LINUX_PROTO_POSTINIT_CLEANUP_ELF) $(LINUX_PROTO_GENERATED_DISK_DIR)/post-init-cleanup-smoke
	@install -m 644 $(LINUX_PROTO_POSTINIT_DATA) $(LINUX_PROTO_GENERATED_DISK_DIR)/post-init-data.txt
	@rm -f $@
	@mke2fs -q -F -t ext4 -d $(LINUX_PROTO_GENERATED_DISK_DIR) -L rootfs $@ $(LINUX_PROTO_GENERATED_DISK_BLOCKS)

$(LINUX_PROTO_GENERATED_DTS): workloads/linux_proto/mycpu_virt.dts.in $(wildcard $(WORKLOAD_INITRD_IMAGE)) FORCE
	@if [ -n "$(WORKLOAD_INITRD_IMAGE)" ] && [ -f "$(WORKLOAD_INITRD_IMAGE)" ]; then \
		initrd_size=$$(stat -c%s "$(WORKLOAD_INITRD_IMAGE)"); \
		initrd_end=$$(printf '0x%x' $$(( $(BOARD_LINUX_INITRD_ADDR) + $$initrd_size ))); \
	else \
		initrd_end='$(BOARD_LINUX_INITRD_ADDR)'; \
	fi; \
	sed \
		-e 's|@BOARD_LINUX_BOOTARGS@|$(BOARD_LINUX_BOOTARGS)|g' \
		-e 's|@BOARD_LINUX_INITRD_ADDR@|$(BOARD_LINUX_INITRD_ADDR)|g' \
		-e "s|@BOARD_LINUX_INITRD_END@|$$initrd_end|g" \
		-e 's|@BOARD_LINUX_TIMEBASE_FREQUENCY@|$(BOARD_LINUX_TIMEBASE_FREQUENCY)|g' \
		-e 's|@BOARD_LINUX_RISCV_ISA@|$(BOARD_LINUX_RISCV_ISA)|g' \
		-e 's|@BOARD_RAM_BASE@|$(BOARD_RAM_BASE)|g' \
		-e 's|@BOARD_RAM_SIZE@|$(BOARD_RAM_SIZE)|g' \
		-e 's|@BOARD_UART_BASE@|$(BOARD_UART_BASE)|g' \
		-e 's|@BOARD_CLINT_BASE@|$(BOARD_CLINT_BASE)|g' \
		-e 's|@BOARD_PLIC_BASE@|$(BOARD_PLIC_BASE)|g' \
		-e 's|@BOARD_BLOCK_BASE@|$(BOARD_BLOCK_BASE)|g' \
		$< > $@

$(LINUX_PROTO_GENERATED_DTB): $(LINUX_PROTO_GENERATED_DTS) | check-dtc-tool
	$(DTC) -I dts -O dtb -o $@ $<

test: $(TARGET) $(ASM_ELFS) $(ATOMIC_ASM_ELFS) $(FLAT_BIN_TESTS:%=tests/asm/%.bin) $(UNIT_TEST_BINS) $(GUEST_SUPERVISOR_DEMO) $(GUEST_KERNEL_ALPHA_DEMO) $(GUEST_KERNEL_ALPHA_FAULT_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO) $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO) $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO) $(GUEST_INTERACTIVE_OS_DEMO) $(GUEST_COURSE_OS_SHELL_DEMO) $(GUEST_AI_ACCEL_DEMO) $(GUEST_VECTOR_DEMO) $(GUEST_VECTOR_CNN_DEMO) $(ASM_TESTS:%=test-%) $(ATOMIC_ASM_TESTS:%=test-%) $(UNIT_TEST_NAMES:%=test-unit-%) test-host-instruction_semantics_smoke test-host-rvc_semantics_smoke test-host-atomic_semantics_smoke test-host-debug_cli_smoke test-host-debug_protocol_command_smoke test-host-execution_profile_smoke test-host-dbt_block_plan_smoke test-host-dbt_translator_smoke test-host-dbt_ir_eval_smoke test-host-dbt_ir_lowering_smoke test-host-dbt_block_cache_smoke test-host-dbt_executable_cache_smoke test-host-dbt_executable_memory_smoke test-host-dbt_helper_execution_bridge_smoke test-host-dbt_helper_replay_smoke test-host-dbt_host_emitter_smoke test-host-dbt_jit_engine_smoke test-host-dbt_reference_fallback_smoke test-host-dbt_reference_fallback_execution_smoke test-host-dbt_runtime_dispatch_smoke test-host-dbt_runtime_harness_smoke test-host-dbt_runtime_invalidation_smoke test-host-interpreter_dbt_prototype_smoke test-host-interactive_terminal_smoke test-host-course_os_shell_terminal_smoke test-host-vector_vlite_smoke test-host-vector_backend_smoke test-host-vector_operator_smoke test-host-vector_cnn_smoke test-host-vector_pipeline_smoke test-host-virtio_blk_smoke test-host-xv6_boot_smoke test-host-run_debug_cli_probe test-host-ai_tensor_golden_ops_smoke test-host-ai_accelerator_submit_smoke test-host-ai_accelerator_dma_smoke test-host-ai_accelerator_cnn_smoke test-host-ai_accelerator_gemm_smoke test-host-ai_accelerator_profile_smoke test-host-ai_accel_guest_smoke test-guest-supervisor_demo test-guest-kernel_alpha_demo test-guest-kernel_alpha_fault_demo test-guest-kernel_alpha_storage_no_media_demo test-guest-kernel_alpha_storage_not_ready_demo test-guest-kernel_alpha_storage_bad_magic_demo test-guest-kernel_alpha_storage_bad_block_count_demo test-guest-kernel_alpha_storage_lba_range_demo test-guest-kernel_alpha_storage_bad_command_demo test-guest-kernel_alpha_plic_not_ready_demo test-guest-kernel_alpha_timer_not_ready_demo test-guest-interactive_os_demo test-guest-course_os_shell_demo test-guest-ai_accel_demo test-guest-vector_demo test-guest-vector_cnn_demo $(FLAT_BIN_TESTS:%=test-flat-%)

test-fast-smoke: $(TARGET) $(UNIT_TEST_BINS) test-host-debug_protocol_command_smoke test-host-instruction_semantics_smoke-core test-host-rvc_semantics_smoke test-host-atomic_semantics_smoke test-host-debug_cli_smoke test-host-interactive_terminal_smoke

test-standard-regression: test test-pipeline

test-slow-guest: test-guest-supervisor_demo test-guest-kernel_alpha_demo test-guest-kernel_alpha_fault_demo test-guest-kernel_alpha_storage_no_media_demo test-guest-kernel_alpha_storage_not_ready_demo test-guest-kernel_alpha_storage_bad_magic_demo test-guest-kernel_alpha_storage_bad_block_count_demo test-guest-kernel_alpha_storage_lba_range_demo test-guest-kernel_alpha_storage_bad_command_demo test-guest-kernel_alpha_plic_not_ready_demo test-guest-kernel_alpha_timer_not_ready_demo test-guest-interactive_os_demo test-guest-course_os_shell_demo test-guest-ai_accel_demo test-guest-vector_demo test-guest-vector_cnn_demo test-pipeline-guest-supervisor_demo test-pipeline-guest-kernel_alpha_demo test-pipeline-guest-kernel_alpha_fault_demo test-pipeline-guest-kernel_alpha_storage_no_media_demo test-pipeline-guest-kernel_alpha_storage_not_ready_demo test-pipeline-guest-kernel_alpha_storage_bad_magic_demo test-pipeline-guest-kernel_alpha_storage_bad_block_count_demo test-pipeline-guest-kernel_alpha_storage_lba_range_demo test-pipeline-guest-kernel_alpha_storage_bad_command_demo test-pipeline-guest-kernel_alpha_plic_not_ready_demo test-pipeline-guest-kernel_alpha_timer_not_ready_demo test-pipeline-guest-interactive_os_demo test-pipeline-guest-course_os_shell_demo test-pipeline-guest-ai_accel_demo test-pipeline-guest-vector_demo test-pipeline-guest-vector_cnn_demo test-host-xv6_boot_smoke test-host-xv6_shell_smoke run-workload-xv6

test-opt-in-external: test-host-run_debug_cli_probe_linux_proto_runtime test-host-run_debug_cli_probe_linux_distribution_runtime test-host-run_debug_cli_probe_linux_distribution_curated_matrix test-host-spike_differential

FORCE:

define RUN_ASM_TEST
	@extra_args=''; \
	case "$(1)" in \
		hello) expected='Hello, RISC-V!' ;; \
		sum) expected='55' ;; \
		control_flow) expected='OK' ;; \
		csr_trap) expected='CSR' ;; \
		timer_interrupt) expected='TMR' ;; \
		mtvec_modes) expected='MTV' ;; \
		trap_state) expected='STS' ;; \
		exception_traps) expected='EXC' ;; \
		access_faults) expected='ACF' ;; \
		mmio_access_faults) expected='MMIO' ;; \
		loads_signed_unsigned) expected='LDS' ;; \
		alu_word) expected='W32' ;; \
		branches_signed_unsigned) expected='BRU' ;; \
		muldiv) expected='MLD' ;; \
		muldiv_edge_cases) expected='MDE' ;; \
		illegal_integer_encodings) expected='ILL' ;; \
		fence_noop) expected='FNC' ;; \
		privilege_transitions) expected='PRV' ;; \
		sret_transitions) expected='SRT' ;; \
		supervisor_exception_delegation) expected='SDG' ;; \
		supervisor_timer_interrupt) expected='STI' ;; \
		csr_access_control) expected='CSR' ;; \
		csr_illegal_matrix) expected='CRM' ;; \
		counteren_access_control) expected='CNT' ;; \
		instret_counting) expected='IRT' ;; \
		machine_counter_csrs) expected='MCR' ;; \
		interrupt_csr_views) expected='ICV' ;; \
		delegation_edge_cases) expected='DEG' ;; \
		plic_machine_external_interrupt) expected='PME' ;; \
		plic_supervisor_external_interrupt) expected='PSE' ;; \
		storage_device_basic) expected='DSK'; extra_args='--disk tests/data/storage_basic.txt' ;; \
		supervisor_platform_smoke) expected='DRV'; extra_args='--disk tests/data/storage_basic.txt' ;; \
		sv39_basic) expected='SV39' ;; \
		sv39_page_fault) expected='PFLT' ;; \
		sv39_edge_faults) expected='EDG' ;; \
		sv39_sum_mxr) expected='SMX' ;; \
		sv39_mprv) expected='MPRV' ;; \
		sv39_pagewalk_contracts) expected='PWK' ;; \
		sv39_exec_privilege) expected='XPV' ;; \
		sv39_tlb_flush) expected='TLB' ;; \
		sv39_tlb_ad_bits) expected='TAD' ;; \
		csr_semantic_consistency) expected='CSC' ;; \
		clint_split_access) expected='CLT' ;; \
		atomic_basic) expected='ATM' ;; \
		atomic_ordering_smoke) expected='AOR' ;; \
		*) echo "Unknown asm test: $(1)" >&2; exit 1 ;; \
	esac; \
	echo "=== $(1)$(4) ==="; \
	output="$$(timeout $(2) ./$(TARGET) $(3) $$extra_args tests/asm/$(1).elf 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: $(1)$(4)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for $(1)$(4)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"
endef

test-%: FORCE $(TARGET) tests/asm/%.elf
	$(call RUN_ASM_TEST,$*,$(TEST_TIMEOUT),,,)

test-unit-%: FORCE tests/unit/%
	@echo "=== unit:$* ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./tests/unit/$* 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Unit test timed out: $*" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

define RUN_HOST_TEST_BINARY
	@echo "=== host:$(1) ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./tests/host/$(1) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: $(1)" >&2; \
	exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"
endef

test-host-%: FORCE tests/host/%
	$(call RUN_HOST_TEST_BINARY,$*)

test-host-instruction_semantics_smoke: FORCE tests/host/instruction_semantics_smoke tests/host/instruction_semantics_smoke_fp_compare_convert tests/host/instruction_semantics_smoke_fp_fma_flags
	@$(MAKE) --no-print-directory test-host-instruction_semantics_smoke-core
	@$(MAKE) --no-print-directory test-host-instruction_semantics_smoke_fp_compare_convert
	@$(MAKE) --no-print-directory test-host-instruction_semantics_smoke_fp_fma_flags

test-host-instruction_semantics_smoke-core: FORCE tests/host/instruction_semantics_smoke
	$(call RUN_HOST_TEST_BINARY,instruction_semantics_smoke)

test-host-pipeline_backend_smoke: FORCE tests/host/pipeline_backend_smoke tests/host/pipeline_backend_smoke_fp_arith_fma tests/host/pipeline_backend_smoke_fp_convert_tail
	@$(MAKE) --no-print-directory test-host-pipeline_backend_smoke-core
	@$(MAKE) --no-print-directory test-host-pipeline_backend_smoke_fp_arith_fma
	@$(MAKE) --no-print-directory test-host-pipeline_backend_smoke_fp_convert_tail

test-host-pipeline_backend_smoke-core: FORCE tests/host/pipeline_backend_smoke
	$(call RUN_HOST_TEST_BINARY,pipeline_backend_smoke)

test-host-vector_vlite_smoke: FORCE tests/host/vector_vlite_smoke
	@echo "=== host:vector_vlite_smoke ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./tests/host/vector_vlite_smoke 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: vector_vlite_smoke" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-vector_backend_smoke: FORCE tests/host/vector_backend_smoke
	@echo "=== host:vector_backend_smoke ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./tests/host/vector_backend_smoke 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: vector_backend_smoke" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-spike_differential: FORCE tests/host/spike_differential_smoke
	@echo "=== host:spike_differential ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./tests/host/spike_differential_smoke --run-differential 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: spike_differential" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-interactive_terminal_smoke: FORCE $(TARGET) $(GUEST_INTERACTIVE_OS_DEMO) tests/host/interactive_terminal_smoke
	@echo "=== host:interactive_terminal_smoke ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./tests/host/interactive_terminal_smoke functional 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: interactive_terminal_smoke" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-course_os_shell_terminal_smoke: FORCE $(TARGET) $(GUEST_COURSE_OS_SHELL_DEMO) tests/host/course_os_shell_terminal_smoke
	@echo "=== host:course_os_shell_terminal_smoke ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./tests/host/course_os_shell_terminal_smoke functional 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: course_os_shell_terminal_smoke" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-debug_cli_smoke: FORCE $(TARGET) $(GUEST_INTERACTIVE_OS_DEMO) $(GUEST_AI_ACCEL_DEMO) tests/host/debug_cli_smoke
	@echo "=== host:debug_cli_smoke ==="; \
	output="$$(timeout $(DEBUG_CLI_SMOKE_TIMEOUT) ./tests/host/debug_cli_smoke 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: debug_cli_smoke" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-xv6_boot_smoke: FORCE $(TARGET) build-workload-xv6 tests/host/xv6_boot_smoke
	@echo "=== host:xv6_boot_smoke ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./tests/host/xv6_boot_smoke 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: xv6_boot_smoke" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-xv6_shell_smoke: FORCE $(TARGET) build-workload-xv6 tests/host/xv6_shell_smoke
	@echo "=== host:xv6_shell_smoke ==="; \
	output="$$(timeout $(XV6_SHELL_SMOKE_TIMEOUT) ./tests/host/xv6_shell_smoke 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: xv6_shell_smoke" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-ai_accelerator_profile_smoke: FORCE $(TARGET) tests/host/ai_accelerator_profile_smoke
	@echo "=== host:ai_accelerator_profile_smoke ==="; \
	output="$$(timeout $(AI_ACCEL_PROFILE_SMOKE_TIMEOUT) ./tests/host/ai_accelerator_profile_smoke 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: ai_accelerator_profile_smoke" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-ai_accel_guest_smoke: FORCE $(TARGET) $(GUEST_AI_ACCEL_DEMO) tests/host/ai_accel_guest_smoke
	@echo "=== host:ai_accel_guest_smoke ==="; \
	output="$$(timeout $(AI_ACCEL_GUEST_SMOKE_TIMEOUT) ./tests/host/ai_accel_guest_smoke 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: ai_accel_guest_smoke" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe: FORCE $(TARGET) build-workload-xv6
	@echo "=== host:run_debug_cli_probe ==="; \
	output="$$(python3 tests/host/run_debug_cli_probe_test.py 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_proto_runtime: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_proto_runtime ==="; \
	output="$$(MYCPU_RUN_LINUX_PROTO_RUNTIME=1 python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_proto_block_mode_runtime_reaches_fourth_stage_when_requested 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_runtime: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_runtime ==="; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS),MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS must point to an external distribution rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_filesystem: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_filesystem ==="; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS),MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS must point to an external distribution rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=filesystem_consistency python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_tty_login: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_tty_login ==="; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS),MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS must point to an external distribution rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=tty_login_probe python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_process_control: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_process_control ==="; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS),MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS must point to an external distribution rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=process_control python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_fs_state_guardrail: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_fs_state_guardrail ==="; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS),MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS must point to an external distribution rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=fs_state_guardrail python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_filesystem_persistence: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_filesystem_persistence ==="; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS),MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS must point to an external distribution rootfs image); \
	temp_rootfs="$$(mktemp /tmp/mycpu-distro-rootfs.XXXXXX.ext4)"; \
	trap 'rm -f "$$temp_rootfs"' EXIT; \
	cp "$(MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS)" "$$temp_rootfs"; \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=filesystem_persistence MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS="$$temp_rootfs" python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_curated_alpine_shell: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_curated_alpine_shell ==="; \
	if [ -z "$(MYCPU_LINUX_DISTRO_CURATED_IMAGE)" ]; then \
		echo "MYCPU_LINUX_DISTRO_CURATED_IMAGE must point to an external distribution Image" >&2; \
		exit 1; \
	fi; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS),MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_curated_debian_shell: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_curated_debian_shell ==="; \
	if [ -z "$(MYCPU_LINUX_DISTRO_CURATED_IMAGE)" ]; then \
		echo "MYCPU_LINUX_DISTRO_CURATED_IMAGE must point to an external distribution Image" >&2; \
		exit 1; \
	fi; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_CURATED_DEBIAN_ROOTFS),MYCPU_LINUX_DISTRO_CURATED_DEBIAN_ROOTFS must point to an external Debian rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=debian python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_curated_matrix: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_curated_matrix ==="
	@$(MAKE) test-host-run_debug_cli_probe_linux_distribution_curated_alpine_shell
	@$(MAKE) test-host-run_debug_cli_probe_linux_distribution_curated_debian_shell

test-host-run_debug_cli_probe_linux_distribution_curated_alpine_isa_advertisement: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_curated_alpine_isa_advertisement ==="; \
	if [ -z "$(MYCPU_LINUX_DISTRO_CURATED_IMAGE)" ]; then \
		echo "MYCPU_LINUX_DISTRO_CURATED_IMAGE must point to an external distribution Image" >&2; \
		exit 1; \
	fi; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS),MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_curated_alpine_boot_log_reports_compressed_isa 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_curated_alpine_busybox_awk_fp: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_curated_alpine_busybox_awk_fp ==="; \
	if [ -z "$(MYCPU_LINUX_DISTRO_CURATED_IMAGE)" ]; then \
		echo "MYCPU_LINUX_DISTRO_CURATED_IMAGE must point to an external distribution Image" >&2; \
		exit 1; \
	fi; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS),MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_curated_alpine_busybox_awk_fp_matrix 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_curated_alpine_proc_cpuinfo_isa_view: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_curated_alpine_proc_cpuinfo_isa_view ==="; \
	if [ -z "$(MYCPU_LINUX_DISTRO_CURATED_IMAGE)" ]; then \
		echo "MYCPU_LINUX_DISTRO_CURATED_IMAGE must point to an external distribution Image" >&2; \
		exit 1; \
	fi; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS),MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_curated_alpine_proc_cpuinfo_isa_view 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_curated_alpine_auxv_hwcap_view: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_curated_alpine_auxv_hwcap_view ==="; \
	if [ -z "$(MYCPU_LINUX_DISTRO_CURATED_IMAGE)" ]; then \
		echo "MYCPU_LINUX_DISTRO_CURATED_IMAGE must point to an external distribution Image" >&2; \
		exit 1; \
	fi; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS),MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_curated_alpine_auxv_hwcap_view 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_curated_alpine_busybox_userland_abi_view: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_curated_alpine_busybox_userland_abi_view ==="; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS),MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_curated_alpine_busybox_userland_abi_view 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_curated_alpine_fp_static_surface_view: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_curated_alpine_fp_static_surface_view ==="; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS),MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_curated_alpine_fp_static_surface_view 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-host-run_debug_cli_probe_linux_distribution_curated_alpine_fcsr_syscall_roundtrip: FORCE $(TARGET)
	@echo "=== host:run_debug_cli_probe_linux_distribution_curated_alpine_fcsr_syscall_roundtrip ==="; \
	if [ -z "$(MYCPU_LINUX_DISTRO_CURATED_IMAGE)" ]; then \
		echo "MYCPU_LINUX_DISTRO_CURATED_IMAGE must point to an external distribution Image" >&2; \
		exit 1; \
	fi; \
	$(call REQUIRE_EXTERNAL_LINUX_ROOTFS,$(MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS),MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image); \
	output="$$(MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_curated_alpine_fcsr_syscall_roundtrip_probe 2>&1)"; \
	status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

build-workload: FORCE $(WORKLOAD_BUILD_DEPS)
	$(WORKLOAD_BUILD_COMMAND)

build-workload-xv6: FORCE
	$(MAKE) WORKLOAD_NAME=xv6 WORKLOAD_BOARD=$(WORKLOAD_BOARD) build-workload

run-workload: FORCE $(TARGET) build-workload
	$(WORKLOAD_RUN_COMMAND)

run-workload-xv6: FORCE
	$(MAKE) WORKLOAD_NAME=xv6 WORKLOAD_BOARD=$(WORKLOAD_BOARD) run-workload

smoke-workload: FORCE
	$(MAKE) WORKLOAD_NAME=$(WORKLOAD_NAME) WORKLOAD_BOARD=$(WORKLOAD_BOARD) $(WORKLOAD_SMOKE_TARGET)

smoke-workload-xv6: FORCE
	$(MAKE) WORKLOAD_NAME=xv6 WORKLOAD_BOARD=$(WORKLOAD_BOARD) smoke-workload

define RUN_INTERACTIVE_GUEST_DEMO_TEST
	@echo "=== guest_interactive_os_demo$(3) ==="; \
	output="$$(timeout $(1) ./tests/host/interactive_terminal_smoke $(2) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_interactive_os_demo$(3)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"
endef

define RUN_COURSE_OS_SHELL_GUEST_DEMO_TEST
	@echo "=== guest_course_os_shell_demo$(3) ==="; \
	output="$$(timeout $(1) ./tests/host/course_os_shell_terminal_smoke $(2) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_course_os_shell_demo$(3)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"
endef

test-guest-interactive_os_demo: FORCE $(TARGET) $(GUEST_INTERACTIVE_OS_DEMO) tests/host/interactive_terminal_smoke
	$(call RUN_INTERACTIVE_GUEST_DEMO_TEST,$(TEST_TIMEOUT),functional,)

test-pipeline-guest-interactive_os_demo: FORCE $(TARGET) $(GUEST_INTERACTIVE_OS_DEMO) tests/host/interactive_terminal_smoke
	$(call RUN_INTERACTIVE_GUEST_DEMO_TEST,$(PIPELINE_GUEST_TEST_TIMEOUT),pipeline, (pipeline))

test-guest-course_os_shell_demo: FORCE $(TARGET) $(GUEST_COURSE_OS_SHELL_DEMO) tests/host/course_os_shell_terminal_smoke
	$(call RUN_COURSE_OS_SHELL_GUEST_DEMO_TEST,$(TEST_TIMEOUT),functional,)

test-pipeline-guest-course_os_shell_demo: FORCE $(TARGET) $(GUEST_COURSE_OS_SHELL_DEMO) tests/host/course_os_shell_terminal_smoke
	$(call RUN_COURSE_OS_SHELL_GUEST_DEMO_TEST,$(PIPELINE_GUEST_TEST_TIMEOUT),pipeline, (pipeline))

test-guest-ai_accel_demo: FORCE $(TARGET) $(GUEST_AI_ACCEL_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_ai_accel_demo,$(GUEST_AI_ACCEL_DEMO),KMVAI,$(TEST_TIMEOUT),,,)

test-pipeline-guest-ai_accel_demo: FORCE $(TARGET) $(GUEST_AI_ACCEL_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_ai_accel_demo,$(GUEST_AI_ACCEL_DEMO),KMVAI,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,, (pipeline))

test-guest-vector_demo: FORCE $(TARGET) $(GUEST_VECTOR_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_vector_demo,$(GUEST_VECTOR_DEMO),V2OK,$(TEST_TIMEOUT),,,)

test-pipeline-guest-vector_demo: FORCE $(TARGET) $(GUEST_VECTOR_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_vector_demo,$(GUEST_VECTOR_DEMO),V2OK,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,, (pipeline))

test-guest-vector_cnn_demo: FORCE $(TARGET) $(GUEST_VECTOR_CNN_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_vector_cnn_demo,$(GUEST_VECTOR_CNN_DEMO),V3OK,$(TEST_TIMEOUT),,,)

test-pipeline-guest-vector_cnn_demo: FORCE $(TARGET) $(GUEST_VECTOR_CNN_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_vector_cnn_demo,$(GUEST_VECTOR_CNN_DEMO),V3OK,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,, (pipeline))

test-backend-cli: FORCE $(TARGET) tests/asm/hello.elf
	@echo "=== backend_cli ==="; \
	sh tests/host/backend_cli.sh $(TARGET) tests/asm/hello.elf $(TEST_TIMEOUT)

test-pipeline-%: FORCE $(TARGET) tests/asm/%.elf
	$(call RUN_ASM_TEST,$*,$(TEST_TIMEOUT),--backend pipeline, (pipeline))

test-pipeline: $(ASM_TESTS:%=test-pipeline-%) test-host-instruction_semantics_smoke test-host-atomic_semantics_smoke test-host-predictor_smoke test-host-pipeline_backend_smoke test-host-pipeline_core_state_smoke test-host-pipeline_forwarding_smoke test-host-pipeline_commit_trace_smoke test-host-pipeline_rename_commit_smoke test-host-pipeline_speculation_contracts_smoke test-host-backend_differential_smoke test-host-debug_cli_smoke test-host-debug_protocol_command_smoke test-host-execution_profile_smoke test-host-interpreter_dbt_prototype_smoke test-host-interactive_terminal_smoke test-host-course_os_shell_terminal_smoke test-host-vector_vlite_smoke test-host-vector_backend_smoke test-host-vector_operator_smoke test-host-vector_cnn_smoke test-host-vector_pipeline_smoke test-host-xv6_boot_smoke test-host-run_debug_cli_probe test-backend-cli test-pipeline-guest-supervisor_demo test-pipeline-guest-kernel_alpha_demo test-pipeline-guest-kernel_alpha_fault_demo test-pipeline-guest-kernel_alpha_storage_no_media_demo test-pipeline-guest-kernel_alpha_storage_not_ready_demo test-pipeline-guest-kernel_alpha_storage_bad_magic_demo test-pipeline-guest-kernel_alpha_storage_bad_block_count_demo test-pipeline-guest-kernel_alpha_storage_lba_range_demo test-pipeline-guest-kernel_alpha_storage_bad_command_demo test-pipeline-guest-kernel_alpha_plic_not_ready_demo test-pipeline-guest-kernel_alpha_timer_not_ready_demo test-pipeline-guest-interactive_os_demo test-pipeline-guest-course_os_shell_demo test-pipeline-guest-ai_accel_demo test-pipeline-guest-vector_demo test-pipeline-guest-vector_cnn_demo

test-flat-%: FORCE $(TARGET) tests/asm/%.bin
	@case "$*" in \
		hello) expected='Hello, RISC-V!' ;; \
		*) echo "Unknown flat-binary test: $*" >&2; exit 1 ;; \
	esac; \
	echo "=== $* (flat) ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) -b 80000000 tests/asm/$*.bin 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: $* (flat)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for $* (flat)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

define RUN_GUEST_DEMO_TEST
	@expected='$(3)'; \
	echo "=== $(1)$(7) ==="; \
	output="$$(timeout $(4) ./$(TARGET) $(5) $(6) $(2) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: $(1)$(7)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for $(1)$(7)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"
endef

test-guest-supervisor_demo: FORCE $(TARGET) $(GUEST_SUPERVISOR_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_supervisor_demo,$(GUEST_SUPERVISOR_DEMO),KRN,$(SUPERVISOR_GUEST_TEST_TIMEOUT),,--disk tests/data/storage_basic.txt,)

test-pipeline-guest-supervisor_demo: FORCE $(TARGET) $(GUEST_SUPERVISOR_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_supervisor_demo,$(GUEST_SUPERVISOR_DEMO),KRN,$(PIPELINE_SUPERVISOR_GUEST_TEST_TIMEOUT),--backend pipeline,--disk tests/data/storage_basic.txt, (pipeline))

test-guest-kernel_alpha_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_demo,$(GUEST_KERNEL_ALPHA_DEMO),KMVPET|course-os-stage1 sched=CFS-lite ctx=9 pf=4 reclaim=1 fs_create=5 btree_steps=48 proc=ps/meminfo/schedstat/fsstat|course-os-stage2 syscall=ok shell=ok procs=ok fd=ok fs=128/64K/3 pipe=ok cow=ok crash=isolated proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog|course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid,$(KERNEL_ALPHA_GUEST_TEST_TIMEOUT),,--disk tests/data/storage_basic.txt,)

test-pipeline-guest-kernel_alpha_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_demo,$(GUEST_KERNEL_ALPHA_DEMO),KMVPET|course-os-stage1 sched=CFS-lite ctx=9 pf=4 reclaim=1 fs_create=5 btree_steps=48 proc=ps/meminfo/schedstat/fsstat|course-os-stage2 syscall=ok shell=ok procs=ok fd=ok fs=128/64K/3 pipe=ok cow=ok crash=isolated proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog|course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,--disk tests/data/storage_basic.txt, (pipeline))

test-guest-kernel_alpha_fault_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_FAULT_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_fault_demo,$(GUEST_KERNEL_ALPHA_FAULT_DEMO),KMVX,$(TEST_TIMEOUT),,,)

test-pipeline-guest-kernel_alpha_fault_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_FAULT_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_fault_demo,$(GUEST_KERNEL_ALPHA_FAULT_DEMO),KMVX,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,, (pipeline))

test-guest-kernel_alpha_storage_no_media_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_no_media_demo,$(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO),KMVNX,$(TEST_TIMEOUT),,,)

test-pipeline-guest-kernel_alpha_storage_no_media_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_no_media_demo,$(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO),KMVNX,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,, (pipeline))

test-guest-kernel_alpha_storage_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_not_ready_demo,$(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO),KMVRX,$(TEST_TIMEOUT),,--disk-not-ready tests/data/storage_basic.txt,)

test-pipeline-guest-kernel_alpha_storage_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_not_ready_demo,$(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO),KMVRX,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,--disk-not-ready tests/data/storage_basic.txt, (pipeline))

test-guest-kernel_alpha_storage_bad_magic_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_bad_magic_demo,$(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO),KMVGX,$(TEST_TIMEOUT),,--disk-bad-magic tests/data/storage_basic.txt,)

test-pipeline-guest-kernel_alpha_storage_bad_magic_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_bad_magic_demo,$(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO),KMVGX,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,--disk-bad-magic tests/data/storage_basic.txt, (pipeline))

test-guest-kernel_alpha_storage_bad_block_count_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_bad_block_count_demo,$(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO),KMVBX,$(TEST_TIMEOUT),,--disk tests/data/storage_basic.txt,)

test-pipeline-guest-kernel_alpha_storage_bad_block_count_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_bad_block_count_demo,$(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO),KMVBX,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,--disk tests/data/storage_basic.txt, (pipeline))

test-guest-kernel_alpha_storage_lba_range_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_lba_range_demo,$(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO),KMVLX,$(TEST_TIMEOUT),,--disk tests/data/storage_basic.txt,)

test-pipeline-guest-kernel_alpha_storage_lba_range_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_lba_range_demo,$(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO),KMVLX,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,--disk tests/data/storage_basic.txt, (pipeline))

test-guest-kernel_alpha_storage_bad_command_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_bad_command_demo,$(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO),KMVCX,$(TEST_TIMEOUT),,--disk tests/data/storage_basic.txt,)

test-pipeline-guest-kernel_alpha_storage_bad_command_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_storage_bad_command_demo,$(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO),KMVCX,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,--disk tests/data/storage_basic.txt, (pipeline))

test-guest-kernel_alpha_plic_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_plic_not_ready_demo,$(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO),KMVPX,$(TEST_TIMEOUT),,,)

test-pipeline-guest-kernel_alpha_plic_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_plic_not_ready_demo,$(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO),KMVPX,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,, (pipeline))

test-guest-kernel_alpha_timer_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_timer_not_ready_demo,$(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO),KMVPETX,$(TEST_TIMEOUT),,,)

test-pipeline-guest-kernel_alpha_timer_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO)
	$(call RUN_GUEST_DEMO_TEST,guest_kernel_alpha_timer_not_ready_demo,$(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO),KMVPETX,$(PIPELINE_GUEST_TEST_TIMEOUT),--backend pipeline,, (pipeline))

check-riscv-toolchain:
	@missing=0; \
	for tool in $(RV_REQUIRED_TOOLS); do \
		if ! command -v $$tool >/dev/null 2>&1; then \
			echo "Missing required RISC-V tool: $$tool" >&2; \
			missing=1; \
		fi; \
	done; \
	if [ $$missing -ne 0 ]; then \
		echo "Install on Ubuntu: sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf" >&2; \
		exit 1; \
	fi

check-dtc-tool:
	@command -v $(DTC) >/dev/null 2>&1 || { \
		echo "Missing device-tree compiler: $(DTC)" >&2; \
		echo "Install on Ubuntu: sudo apt install device-tree-compiler" >&2; \
		exit 1; \
	}

clean:
	@if [ -d $(XV6_EXTERNAL_DIR) ]; then $(WORKLOAD_CLEAN_COMMAND) >/dev/null; fi
	rm -f $(TARGET) tests/asm/*.elf tests/asm/*.bin tests/asm/*.d $(UNIT_TEST_BINS) $(HOST_TEST_BINS) $(INDEPENDENT_HOST_TEST_BINS) $(SUPERVISOR_RUNTIME_UNIT_OBJS) $(SUPERVISOR_RUNTIME_UNIT_DEPS) $(KERNEL_RUNTIME_UNIT_OBJS) $(KERNEL_RUNTIME_UNIT_DEPS) $(KERNEL_BRINGUP_UNIT_OBJS) $(KERNEL_BRINGUP_UNIT_DEPS) $(VM_ADDRESS_SPACE_UNIT_OBJS) $(VM_ADDRESS_SPACE_UNIT_DEPS) $(VM_PROCESS_UNIT_OBJS) $(VM_PROCESS_UNIT_DEPS) $(VM_OBJECT_UNIT_OBJS) $(VM_OBJECT_UNIT_DEPS) $(VM_FAULT_UNIT_OBJS) $(VM_FAULT_UNIT_DEPS) $(TRAP_RUNTIME_UNIT_OBJS) $(TRAP_RUNTIME_UNIT_DEPS) $(TRAP_DISPATCH_UNIT_OBJS) $(TRAP_DISPATCH_UNIT_DEPS) $(USER_TASK_UNIT_OBJS) $(USER_TASK_UNIT_DEPS) $(USER_TASK_BOOTSTRAP_UNIT_OBJS) $(USER_TASK_BOOTSTRAP_UNIT_DEPS) $(USER_PROGRAM_UNIT_OBJS) $(USER_PROGRAM_UNIT_DEPS) $(USER_PROGRAM_SMOKE_UNIT_OBJS) $(USER_PROGRAM_SMOKE_UNIT_DEPS) $(SUPERVISOR_DEMO_SMOKE_UNIT_OBJS) $(SUPERVISOR_DEMO_SMOKE_UNIT_DEPS) $(MONITOR_COMMANDS_UNIT_OBJS) $(MONITOR_COMMANDS_UNIT_DEPS) $(COURSE_OS_STAGE1_UNIT_OBJS) $(COURSE_OS_STAGE1_UNIT_DEPS) $(COURSE_OS_STAGE2_SYSCALL_UNIT_OBJS) $(COURSE_OS_STAGE2_SYSCALL_UNIT_DEPS) $(COURSE_OS_STAGE2_PROCESS_UNIT_OBJS) $(COURSE_OS_STAGE2_PROCESS_UNIT_DEPS) $(COURSE_OS_STAGE2_FD_FS_UNIT_OBJS) $(COURSE_OS_STAGE2_FD_FS_UNIT_DEPS) $(COURSE_OS_STAGE2_SHELL_UNIT_OBJS) $(COURSE_OS_STAGE2_SHELL_UNIT_DEPS) $(COURSE_OS_STAGE2_COW_CRASH_UNIT_OBJS) $(COURSE_OS_STAGE2_COW_CRASH_UNIT_DEPS) $(COURSE_OS_STAGE2_UNIT_OBJS) $(COURSE_OS_STAGE2_UNIT_DEPS) $(COURSE_OS_STAGE3_ELF_UNIT_OBJS) $(COURSE_OS_STAGE3_ELF_UNIT_DEPS) $(COURSE_OS_STAGE3_SCHED_SYNC_UNIT_OBJS) $(COURSE_OS_STAGE3_SCHED_SYNC_UNIT_DEPS) $(COURSE_OS_STAGE3_VM_UNIT_OBJS) $(COURSE_OS_STAGE3_VM_UNIT_DEPS) $(COURSE_OS_STAGE3_FS_SHELL_UNIT_OBJS) $(COURSE_OS_STAGE3_FS_SHELL_UNIT_DEPS) $(COURSE_OS_STAGE3_PROC_UNIT_OBJS) $(COURSE_OS_STAGE3_PROC_UNIT_DEPS) $(COURSE_OS_STAGE3_UNIT_OBJS) $(COURSE_OS_STAGE3_UNIT_DEPS) $(KERNEL_ALPHA_COMMON_UNIT_OBJS) $(KERNEL_ALPHA_COMMON_UNIT_DEPS) $(KERNEL_ALPHA_STORAGE_UNIT_OBJS) $(KERNEL_ALPHA_STORAGE_UNIT_DEPS) $(KERNEL_ALPHA_INTERRUPT_UNIT_OBJS) $(KERNEL_ALPHA_INTERRUPT_UNIT_DEPS) $(GUEST_SUPERVISOR_DEMO) $(GUEST_KERNEL_ALPHA_DEMO) $(GUEST_KERNEL_ALPHA_FAULT_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO) $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO) $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO) $(GUEST_INTERACTIVE_OS_DEMO) $(GUEST_COURSE_OS_SHELL_DEMO) $(GUEST_AI_ACCEL_DEMO) $(GUEST_VECTOR_DEMO) $(GUEST_VECTOR_CNN_DEMO) $(LINUX_PROTO_SBI_SHIM_ELF) $(LINUX_PROTO_SBI_SHIM_BIN) $(LINUX_PROTO_SBI_SHIM_DEP) $(LINUX_PROTO_MININIT_ELF) $(LINUX_PROTO_MININIT_DEP) $(LINUX_PROTO_POSTINIT_ELF) $(LINUX_PROTO_POSTINIT_DEP) $(LINUX_PROTO_POSTINIT_EXEC_ELF) $(LINUX_PROTO_POSTINIT_EXEC_DEP) $(LINUX_PROTO_GENERATED_INITRD) $(LINUX_PROTO_GENERATED_DISK) $(LINUX_PROTO_GENERATED_DTS) $(LINUX_PROTO_GENERATED_DTB)
	rm -rf $(LINUX_PROTO_GENERATED_INITRD_DIR) $(LINUX_PROTO_GENERATED_DISK_DIR)
	find src guest -type f \( -name '*.o' -o -name '*.d' \) -delete

-include $(DEPS)
tests/unit/supervisor_runtime_test.o: tests/unit/supervisor_runtime.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/supervisor_runtime_guest.o: guest/kernel/supervisor_runtime.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/supervisor_runtime: $(SUPERVISOR_RUNTIME_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/ai_accel_queue_test.o: tests/unit/ai_accel_queue.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/ai_accel_queue_guest.o: guest/kernel/ai_accel.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/ai_accel_queue: $(AI_ACCEL_QUEUE_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/kernel_runtime_test.o: tests/unit/kernel_runtime.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_runtime_guest.o: guest/kernel/kernel_runtime.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_runtime: $(KERNEL_RUNTIME_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/kernel_bringup_test.o: tests/unit/kernel_bringup.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_bringup_guest.o: guest/kernel/kernel_bringup.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_bringup: $(KERNEL_BRINGUP_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/vm_address_space_test.o: tests/unit/vm_address_space.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/vm_address_space_guest.o: guest/kernel/vm_address_space.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/vm_address_space: $(VM_ADDRESS_SPACE_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/vm_process_test.o: tests/unit/vm_process.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/vm_process_guest.o: guest/kernel/vm_process.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/vm_process: $(VM_PROCESS_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/vm_object_test.o: tests/unit/vm_object.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/vm_object_guest.o: guest/kernel/vm_object.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/vm_object: $(VM_OBJECT_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/vm_fault_test.o: tests/unit/vm_fault.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/vm_fault_guest.o: guest/kernel/vm_fault.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/vm_fault: $(VM_FAULT_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/trap_runtime_test.o: tests/unit/trap_runtime.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/trap_runtime_guest.o: guest/kernel/trap.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/trap_runtime: $(TRAP_RUNTIME_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/trap_dispatch_test.o: tests/unit/trap_dispatch.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/trap_dispatch_guest.o: guest/kernel/trap_dispatch.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/trap_dispatch: $(TRAP_DISPATCH_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/user_task_test.o: tests/unit/user_task.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/user_task_guest.o: guest/kernel/user_task.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/user_task: $(USER_TASK_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/user_task_bootstrap_test.o: tests/unit/user_task_bootstrap.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/user_task_bootstrap_guest.o: guest/kernel/user_task_bootstrap.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/user_task_bootstrap: $(USER_TASK_BOOTSTRAP_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/user_program_test.o: tests/unit/user_program.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/user_program_guest.o: guest/kernel/user_program.c
	$(CC) $(CFLAGS) -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/user_program: $(USER_PROGRAM_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/user_program_smoke_test.o: tests/unit/user_program_smoke.c
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/user_program_smoke_guest.o: guest/kernel/user_program_smoke.c
	$(CC) $(CFLAGS) -DUNIT_TEST_HOST -ffunction-sections -fdata-sections -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/user_program_smoke: $(USER_PROGRAM_SMOKE_UNIT_OBJS)
	$(CC) $(CFLAGS) -Wl,--gc-sections -o $@ $^

tests/unit/supervisor_demo_smoke_test.o: tests/unit/supervisor_demo_smoke.c
	$(CC) $(CFLAGS) -DUNIT_TEST_HOST -ffunction-sections -fdata-sections -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/supervisor_demo_smoke_guest.o: guest/kernel/supervisor_demo_smoke.c
	$(CC) $(CFLAGS) -DUNIT_TEST_HOST -ffunction-sections -fdata-sections -Itests/unit/include -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/supervisor_demo_smoke: $(SUPERVISOR_DEMO_SMOKE_UNIT_OBJS)
	$(CC) $(CFLAGS) -Wl,--gc-sections -o $@ $^

tests/unit/monitor_commands_test.o: tests/unit/monitor_commands.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/monitor_commands_guest.o: guest/kernel/monitor_commands.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/vm_debug_guest.o: guest/kernel/vm_debug.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/monitor_commands: $(MONITOR_COMMANDS_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage1_test.o: tests/unit/course_os_stage1.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_scheduler_guest.o: guest/kernel/course_scheduler.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_memory_guest.o: guest/kernel/course_memory.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_fs_guest.o: guest/kernel/course_fs.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/procfs_guest.o: guest/kernel/procfs.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage1: $(COURSE_OS_STAGE1_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage2_syscall_test.o: tests/unit/course_os_stage2_syscall.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_syscall_guest.o: guest/kernel/course_syscall.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage2_syscall: $(COURSE_OS_STAGE2_SYSCALL_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage2_process_test.o: tests/unit/course_os_stage2_process.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_process_guest.o: guest/kernel/course_process.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_user_programs_guest.o: guest/kernel/course_user_programs.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage2_process: $(COURSE_OS_STAGE2_PROCESS_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage2_fd_fs_test.o: tests/unit/course_os_stage2_fd_fs.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_fd_guest.o: guest/kernel/course_fd.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage2_fd_fs: $(COURSE_OS_STAGE2_FD_FS_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage2_shell_test.o: tests/unit/course_os_stage2_shell.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_shell_guest.o: guest/kernel/course_shell.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage2_shell: $(COURSE_OS_STAGE2_SHELL_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage2_cow_crash_test.o: tests/unit/course_os_stage2_cow_crash.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage2_cow_crash: $(COURSE_OS_STAGE2_COW_CRASH_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage2_test.o: tests/unit/course_os_stage2.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage2_guest.o: guest/kernel/course_os_stage2.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage2: $(COURSE_OS_STAGE2_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage3_elf_test.o: tests/unit/course_os_stage3_elf.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_elf_loader_guest.o: guest/kernel/course_elf_loader.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_libc_guest.o: guest/kernel/course_libc.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage3_elf: $(COURSE_OS_STAGE3_ELF_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage3_sched_sync_test.o: tests/unit/course_os_stage3_sched_sync.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_sync_guest.o: guest/kernel/course_sync.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage3_sched_sync: $(COURSE_OS_STAGE3_SCHED_SYNC_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage3_vm_test.o: tests/unit/course_os_stage3_vm.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage3_vm: $(COURSE_OS_STAGE3_VM_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage3_fs_shell_test.o: tests/unit/course_os_stage3_fs_shell.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage3_fs_shell: $(COURSE_OS_STAGE3_FS_SHELL_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage3_proc_test.o: tests/unit/course_os_stage3_proc.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage3_proc: $(COURSE_OS_STAGE3_PROC_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/course_os_stage3_test.o: tests/unit/course_os_stage3.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage3_guest.o: guest/kernel/course_os_stage3.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/course_os_stage3: $(COURSE_OS_STAGE3_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/kernel_alpha_common_test.o: tests/unit/kernel_alpha_common.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_alpha_common_guest.o: guest/kernel_alpha/common.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_alpha_common: $(KERNEL_ALPHA_COMMON_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/kernel_alpha_storage_test.o: tests/unit/kernel_alpha_storage.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_alpha_storage_guest.o: guest/kernel_alpha/storage_contract.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_alpha_storage: $(KERNEL_ALPHA_STORAGE_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/kernel_alpha_interrupt_test.o: tests/unit/kernel_alpha_interrupt.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_alpha_interrupt_guest.o: guest/kernel_alpha/interrupt_contract.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_alpha_interrupt: $(KERNEL_ALPHA_INTERRUPT_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/%: tests/unit/%.cpp $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(CORE_OBJS)

tests/host/%: tests/host/%.cpp $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(CORE_OBJS)
