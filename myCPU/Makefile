CC       = gcc
CXX      = g++
CFLAGS   = -O2 -Wall -Wextra -g -std=c11
CXXFLAGS = -O2 -Wall -Wextra -g -std=c++17
C_SRCS   = src/memory.c src/decode.c
CPP_SRCS = src/cpu.cpp src/trap.cpp src/main.cpp src/arch/core_state.cpp src/arch/csr_file.cpp src/exec/system_ops.cpp src/exec/memory_ops.cpp src/exec/control_flow_ops.cpp src/exec/integer_ops.cpp src/exec/functional_backend.cpp src/exec/pipeline_backend.cpp src/isa/execution_context.cpp src/isa/instruction_semantics.cpp src/mem/ram.cpp src/mem/bus.cpp src/mem/address_space.cpp src/devices/uart16550.cpp src/devices/clint.cpp src/devices/plic.cpp src/devices/simple_storage.cpp src/loader/elf_loader.cpp src/loader/binary_loader.cpp src/platform/machine.cpp
OBJS     = $(C_SRCS:.c=.o) $(CPP_SRCS:.cpp=.o)
TARGET   = mycpu
HOST_TEST_BINS = tests/host/instruction_semantics_smoke tests/host/address_space_faults_smoke tests/host/pipeline_backend_smoke tests/host/backend_differential_smoke
HOST_TEST_LINK_OBJS = $(filter-out src/main.o,$(OBJS))
ASM_TESTS = hello sum control_flow csr_trap timer_interrupt mtvec_modes trap_state exception_traps access_faults loads_signed_unsigned alu_word branches_signed_unsigned muldiv fence_noop privilege_transitions sret_transitions supervisor_exception_delegation supervisor_timer_interrupt csr_access_control counteren_access_control instret_counting machine_counter_csrs interrupt_csr_views delegation_edge_cases plic_machine_external_interrupt plic_supervisor_external_interrupt storage_device_basic supervisor_platform_smoke sv39_basic sv39_page_fault sv39_edge_faults sv39_sum_mxr sv39_tlb_flush sv39_tlb_ad_bits csr_semantic_consistency clint_split_access
ASM_ELFS  = $(ASM_TESTS:%=tests/asm/%.elf)
FLAT_BIN_TESTS = hello
GUEST_SUPERVISOR_DEMO = guest/supervisor_demo.elf
GUEST_SRCS = guest/lib/platform.S guest/lib/trap_runtime.S guest/kernel/console.c guest/kernel/memory.c guest/kernel/panic.c guest/kernel/pmm.c guest/kernel/runtime_context.c guest/kernel/storage.c guest/kernel/supervisor_demo_smoke.c guest/kernel/timer.c guest/kernel/trap.c guest/kernel/user_program.c guest/kernel/user_program_smoke.c guest/kernel/user_task.c guest/kernel/user_task_bootstrap.c guest/kernel/vm.c guest/supervisor_demo/start.S guest/supervisor_demo/main.c
GUEST_OBJS = $(GUEST_SRCS:.S=.o)
GUEST_OBJS := $(GUEST_OBJS:.c=.o)
DEPS     = $(OBJS:.o=.d) $(GUEST_OBJS:.o=.d)
TEST_TIMEOUT ?= 2s

# RISC-V toolchain (adjust prefix if needed)
RV_CC   = riscv64-unknown-elf-gcc
RV_OBJCOPY = riscv64-unknown-elf-objcopy
RV_FLAGS = -march=rv64im_zicsr -mabi=lp64
RV_ELF_LDFLAGS = -nostdlib -Wl,-Ttext-segment=0x80000000
GUEST_RV_CFLAGS = $(RV_FLAGS) -mcmodel=medany -mno-relax -msmall-data-limit=0 -ffreestanding -fno-builtin -fno-stack-protector -fno-pie -MMD -MP -g -O2 -Wall -Wextra
GUEST_RV_LDFLAGS = -nostdlib -nostartfiles -static -no-pie -Wl,-T,guest/supervisor_demo/link.ld
RV_REQUIRED_TOOLS = $(RV_CC) $(RV_OBJCOPY)

.PHONY: all clean test test-pipeline check-riscv-toolchain FORCE test-backend-cli test-host-instruction_semantics test-host-address_space_faults test-host-pipeline_backend test-host-backend_differential

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

# Build flat binary from ELF
tests/asm/%.bin: tests/asm/%.elf | check-riscv-toolchain
	$(RV_OBJCOPY) -O binary $< $@

test: $(TARGET) $(ASM_ELFS) $(FLAT_BIN_TESTS:%=tests/asm/%.bin) $(GUEST_SUPERVISOR_DEMO) $(ASM_TESTS:%=test-%) test-guest-supervisor_demo $(FLAT_BIN_TESTS:%=test-flat-%) test-backend-cli test-host-instruction_semantics test-host-address_space_faults test-host-pipeline_backend test-host-backend_differential

test-pipeline: $(TARGET) $(ASM_ELFS) $(ASM_TESTS:%=test-pipeline-%) test-host-pipeline_backend test-host-backend_differential

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

test-backend-cli: FORCE $(TARGET) tests/asm/hello.elf
	@echo "=== backend_cli ==="
	@sh tests/host/backend_cli.sh $(TARGET) tests/asm/hello.elf $(TEST_TIMEOUT)

tests/host/%: tests/host/%.cpp $(HOST_TEST_LINK_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(HOST_TEST_LINK_OBJS)

test-host-instruction_semantics: FORCE $(HOST_TEST_BINS)
	@echo "=== host_instruction_semantics ==="
	@./tests/host/instruction_semantics_smoke

test-host-address_space_faults: FORCE $(HOST_TEST_BINS)
	@echo "=== host_address_space_faults ==="
	@./tests/host/address_space_faults_smoke

test-host-pipeline_backend: FORCE $(HOST_TEST_BINS)
	@echo "=== host_pipeline_backend ==="
	@./tests/host/pipeline_backend_smoke

test-host-backend_differential: FORCE $(HOST_TEST_BINS)
	@echo "=== host_backend_differential ==="
	@./tests/host/backend_differential_smoke

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
	rm -f $(TARGET) tests/asm/*.elf tests/asm/*.bin $(GUEST_SUPERVISOR_DEMO) $(HOST_TEST_BINS)
	find src guest -type f \( -name '*.o' -o -name '*.d' \) -delete

-include $(DEPS)
