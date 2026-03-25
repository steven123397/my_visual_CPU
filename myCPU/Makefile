CC       = gcc
CXX      = g++
CFLAGS   = -O2 -Wall -Wextra -g -std=c11
CXXFLAGS = -O2 -Wall -Wextra -g -std=c++17
C_SRCS   = src/memory.c src/decode.c
CPP_SRCS = src/cpu.cpp src/trap.cpp src/main.cpp src/arch/core_state.cpp src/arch/csr_file.cpp src/exec/system_ops.cpp src/exec/memory_ops.cpp src/exec/control_flow_ops.cpp src/exec/integer_ops.cpp src/exec/functional_backend.cpp src/exec/pipeline_backend.cpp src/isa/execution_context.cpp src/isa/instruction_semantics.cpp src/mem/ram.cpp src/mem/bus.cpp src/mem/address_space.cpp src/devices/uart16550.cpp src/devices/clint.cpp src/devices/plic.cpp src/devices/simple_storage.cpp src/loader/elf_loader.cpp src/loader/binary_loader.cpp src/platform/machine.cpp
OBJS     = $(C_SRCS:.c=.o) $(CPP_SRCS:.cpp=.o)
CORE_OBJS = $(filter-out src/main.o,$(OBJS))
TARGET   = mycpu
HOST_TEST_BINS = tests/host/instruction_semantics_smoke tests/host/address_space_faults_smoke tests/host/pipeline_backend_smoke tests/host/backend_differential_smoke
ASM_TESTS = hello sum control_flow csr_trap timer_interrupt mtvec_modes trap_state exception_traps access_faults loads_signed_unsigned alu_word branches_signed_unsigned muldiv muldiv_edge_cases illegal_integer_encodings fence_noop privilege_transitions sret_transitions supervisor_exception_delegation supervisor_timer_interrupt csr_access_control counteren_access_control instret_counting machine_counter_csrs interrupt_csr_views delegation_edge_cases plic_machine_external_interrupt plic_supervisor_external_interrupt storage_device_basic supervisor_platform_smoke sv39_basic sv39_page_fault sv39_edge_faults sv39_sum_mxr sv39_tlb_flush sv39_tlb_ad_bits csr_semantic_consistency clint_split_access
ASM_ELFS  = $(ASM_TESTS:%=tests/asm/%.elf)
FLAT_BIN_TESTS = hello
UNIT_TEST_NAMES = elf_loader_bss bus_device_guards storage_readiness supervisor_runtime kernel_runtime kernel_alpha_common kernel_alpha_storage kernel_alpha_interrupt
UNIT_TEST_BINS = $(UNIT_TEST_NAMES:%=tests/unit/%)
SUPERVISOR_RUNTIME_UNIT_OBJS = tests/unit/supervisor_runtime_test.o tests/unit/supervisor_runtime_guest.o
SUPERVISOR_RUNTIME_UNIT_DEPS = $(SUPERVISOR_RUNTIME_UNIT_OBJS:.o=.d)
KERNEL_RUNTIME_UNIT_OBJS = tests/unit/kernel_runtime_test.o tests/unit/kernel_runtime_guest.o
KERNEL_RUNTIME_UNIT_DEPS = $(KERNEL_RUNTIME_UNIT_OBJS:.o=.d)
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
GUEST_SRCS = guest/lib/platform.S guest/lib/trap_runtime.S guest/kernel/console.c guest/kernel/kernel_bringup.c guest/kernel/kernel_runtime.c guest/kernel/memory.c guest/kernel/panic.c guest/kernel/pmm.c guest/kernel/runtime_context.c guest/kernel/storage.c guest/kernel/supervisor_demo_smoke.c guest/kernel/supervisor_runtime.c guest/kernel/timer.c guest/kernel/trap.c guest/kernel/trap_dispatch.c guest/kernel/user_program.c guest/kernel/user_program_smoke.c guest/kernel/user_task.c guest/kernel/user_task_bootstrap.c guest/kernel/vm.c guest/kernel/vm_address_space.c guest/kernel/vm_process.c guest/kernel/vm_object.c guest/kernel/vm_fault.c guest/supervisor_demo/start.S guest/supervisor_demo/main.c
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
DEPS     = $(OBJS:.o=.d) $(GUEST_OBJS:.o=.d) $(GUEST_KERNEL_ALPHA_OBJS:.o=.d) $(GUEST_KERNEL_ALPHA_FAULT_OBJS:.o=.d) $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_OBJS:.o=.d)
DEPS     += $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_OBJS:.o=.d)
DEPS     += $(SUPERVISOR_RUNTIME_UNIT_DEPS)
DEPS     += $(KERNEL_RUNTIME_UNIT_DEPS)
DEPS     += $(KERNEL_ALPHA_COMMON_UNIT_DEPS)
DEPS     += $(KERNEL_ALPHA_STORAGE_UNIT_DEPS)
DEPS     += $(KERNEL_ALPHA_INTERRUPT_UNIT_DEPS)
TEST_TIMEOUT ?= 2s
PIPELINE_GUEST_TEST_TIMEOUT ?= 4s

# RISC-V toolchain (adjust prefix if needed)
RV_CC   = riscv64-unknown-elf-gcc
RV_OBJCOPY = riscv64-unknown-elf-objcopy
RV_FLAGS = -march=rv64im -mabi=lp64
RV_ELF_LDFLAGS = -nostdlib -Wl,-Ttext-segment=0x80000000
GUEST_RV_CFLAGS = $(RV_FLAGS) -mcmodel=medany -mno-relax -msmall-data-limit=0 -ffreestanding -fno-builtin -fno-stack-protector -fno-pie -MMD -MP -g -O2 -Wall -Wextra
GUEST_RV_LDFLAGS = -nostdlib -nostartfiles -static -no-pie -Wl,-T,guest/supervisor_demo/link.ld
RV_REQUIRED_TOOLS = $(RV_CC) $(RV_OBJCOPY)

.PHONY: all clean test test-pipeline check-riscv-toolchain FORCE

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

# Build ELF test binaries
tests/asm/%.elf: tests/asm/%.S | check-riscv-toolchain
	$(RV_CC) $(RV_FLAGS) $(RV_ELF_LDFLAGS) -o $@ $<

guest/%.o: guest/%.S | check-riscv-toolchain
	$(RV_CC) $(GUEST_RV_CFLAGS) -Iguest/include -Iinclude -c -o $@ $<

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

# Build flat binary from ELF
tests/asm/%.bin: tests/asm/%.elf | check-riscv-toolchain
	$(RV_OBJCOPY) -O binary $< $@

test: $(TARGET) $(ASM_ELFS) $(FLAT_BIN_TESTS:%=tests/asm/%.bin) $(UNIT_TEST_BINS) $(GUEST_SUPERVISOR_DEMO) $(GUEST_KERNEL_ALPHA_DEMO) $(GUEST_KERNEL_ALPHA_FAULT_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO) $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO) $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO) $(ASM_TESTS:%=test-%) $(UNIT_TEST_NAMES:%=test-unit-%) test-guest-supervisor_demo test-guest-kernel_alpha_demo test-guest-kernel_alpha_fault_demo test-guest-kernel_alpha_storage_no_media_demo test-guest-kernel_alpha_storage_not_ready_demo test-guest-kernel_alpha_storage_bad_magic_demo test-guest-kernel_alpha_storage_bad_block_count_demo test-guest-kernel_alpha_storage_lba_range_demo test-guest-kernel_alpha_storage_bad_command_demo test-guest-kernel_alpha_plic_not_ready_demo test-guest-kernel_alpha_timer_not_ready_demo $(FLAT_BIN_TESTS:%=test-flat-%)

FORCE:

test-%: FORCE $(TARGET) tests/asm/%.elf
	@extra_args=''; \
	case "$*" in \
		hello) expected='Hello, RISC-V!' ;; \
		sum) expected='55' ;; \
		control_flow) expected='OK' ;; \
		csr_trap) expected='CSR' ;; \
		timer_interrupt) expected='TMR' ;; \
		mtvec_modes) expected='MTV' ;; \
		trap_state) expected='STS' ;; \
		exception_traps) expected='EXC' ;; \
		access_faults) expected='ACF' ;; \
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
		sv39_tlb_flush) expected='TLB' ;; \
		sv39_tlb_ad_bits) expected='TAD' ;; \
		csr_semantic_consistency) expected='CSC' ;; \
		clint_split_access) expected='CLT' ;; \
		*) echo "Unknown asm test: $*" >&2; exit 1 ;; \
	esac; \
	echo "=== $* ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) $$extra_args tests/asm/$*.elf 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: $*" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for $*" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

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

test-host-%: FORCE tests/host/%
	@echo "=== host:$* ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./tests/host/$* 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Host test timed out: $*" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	printf '%s\n' "$$output"

test-backend-cli: FORCE $(TARGET) tests/asm/hello.elf
	@echo "=== backend_cli ==="; \
	sh tests/host/backend_cli.sh $(TARGET) tests/asm/hello.elf $(TEST_TIMEOUT)

test-pipeline-%: FORCE $(TARGET) tests/asm/%.elf
	@extra_args=''; \
	case "$*" in \
		hello) expected='Hello, RISC-V!' ;; \
		sum) expected='55' ;; \
		control_flow) expected='OK' ;; \
		csr_trap) expected='CSR' ;; \
		timer_interrupt) expected='TMR' ;; \
		mtvec_modes) expected='MTV' ;; \
		trap_state) expected='STS' ;; \
		exception_traps) expected='EXC' ;; \
		access_faults) expected='ACF' ;; \
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
		sv39_tlb_flush) expected='TLB' ;; \
		sv39_tlb_ad_bits) expected='TAD' ;; \
		csr_semantic_consistency) expected='CSC' ;; \
		clint_split_access) expected='CLT' ;; \
		*) echo "Unknown asm test: $*" >&2; exit 1 ;; \
	esac; \
	echo "=== $* (pipeline) ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) --backend pipeline $$extra_args tests/asm/$*.elf 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: $* (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for $* (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline: $(ASM_TESTS:%=test-pipeline-%) test-host-pipeline_backend_smoke test-host-backend_differential_smoke test-backend-cli test-pipeline-guest-supervisor_demo test-pipeline-guest-kernel_alpha_demo test-pipeline-guest-kernel_alpha_fault_demo test-pipeline-guest-kernel_alpha_storage_no_media_demo test-pipeline-guest-kernel_alpha_storage_not_ready_demo test-pipeline-guest-kernel_alpha_storage_bad_magic_demo test-pipeline-guest-kernel_alpha_storage_bad_block_count_demo test-pipeline-guest-kernel_alpha_storage_lba_range_demo test-pipeline-guest-kernel_alpha_storage_bad_command_demo test-pipeline-guest-kernel_alpha_plic_not_ready_demo test-pipeline-guest-kernel_alpha_timer_not_ready_demo

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

test-guest-supervisor_demo: FORCE $(TARGET) $(GUEST_SUPERVISOR_DEMO)
	@expected='KRN'; \
	echo "=== guest_supervisor_demo ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) --disk tests/data/storage_basic.txt $(GUEST_SUPERVISOR_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_supervisor_demo" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_supervisor_demo" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline-guest-supervisor_demo: FORCE $(TARGET) $(GUEST_SUPERVISOR_DEMO)
	@expected='KRN'; \
	echo "=== guest_supervisor_demo (pipeline) ==="; \
	output="$$(timeout $(PIPELINE_GUEST_TEST_TIMEOUT) ./$(TARGET) --backend pipeline --disk tests/data/storage_basic.txt $(GUEST_SUPERVISOR_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_supervisor_demo (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_supervisor_demo (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-guest-kernel_alpha_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_DEMO)
	@expected='KMVPETDS'; \
	echo "=== guest_kernel_alpha_demo ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) --disk tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_demo" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_demo" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline-guest-kernel_alpha_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_DEMO)
	@expected='KMVPETDS'; \
	echo "=== guest_kernel_alpha_demo (pipeline) ==="; \
	output="$$(timeout $(PIPELINE_GUEST_TEST_TIMEOUT) ./$(TARGET) --backend pipeline --disk tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_demo (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_demo (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-guest-kernel_alpha_fault_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_FAULT_DEMO)
	@expected='KMVX'; \
	echo "=== guest_kernel_alpha_fault_demo ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) $(GUEST_KERNEL_ALPHA_FAULT_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_fault_demo" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_fault_demo" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline-guest-kernel_alpha_fault_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_FAULT_DEMO)
	@expected='KMVX'; \
	echo "=== guest_kernel_alpha_fault_demo (pipeline) ==="; \
	output="$$(timeout $(PIPELINE_GUEST_TEST_TIMEOUT) ./$(TARGET) --backend pipeline $(GUEST_KERNEL_ALPHA_FAULT_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_fault_demo (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_fault_demo (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-guest-kernel_alpha_storage_no_media_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO)
	@expected='KMVNX'; \
	echo "=== guest_kernel_alpha_storage_no_media_demo ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_no_media_demo" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_no_media_demo" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline-guest-kernel_alpha_storage_no_media_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO)
	@expected='KMVNX'; \
	echo "=== guest_kernel_alpha_storage_no_media_demo (pipeline) ==="; \
	output="$$(timeout $(PIPELINE_GUEST_TEST_TIMEOUT) ./$(TARGET) --backend pipeline $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_no_media_demo (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_no_media_demo (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-guest-kernel_alpha_storage_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO)
	@expected='KMVRX'; \
	echo "=== guest_kernel_alpha_storage_not_ready_demo ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) --disk-not-ready tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_not_ready_demo" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_not_ready_demo" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline-guest-kernel_alpha_storage_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO)
	@expected='KMVRX'; \
	echo "=== guest_kernel_alpha_storage_not_ready_demo (pipeline) ==="; \
	output="$$(timeout $(PIPELINE_GUEST_TEST_TIMEOUT) ./$(TARGET) --backend pipeline --disk-not-ready tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_not_ready_demo (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_not_ready_demo (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-guest-kernel_alpha_storage_bad_magic_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO)
	@expected='KMVGX'; \
	echo "=== guest_kernel_alpha_storage_bad_magic_demo ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) --disk-bad-magic tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_bad_magic_demo" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_bad_magic_demo" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline-guest-kernel_alpha_storage_bad_magic_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO)
	@expected='KMVGX'; \
	echo "=== guest_kernel_alpha_storage_bad_magic_demo (pipeline) ==="; \
	output="$$(timeout $(PIPELINE_GUEST_TEST_TIMEOUT) ./$(TARGET) --backend pipeline --disk-bad-magic tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_bad_magic_demo (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_bad_magic_demo (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-guest-kernel_alpha_storage_bad_block_count_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO)
	@expected='KMVBX'; \
	echo "=== guest_kernel_alpha_storage_bad_block_count_demo ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) --disk tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_bad_block_count_demo" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_bad_block_count_demo" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline-guest-kernel_alpha_storage_bad_block_count_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO)
	@expected='KMVBX'; \
	echo "=== guest_kernel_alpha_storage_bad_block_count_demo (pipeline) ==="; \
	output="$$(timeout $(PIPELINE_GUEST_TEST_TIMEOUT) ./$(TARGET) --backend pipeline --disk tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_bad_block_count_demo (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_bad_block_count_demo (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-guest-kernel_alpha_storage_lba_range_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO)
	@expected='KMVLX'; \
	echo "=== guest_kernel_alpha_storage_lba_range_demo ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) --disk tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_lba_range_demo" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_lba_range_demo" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline-guest-kernel_alpha_storage_lba_range_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO)
	@expected='KMVLX'; \
	echo "=== guest_kernel_alpha_storage_lba_range_demo (pipeline) ==="; \
	output="$$(timeout $(PIPELINE_GUEST_TEST_TIMEOUT) ./$(TARGET) --backend pipeline --disk tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_lba_range_demo (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_lba_range_demo (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-guest-kernel_alpha_storage_bad_command_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO)
	@expected='KMVCX'; \
	echo "=== guest_kernel_alpha_storage_bad_command_demo ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) --disk tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_bad_command_demo" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_bad_command_demo" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline-guest-kernel_alpha_storage_bad_command_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO)
	@expected='KMVCX'; \
	echo "=== guest_kernel_alpha_storage_bad_command_demo (pipeline) ==="; \
	output="$$(timeout $(PIPELINE_GUEST_TEST_TIMEOUT) ./$(TARGET) --backend pipeline --disk tests/data/storage_basic.txt $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_storage_bad_command_demo (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_storage_bad_command_demo (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-guest-kernel_alpha_plic_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO)
	@expected='KMVPX'; \
	echo "=== guest_kernel_alpha_plic_not_ready_demo ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_plic_not_ready_demo" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_plic_not_ready_demo" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline-guest-kernel_alpha_plic_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO)
	@expected='KMVPX'; \
	echo "=== guest_kernel_alpha_plic_not_ready_demo (pipeline) ==="; \
	output="$$(timeout $(PIPELINE_GUEST_TEST_TIMEOUT) ./$(TARGET) --backend pipeline $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_plic_not_ready_demo (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_plic_not_ready_demo (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-guest-kernel_alpha_timer_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO)
	@expected='KMVPETX'; \
	echo "=== guest_kernel_alpha_timer_not_ready_demo ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_timer_not_ready_demo" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_timer_not_ready_demo" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

test-pipeline-guest-kernel_alpha_timer_not_ready_demo: FORCE $(TARGET) $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO)
	@expected='KMVPETX'; \
	echo "=== guest_kernel_alpha_timer_not_ready_demo (pipeline) ==="; \
	output="$$(timeout $(PIPELINE_GUEST_TEST_TIMEOUT) ./$(TARGET) --backend pipeline $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO) 2>&1)"; \
	status=$$?; \
	if [ $$status -eq 124 ]; then \
		echo "Test timed out: guest_kernel_alpha_timer_not_ready_demo (pipeline)" >&2; \
		exit 1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '%s\n' "$$output" >&2; \
		exit $$status; \
	fi; \
	if [ "$$output" != "$$expected" ]; then \
		echo "Unexpected output for guest_kernel_alpha_timer_not_ready_demo (pipeline)" >&2; \
		printf 'Expected: %s\n' "$$expected" >&2; \
		printf 'Actual: %s\n' "$$output" >&2; \
		exit 1; \
	fi; \
	printf '%s\n' "$$output"

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

clean:
	rm -f $(TARGET) tests/asm/*.elf tests/asm/*.bin $(UNIT_TEST_BINS) $(HOST_TEST_BINS) $(SUPERVISOR_RUNTIME_UNIT_OBJS) $(SUPERVISOR_RUNTIME_UNIT_DEPS) $(KERNEL_RUNTIME_UNIT_OBJS) $(KERNEL_RUNTIME_UNIT_DEPS) $(KERNEL_ALPHA_COMMON_UNIT_OBJS) $(KERNEL_ALPHA_COMMON_UNIT_DEPS) $(KERNEL_ALPHA_STORAGE_UNIT_OBJS) $(KERNEL_ALPHA_STORAGE_UNIT_DEPS) $(KERNEL_ALPHA_INTERRUPT_UNIT_OBJS) $(KERNEL_ALPHA_INTERRUPT_UNIT_DEPS) $(GUEST_SUPERVISOR_DEMO) $(GUEST_KERNEL_ALPHA_DEMO) $(GUEST_KERNEL_ALPHA_FAULT_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_NO_MEDIA_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_NOT_READY_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_MAGIC_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_LBA_RANGE_DEMO) $(GUEST_KERNEL_ALPHA_STORAGE_BAD_COMMAND_DEMO) $(GUEST_KERNEL_ALPHA_PLIC_NOT_READY_DEMO) $(GUEST_KERNEL_ALPHA_TIMER_NOT_READY_DEMO)
	find src guest -type f \( -name '*.o' -o -name '*.d' \) -delete

-include $(DEPS)
tests/unit/supervisor_runtime_test.o: tests/unit/supervisor_runtime.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/supervisor_runtime_guest.o: guest/kernel/supervisor_runtime.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/supervisor_runtime: $(SUPERVISOR_RUNTIME_UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

tests/unit/kernel_runtime_test.o: tests/unit/kernel_runtime.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_runtime_guest.o: guest/kernel/kernel_runtime.c
	$(CC) $(CFLAGS) -Iguest/include -Iinclude -MMD -MP -c -o $@ $<

tests/unit/kernel_runtime: $(KERNEL_RUNTIME_UNIT_OBJS)
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
