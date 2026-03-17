CC       = gcc
CXX      = g++
CFLAGS   = -O2 -Wall -Wextra -g -std=c11
CXXFLAGS = -O2 -Wall -Wextra -g -std=c++17
C_SRCS   = src/cpu.c src/memory.c src/decode.c src/trap.c src/elf_loader.c
CPP_SRCS = src/main.cpp src/mem/ram.cpp src/mem/bus.cpp src/platform/machine.cpp
OBJS     = $(C_SRCS:.c=.o) $(CPP_SRCS:.cpp=.o)
TARGET   = mycpu

# RISC-V toolchain (adjust prefix if needed)
RV_CC   = riscv64-unknown-elf-gcc
RV_AS   = riscv64-unknown-elf-as
RV_LD   = riscv64-unknown-elf-ld
RV_OBJCOPY = riscv64-unknown-elf-objcopy
RV_FLAGS = -march=rv64im -mabi=lp64
RV_ELF_LDFLAGS = -nostdlib -Wl,-Ttext-segment=0x80000000
RV_REQUIRED_TOOLS = $(RV_CC) $(RV_OBJCOPY)

.PHONY: all clean test check-riscv-toolchain

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

test: $(TARGET) tests/asm/hello.elf tests/asm/sum.elf
	@echo "=== hello ==="
	./$(TARGET) tests/asm/hello.elf
	@echo "=== sum (expect: 55) ==="
	./$(TARGET) tests/asm/sum.elf

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
