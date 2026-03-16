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

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Build ELF test binaries
tests/asm/%.elf: tests/asm/%.S
	$(RV_CC) $(RV_FLAGS) -nostdlib -Ttext=0x80000000 -o $@ $<

# Build flat binary from ELF
tests/asm/%.bin: tests/asm/%.elf
	$(RV_OBJCOPY) -O binary $< $@

test: $(TARGET) tests/asm/hello.elf tests/asm/sum.elf
	@echo "=== hello ==="
	./$(TARGET) tests/asm/hello.elf
	@echo "=== sum (expect: 55) ==="
	./$(TARGET) tests/asm/sum.elf

clean:
	rm -f $(TARGET) $(OBJS) tests/asm/*.elf tests/asm/*.bin
