#!/usr/bin/env bash
set -euo pipefail

# Build isolated evidence assets; existing xv6/core objects must already be built.
repo=$(git rev-parse --show-toplevel)
out=${1:?usage: bash u4-build.sh absolute-output-directory}
mkdir -p "$out"
out=$(realpath "$out")
xv6="$repo/myCPU/external/xv6-riscv"
prefix=${XV6_TOOLPREFIX:-riscv64-unknown-elf-}
march=${XV6_EVIDENCE_MARCH:-rv64gc}
"${prefix}gcc" -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2 \
  "-march=$march" -mabi=lp64 -mcmodel=medany -ffreestanding -fno-common \
  -nostdlib -fno-builtin -fno-stack-protector -fno-pie -no-pie -I"$xv6" \
  -c "$repo/myCPU/tests/guest/system_software_fork_evidence.c" -o "$out/forkev.o"
"${prefix}ld" -z max-page-size=4096 -T "$xv6/user/user.ld" -o "$out/_forkev" \
  "$out/forkev.o" "$xv6/user/ulib.o" "$xv6/user/usys.o" \
  "$xv6/user/printf.o" "$xv6/user/umalloc.o"
"${prefix}nm" -n "$out/_forkev" > "$out/forkev.sym"
cp "$xv6/user/_init" "$xv6/user/_sh" "$xv6/user/_forktest" "$out/"
cp "$xv6/README" "$out/README"
(cd "$out" && "$xv6/mkfs/mkfs" fs.img README _init _sh _forktest _forkev)

gdb -nx -batch "$xv6/kernel/kernel" \
  -ex 'printf "proc %lu\n", (unsigned long)&proc' \
  -ex 'printf "cpus %lu\n", (unsigned long)&cpus' \
  -ex 'printf "cpu_proc %lu\n", (unsigned long)&((struct cpu*)0)->proc' \
  -ex 'printf "kmem %lu\n", (unsigned long)&kmem' \
  -ex 'printf "sched %lu\n", (unsigned long)&sched' \
  -ex 'printf "proc_size %lu\n", (unsigned long)sizeof(struct proc)' \
  -ex 'printf "nproc %lu\n", (unsigned long)(sizeof(proc)/sizeof(proc[0]))' \
  -ex 'printf "pid %lu\n", (unsigned long)&((struct proc*)0)->pid' \
  -ex 'printf "state %lu\n", (unsigned long)&((struct proc*)0)->state' \
  -ex 'printf "parent %lu\n", (unsigned long)&((struct proc*)0)->parent' \
  -ex 'printf "pagetable %lu\n", (unsigned long)&((struct proc*)0)->pagetable' \
  -ex 'printf "xstate %lu\n", (unsigned long)&((struct proc*)0)->xstate' \
  -ex 'printf "freelist %lu\n", (unsigned long)&kmem.freelist-(unsigned long)&kmem' \
  -ex 'printf "run_next %lu\n", (unsigned long)&((struct run*)0)->next' \
  -ex 'printf "unused %lu\n", (unsigned long)UNUSED' \
  -ex 'printf "zombie %lu\n", (unsigned long)ZOMBIE' > "$out/layout.txt"

make -C "$repo/myCPU" tests/host/system_software_fork_evidence
printf 'Assets: %s\n' "$out"
printf 'Run: timeout 1200 %s %s %s %s\n' \
  "$repo/myCPU/tests/host/system_software_fork_evidence" \
  "$xv6/kernel/kernel" "$out/fs.img" "$out/layout.txt"
