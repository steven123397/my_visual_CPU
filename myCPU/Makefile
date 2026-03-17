CC       = gcc
CXX      = g++
CFLAGS   = -O2 -Wall -Wextra -g -std=c11
CXXFLAGS = -O2 -Wall -Wextra -g -std=c++17
C_SRCS   = src/memory.c src/decode.c src/elf_loader.c
CPP_SRCS = src/cpu.cpp src/trap.cpp src/main.cpp src/arch/core_state.cpp src/arch/csr_file.cpp src/mem/ram.cpp src/mem/bus.cpp src/platform/machine.cpp
OBJS     = $(C_SRCS:.c=.o) $(CPP_SRCS:.cpp=.o)
TARGET   = mycpu
ASM_TESTS = hello sum control_flow csr_trap
ASM_ELFS  = $(ASM_TESTS:%=tests/asm/%.elf)
TEST_TIMEOUT ?= 2s

# RISC-V toolchain (adjust prefix if needed)
RV_CC   = riscv64-unknown-elf-gcc
RV_AS   = riscv64-unknown-elf-as
RV_LD   = riscv64-unknown-elf-ld
RV_OBJCOPY = riscv64-unknown-elf-objcopy
RV_FLAGS = -march=rv64im -mabi=lp64
RV_ELF_LDFLAGS = -nostdlib -Wl,-Ttext-segment=0x80000000
RV_REQUIRED_TOOLS = $(RV_CC) $(RV_OBJCOPY)

.PHONY: all clean test check-riscv-toolchain $(ASM_TESTS:%=test-%)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Build ELF test binaries
tests/asm/%.elf: tests/asm/%.S | check-riscv-toolchain
	$(RV_CC) $(RV_FLAGS) $(RV_ELF_LDFLAGS) -o $@ $<

# Build flat binary from ELF
tests/asm/%.bin: tests/asm/%.elf | check-riscv-toolchain
	$(RV_OBJCOPY) -O binary $< $@

test: $(TARGET) $(ASM_ELFS) $(ASM_TESTS:%=test-%)

test-%: $(TARGET) tests/asm/%.elf
	@case "$*" in \
		hello) expected='Hello, RISC-V!' ;; \
		sum) expected='55' ;; \
		control_flow) expected='OK' ;; \
		csr_trap) expected='CSR' ;; \
		*) echo "Unknown asm test: $*" >&2; exit 1 ;; \
	esac; \
	echo "=== $* ==="; \
	output="$$(timeout $(TEST_TIMEOUT) ./$(TARGET) tests/asm/$*.elf 2>&1)"; \
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
	rm -f $(TARGET) $(OBJS) tests/asm/*.elf tests/asm/*.bin
