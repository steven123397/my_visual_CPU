#!/usr/bin/env python3
import contextlib
import importlib.util
import io
import json
import os
import pathlib
import re
import subprocess
import tempfile
import textwrap
import unittest
import unittest.mock


MYCPU_DIR = pathlib.Path(__file__).resolve().parents[2]
MODULE_PATH = pathlib.Path(__file__).resolve().parents[2] / "workloads" / "run_debug_cli_probe.py"
MODULE_SPEC = importlib.util.spec_from_file_location("run_debug_cli_probe", MODULE_PATH)
if MODULE_SPEC is None or MODULE_SPEC.loader is None:
    raise RuntimeError(f"failed to load module spec for {MODULE_PATH}")
PROBE = importlib.util.module_from_spec(MODULE_SPEC)
MODULE_SPEC.loader.exec_module(PROBE)
DEFAULT_LINUX_PROTO_RUNTIME_IMAGE = (
    MYCPU_DIR / "external" / "linux-riscv" / "arch" / "riscv" / "boot" / "Image"
)
DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS = (
    MYCPU_DIR / "workloads" / "linux_proto" / "rootfs.ext4"
)
CURATED_ALPINE_GUEST_RISCV_ISA = "rv64imafdc_zicsr_zifencei"
CURATED_ALPINE_PROC_CPUINFO_ISA = "rv64imafdc_zicntr_zicsr_zifencei_zihpm"
CURATED_ALPINE_AT_HWCAP = 0x112D
CURATED_ALPINE_AT_HWCAP_AUXV_LINE = f"{16:016x} {CURATED_ALPINE_AT_HWCAP:016x}"
CURATED_ALPINE_BASE_ISA_EXTENSIONS = set("acdfim")
LINUX_DISTRO_RUNTIME_PROFILES = {
    "filesystem_consistency": [
        ("cat /etc/os-release", "ID=alpine"),
        ('printf "cwd:"; pwd', "cwd:/"),
        (
            "mkdir -p /tmp/mycpu-smoke; test -d /tmp/mycpu-smoke; "
            'printf "mkdir-status:"; printf "%s" "$?"',
            "mkdir-status:0",
        ),
        (
            "printf '\\150\\145\\154\\154\\157' >/tmp/mycpu-smoke/file; "
            "cat /tmp/mycpu-smoke/file",
            "hello",
        ),
        (
            "printf '\\167\\157\\162\\154\\144' >>/tmp/mycpu-smoke/file; "
            "cat /tmp/mycpu-smoke/file",
            "helloworld",
        ),
        ("wc -c </tmp/mycpu-smoke/file", "10"),
        (
            "rm /tmp/mycpu-smoke/file; test ! -e /tmp/mycpu-smoke/file; "
            'printf "delete-status:"; printf "%s" "$?"',
            "delete-status:0",
        ),
        (
            "rmdir /tmp/mycpu-smoke; test ! -e /tmp/mycpu-smoke; "
            'printf "rmdir-status:"; printf "%s" "$?"',
            "rmdir-status:0",
        ),
        ("printf '\\163\\164\\151\\154\\154\\055\\141\\154\\151\\166\\145'", "still-alive"),
    ],
    "tty_login_probe": [
        ("cat /etc/os-release", "ID=alpine"),
        (
            "for x in getty login stty setsid tty; do "
            'command -v "$x" >/dev/null 2>&1 && printf "%s=present\\n" "$x" || '
            'printf "%s=missing\\n" "$x"; '
            "done",
            "tty=present",
        ),
        ('printf "tty-path:"; tty || true', "tty-path:"),
        ("stty -a 2>&1 | sed -n '1p'", "speed"),
        (
            "setsid sh -c 'printf setsid-status:; tty || true' 2>&1",
            "setsid-status:",
        ),
        (
            "setsid /sbin/getty -n -l /bin/sh -L 115200 ttyS0 vt100",
            "~ # ",
        ),
        ('printf "getty-roundtrip-ok"', "getty-roundtrip-ok"),
    ],
    "process_control": [
        ("cat /etc/os-release", "ID=alpine"),
        ("sleep 1; printf 'sleep-ok'", "sleep-ok"),
        (
            "sh -c 'sleep 1; exit 7' & pid=$!; wait $pid; "
            'printf "wait-status:%s" "$?"',
            "wait-status:7",
        ),
        (
            "trap 'printf trap-hit' TERM; kill -TERM $$",
            "trap-hit",
        ),
        (
            "false || printf 'or-ok'; true && printf ':and-ok'; "
            "false; printf ':status:%s' \"$?\"",
            "or-ok:and-ok:status:1",
        ),
    ],
    "filesystem_persistence": [
        ("cat /etc/os-release", "ID=alpine"),
        (
            "rm -rf /root/mycpu-persist; mkdir -p /root/mycpu-persist/sub; "
            'printf "persist-dir-status:%s" "$?"',
            "persist-dir-status:0",
        ),
        (
            "printf alpha >/root/mycpu-persist/file; "
            "sync /root/mycpu-persist/file 2>/dev/null || sync; "
            "cat /root/mycpu-persist/file",
            "alpha",
        ),
        (
            "printf beta >/root/mycpu-persist/tmp; "
            "mv -f /root/mycpu-persist/tmp /root/mycpu-persist/file; "
            "cat /root/mycpu-persist/file",
            "beta",
        ),
        (
            "find /root/mycpu-persist -maxdepth 2 -type f | sort",
            "/root/mycpu-persist/file",
        ),
        (
            "dd if=/dev/zero of=/root/mycpu-persist/large bs=1024 count=64 2>/dev/null; "
            "sync /root/mycpu-persist/large 2>/dev/null || sync; "
            'printf "large-size:"; wc -c </root/mycpu-persist/large',
            "large-size:65536",
        ),
        (
            "rm -rf /root/mycpu-persist; sync; "
            'printf "persist-cleanup:%s" "$?"',
            "persist-cleanup:0",
        ),
    ],
    "fs_state_guardrail": [
        ("cat /etc/os-release", "ID=alpine"),
        ("awk 'BEGIN{print sqrt(2)}'", "1.41421"),
        ("sleep 1; printf 'timer-roundtrip-ok'", "timer-roundtrip-ok"),
        (
            "sh -c 'awk \"BEGIN{printf \\\"%.1f\\\\n\\\", 2.25+0.25}\"; exit 7' & pid=$!; wait $pid; "
            'printf " child-status:%s" "$?"',
            "2.5 child-status:7",
        ),
        (
            "awk 'BEGIN{printf \"%.1f %.1f\\n\", 2.25+0.25, sqrt(2)*sqrt(2)}'",
            "2.5 2.0",
        ),
    ],
}
CURATED_LINUX_DISTRO_RUNTIME_MATRIX = {
    "alpine": {
        "image_env": "MYCPU_LINUX_DISTRO_CURATED_IMAGE",
        "rootfs_env": "MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS",
        "bootargs_env": "MYCPU_LINUX_DISTRO_CURATED_ALPINE_BOOTARGS",
        "prompt_env": "MYCPU_LINUX_DISTRO_CURATED_ALPINE_PROMPT",
        "bootargs": (
            "console=ttyS0,115200 earlycon=ns16550a,mmio,0x10000000 "
            "root=/dev/vda rw rootfstype=ext4 rootwait init=/bin/sh loglevel=8 ignore_loglevel"
        ),
        "prompt": "~ # ",
        "command": "cat /etc/os-release",
        "expected": "ID=alpine",
        "validated_profiles": (
            "shell",
            "filesystem_consistency",
            "tty_login_probe",
            "process_control",
            "filesystem_persistence",
            "fs_state_guardrail",
        ),
    },
    "debian": {
        "image_env": "MYCPU_LINUX_DISTRO_CURATED_IMAGE",
        "rootfs_env": "MYCPU_LINUX_DISTRO_CURATED_DEBIAN_ROOTFS",
        "bootargs_env": "MYCPU_LINUX_DISTRO_CURATED_DEBIAN_BOOTARGS",
        "prompt_env": "MYCPU_LINUX_DISTRO_CURATED_DEBIAN_PROMPT",
        "bootargs": (
            "console=ttyS0,115200 earlycon=ns16550a,mmio,0x10000000 "
            "root=/dev/vda rw rootfstype=ext4 rootwait init=/mycpu-debian-init "
            "loglevel=8 ignore_loglevel"
        ),
        "prompt": "mycpu-debian# ",
        "command": "cat /etc/os-release",
        "expected": "ID=debian",
        "validated_profiles": ("shell",),
    },
}


def build_linux_dummy_flat_image(temp_dir: pathlib.Path) -> pathlib.Path:
    asm_path = temp_dir / "linux_dummy.S"
    elf_path = temp_dir / "linux_dummy.elf"
    bin_path = temp_dir / "linux_dummy.bin"
    asm_path.write_text(
        textwrap.dedent(
            """\
            .section .text
            .globl _start
        _start:
            li t0, 0x84000000
            addi t1, a1, 0
            li t2, 0x80201000
        loop:
            lw t3, 0(t1)
            lw t4, 0(t0)
            sw t3, 0(t2)
            sw t4, 4(t2)
            j loop
            """
        )
    )

    compile_proc = subprocess.run(
        [
            "riscv64-unknown-elf-gcc",
            "-nostdlib",
            "-static",
            "-march=rv64ima",
            "-mabi=lp64",
            "-Ttext=0x80200000",
            "-o",
            str(elf_path),
            str(asm_path),
        ],
        cwd=MYCPU_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if compile_proc.returncode != 0:
        raise AssertionError(compile_proc.stderr)

    objcopy_proc = subprocess.run(
        [
            "riscv64-unknown-elf-objcopy",
            "-O",
            "binary",
            str(elf_path),
            str(bin_path),
        ],
        cwd=MYCPU_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if objcopy_proc.returncode != 0:
        raise AssertionError(objcopy_proc.stderr)

    return bin_path


def build_l1d_probe_flat_image(temp_dir: pathlib.Path) -> pathlib.Path:
    asm_path = temp_dir / "l1d_probe.S"
    elf_path = temp_dir / "l1d_probe.elf"
    bin_path = temp_dir / "l1d_probe.bin"
    asm_path.write_text(
        textwrap.dedent(
            """\
            .section .text
            .globl _start
        _start:
            la t0, data
            lw t1, 0(t0)
            lw t2, 0(t0)
            sw t2, 4(t0)
            li a7, 93
            ecall

            .align 3
        data:
            .word 0x11223344
            .word 0
            """
        )
    )

    compile_proc = subprocess.run(
        [
            "riscv64-unknown-elf-gcc",
            "-nostdlib",
            "-static",
            "-march=rv64ima",
            "-mabi=lp64",
            "-Ttext=0x80000000",
            "-o",
            str(elf_path),
            str(asm_path),
        ],
        cwd=MYCPU_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if compile_proc.returncode != 0:
        raise AssertionError(compile_proc.stderr)

    objcopy_proc = subprocess.run(
        [
            "riscv64-unknown-elf-objcopy",
            "-O",
            "binary",
            str(elf_path),
            str(bin_path),
        ],
        cwd=MYCPU_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if objcopy_proc.returncode != 0:
        raise AssertionError(objcopy_proc.stderr)

    return bin_path


def build_translation_plan_probe_flat_image(temp_dir: pathlib.Path) -> pathlib.Path:
    asm_path = temp_dir / "translation_plan_probe.S"
    elf_path = temp_dir / "translation_plan_probe.elf"
    bin_path = temp_dir / "translation_plan_probe.bin"
    asm_path.write_text(
        textwrap.dedent(
            """\
            .section .text
            .globl _start
        _start:
            la t0, data
        loop:
            addi t1, t1, 1
            lw t2, 0(t0)
            j loop

            .align 3
        data:
            .word 0x11223344
            """
        )
    )

    compile_proc = subprocess.run(
        [
            "riscv64-unknown-elf-gcc",
            "-nostdlib",
            "-static",
            "-march=rv64ima",
            "-mabi=lp64",
            "-Ttext=0x80000000",
            "-o",
            str(elf_path),
            str(asm_path),
        ],
        cwd=MYCPU_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if compile_proc.returncode != 0:
        raise AssertionError(compile_proc.stderr)

    objcopy_proc = subprocess.run(
        [
            "riscv64-unknown-elf-objcopy",
            "-O",
            "binary",
            str(elf_path),
            str(bin_path),
        ],
        cwd=MYCPU_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if objcopy_proc.returncode != 0:
        raise AssertionError(objcopy_proc.stderr)

    return bin_path


def build_linux_fcsr_syscall_roundtrip_probe(temp_dir: pathlib.Path) -> pathlib.Path:
    source_path = temp_dir / "linux_fcsr_syscall_roundtrip_probe.c"
    elf_path = temp_dir / "linux_fcsr_syscall_roundtrip_probe.elf"
    source_path.write_text(
        textwrap.dedent(
            """\
            typedef unsigned long uint64_t;
            typedef long int64_t;

            struct timespec {
                long tv_sec;
                long tv_nsec;
            };

            static inline long linux_syscall0(long number) {
                register long a7 __asm__("a7") = number;
                register long a0 __asm__("a0");
                __asm__ volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
                return a0;
            }

            static inline long linux_syscall1(long number, long arg0) {
                register long a0 __asm__("a0") = arg0;
                register long a7 __asm__("a7") = number;
                __asm__ volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
                return a0;
            }

            static inline long linux_syscall2(long number, long arg0, long arg1) {
                register long a0 __asm__("a0") = arg0;
                register long a1 __asm__("a1") = arg1;
                register long a7 __asm__("a7") = number;
                __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
                return a0;
            }

            static inline long linux_syscall3(long number, long arg0, long arg1, long arg2) {
                register long a0 __asm__("a0") = arg0;
                register long a1 __asm__("a1") = arg1;
                register long a2 __asm__("a2") = arg2;
                register long a7 __asm__("a7") = number;
                __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
                return a0;
            }

            static inline long linux_write(int fd, const char *buffer, unsigned long size) {
                return linux_syscall3(64, fd, (long)buffer, (long)size);
            }

            static inline long linux_exit(int code) {
                return linux_syscall1(93, code);
            }

            static inline long linux_nanosleep(const struct timespec *request, struct timespec *remain) {
                return linux_syscall2(101, (long)request, (long)remain);
            }

            static inline long linux_getpid(void) {
                return linux_syscall0(172);
            }

            static inline uint64_t read_fcsr(void) {
                uint64_t value;
                __asm__ volatile("frcsr %0" : "=r"(value));
                return value;
            }

            static inline void write_fcsr(uint64_t value) {
                __asm__ volatile("fscsr zero, %0" :: "r"(value));
            }

            static inline uint64_t read_f1_bits(void) {
                uint64_t value;
                __asm__ volatile("fmv.x.d %0, f1" : "=r"(value));
                return value;
            }

            static inline void write_f1_bits(uint64_t value) {
                __asm__ volatile("fmv.d.x f1, %0" :: "r"(value));
            }

            static inline double convert_dynamic_rounding(int64_t value) {
                double out;
                __asm__ volatile("fcvt.d.l %0, %1, dyn" : "=f"(out) : "r"(value));
                return out;
            }

            static void write_text(const char *text) {
                unsigned long size = 0;
                while (text[size] != '\\0') {
                    size++;
                }
                (void)linux_write(1, text, size);
            }

            static void write_hex_u64(uint64_t value) {
                static const char digits[] = "0123456789abcdef";
                char out[19];
                out[0] = '0';
                out[1] = 'x';
                for (int i = 0; i < 16; ++i) {
                    const int shift = 60 - (i * 4);
                    out[2 + i] = digits[(value >> shift) & 0xf];
                }
                out[18] = '\\0';
                write_text(out);
            }

            static void write_fcsr_mismatch(
                const char *label,
                uint64_t before,
                uint64_t after_convert,
                uint64_t after_getpid,
                uint64_t after_syscall,
                uint64_t after_getpid_f1,
                uint64_t after_syscall_f1) {
                write_text("mycpu-fcsr-syscall-roundtrip:");
                write_text(label);
                write_text(" before=");
                write_hex_u64(before);
                write_text(" after-convert=");
                write_hex_u64(after_convert);
                write_text(" after-getpid=");
                write_hex_u64(after_getpid);
                write_text(" after-syscall=");
                write_hex_u64(after_syscall);
                write_text(" after-getpid-f1=");
                write_hex_u64(after_getpid_f1);
                write_text(" after-syscall-f1=");
                write_hex_u64(after_syscall_f1);
                write_text("\\n");
            }

            int main(void) {
                const struct timespec request = {0, 1000000};
                const uint64_t kFrmRdn = 2UL << 5;
                const uint64_t kNx = 1UL;
                const int64_t kInexact = 9007199254740993LL;
                const uint64_t kF1Marker = 0x0123456789abcdefUL;
                volatile double rounded = 0.0;
                uint64_t before = 0;
                uint64_t after_convert = 0;
                uint64_t after_getpid = 0;
                uint64_t after_syscall = 0;
                uint64_t after_getpid_f1 = 0;
                uint64_t after_syscall_f1 = 0;

                write_fcsr(kFrmRdn);
                before = read_fcsr();
                if (before != kFrmRdn) {
                    write_text("mycpu-fcsr-syscall-roundtrip:frm-program-failed\\n");
                    return 11;
                }

                rounded = convert_dynamic_rounding(kInexact);
                (void)rounded;
                after_convert = read_fcsr();
                if (after_convert != (kFrmRdn | kNx)) {
                    write_text("mycpu-fcsr-syscall-roundtrip:convert-flags-failed\\n");
                    return 12;
                }
                write_f1_bits(kF1Marker);

                if (linux_getpid() <= 0) {
                    write_text("mycpu-fcsr-syscall-roundtrip:getpid-failed\\n");
                    return 13;
                }
                after_getpid = read_fcsr();
                after_getpid_f1 = read_f1_bits();
                if (after_getpid != (kFrmRdn | kNx)) {
                    write_fcsr_mismatch("post-getpid-fcsr-mismatch", before, after_convert, after_getpid, 0, after_getpid_f1, 0);
                    return 15;
                }
                if (after_getpid_f1 != kF1Marker) {
                    write_fcsr_mismatch("post-getpid-fpr-mismatch", before, after_convert, after_getpid, 0, after_getpid_f1, 0);
                    return 15;
                }
                if (linux_nanosleep(&request, (struct timespec *)0) != 0) {
                    write_text("mycpu-fcsr-syscall-roundtrip:nanosleep-failed\\n");
                    return 14;
                }

                after_syscall = read_fcsr();
                after_syscall_f1 = read_f1_bits();
                if (after_syscall != (kFrmRdn | kNx)) {
                    write_fcsr_mismatch("post-syscall-fcsr-mismatch", before, after_convert, after_getpid, after_syscall, after_getpid_f1, after_syscall_f1);
                    return 15;
                }
                if (after_syscall_f1 != kF1Marker) {
                    write_fcsr_mismatch("post-syscall-fpr-mismatch", before, after_convert, after_getpid, after_syscall, after_getpid_f1, after_syscall_f1);
                    return 15;
                }

                write_text("mycpu-fcsr-syscall-roundtrip:ok\\n");
                return 0;
            }

            void _start(void) {
                const int rc = main();
                (void)linux_exit(rc);
                for (;;) {
                }
            }
            """
        )
    )

    compile_proc = subprocess.run(
        [
            "riscv64-unknown-elf-gcc",
            "-nostdlib",
            "-static",
            "-march=rv64imafdc",
            "-mabi=lp64d",
            "-O2",
            "-Wl,-e,_start",
            "-o",
            str(elf_path),
            str(source_path),
        ],
        cwd=MYCPU_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if compile_proc.returncode != 0:
        raise AssertionError(compile_proc.stderr)

    return elf_path


def debug_cli_roundtrip(proc: subprocess.Popen[str], command: dict) -> dict:
    if proc.stdin is None or proc.stdout is None:
        raise AssertionError("debug CLI pipes were not created")

    proc.stdin.write(json.dumps(command) + "\n")
    proc.stdin.flush()
    line = proc.stdout.readline()
    if not line:
        stderr = proc.stderr.read() if proc.stderr is not None else ""
        raise AssertionError(
            f"debug CLI produced no response for command {command!r}\nstderr:\n{stderr}"
        )

    response = json.loads(line)
    if response.get("type") == "error":
        raise AssertionError(
            f"debug CLI returned error for command {command!r}: {response}"
        )
    return response


def normalize_next_offset(response: dict) -> int:
    if "next_offset" in response:
        return int(response["next_offset"])
    if "nextOffset" in response:
        return int(response["nextOffset"])
    raise AssertionError(f"missing next offset in response: {response}")


def linux_distro_command_contracts(command: str, expected: str) -> list[tuple[str, str]]:
    raw_sequence = os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_COMMANDS", "")
    if not raw_sequence:
        profile = os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_PROFILE", "")
        if profile:
            if profile not in LINUX_DISTRO_RUNTIME_PROFILES:
                raise AssertionError(f"unknown linux distribution runtime profile: {profile}")
            return list(LINUX_DISTRO_RUNTIME_PROFILES[profile])
        return [(command, expected)]

    contracts: list[tuple[str, str]] = []
    for index, raw_entry in enumerate(raw_sequence.splitlines(), start=1):
        if not raw_entry.strip():
            continue
        command_text, separator, expected_text = raw_entry.partition("=>")
        if not separator:
            raise AssertionError(
                "MYCPU_LINUX_DISTRO_RUNTIME_COMMANDS entry "
                f"{index} must use 'command=>expected' format"
            )
        contracts.append((command_text, expected_text))

    if not contracts:
        raise AssertionError("MYCPU_LINUX_DISTRO_RUNTIME_COMMANDS did not include any command contracts")
    return contracts


def resolve_curated_linux_distro_runtime() -> dict | None:
    distro = os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO", "").strip()
    if not distro:
        return None
    if distro not in CURATED_LINUX_DISTRO_RUNTIME_MATRIX:
        raise AssertionError(f"unknown curated linux distribution runtime: {distro}")

    matrix_entry = CURATED_LINUX_DISTRO_RUNTIME_MATRIX[distro]
    image_env = str(matrix_entry["image_env"])
    rootfs_env = str(matrix_entry["rootfs_env"])
    bootargs_env = str(matrix_entry["bootargs_env"])
    prompt_env = str(matrix_entry["prompt_env"])

    if image_env not in os.environ:
        raise AssertionError(f"missing curated linux distribution Image env: {image_env}")
    if rootfs_env not in os.environ:
        raise AssertionError(f"missing curated linux distribution rootfs env: {rootfs_env}")

    return {
        "distro": distro,
        "image": pathlib.Path(os.environ[image_env]),
        "disk": pathlib.Path(os.environ[rootfs_env]),
        "prompt": os.environ.get(prompt_env, str(matrix_entry["prompt"])),
        "command": str(matrix_entry["command"]),
        "expected": str(matrix_entry["expected"]),
        "bootargs": os.environ.get(bootargs_env, str(matrix_entry["bootargs"])),
        "validated_profiles": tuple(matrix_entry["validated_profiles"]),
    }


def resolve_linux_distro_shell_contract() -> dict:
    curated = resolve_curated_linux_distro_runtime()
    if curated is not None:
        image_path = pathlib.Path(
            os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_IMAGE", str(curated["image"]))
        )
        disk_path = pathlib.Path(
            os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS", str(curated["disk"]))
        )
        prompt = os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_PROMPT", str(curated["prompt"]))
        command = os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_COMMAND", str(curated["command"]))
        expected = os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_EXPECT", str(curated["expected"]))
        bootargs = os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_BOOTARGS", str(curated["bootargs"]))
    else:
        image_path = pathlib.Path(
            os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_IMAGE", str(DEFAULT_LINUX_PROTO_RUNTIME_IMAGE))
        )
        disk_path = pathlib.Path(
            os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS", str(DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS))
        )
        prompt = os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_PROMPT", "mycpu-linux# ")
        if "MYCPU_LINUX_DISTRO_RUNTIME_COMMAND" in os.environ:
            command = os.environ["MYCPU_LINUX_DISTRO_RUNTIME_COMMAND"]
        elif disk_path == DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS:
            command = "help"
        else:
            command = "cat /etc/os-release"

        if "MYCPU_LINUX_DISTRO_RUNTIME_EXPECT" in os.environ:
            expected = os.environ["MYCPU_LINUX_DISTRO_RUNTIME_EXPECT"]
        elif disk_path == DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS:
            expected = "commands: help uptime exit"
        else:
            expected = "ID="

        bootargs = os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_BOOTARGS", "")
    return {
        "image": image_path,
        "disk": disk_path,
        "prompt": prompt,
        "command": command,
        "expected": expected,
        "bootargs": bootargs,
    }


@contextlib.contextmanager
def extract_file_from_ext4_image(image_path: pathlib.Path, guest_path: str, host_name: str):
    with tempfile.TemporaryDirectory(prefix="mycpu-ext4-extract.") as temp_dir:
        extracted_path = pathlib.Path(temp_dir) / host_name
        dump_proc = subprocess.run(
            [
                "debugfs",
                "-R",
                f"dump -p {guest_path} {extracted_path}",
                str(image_path),
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if dump_proc.returncode != 0:
            raise AssertionError(
                f"failed to extract {guest_path} from {image_path}:\n"
                f"stdout:\n{dump_proc.stdout}\n"
                f"stderr:\n{dump_proc.stderr}"
            )
        if not extracted_path.is_file():
            raise AssertionError(
                f"debugfs reported success extracting {guest_path}, but host file is missing: {extracted_path}"
            )
        yield extracted_path


@contextlib.contextmanager
def mutable_ext4_image_copy(source_path: pathlib.Path):
    with tempfile.TemporaryDirectory(prefix="mycpu-ext4-copy.") as temp_dir:
        temp_path = pathlib.Path(temp_dir) / source_path.name
        temp_path.write_bytes(source_path.read_bytes())
        yield temp_path


def install_file_into_ext4_image(
    image_path: pathlib.Path,
    host_path: pathlib.Path,
    guest_path: str,
    *,
    mode: str | None = None,
) -> None:
    install_proc = subprocess.run(
        [
            "debugfs",
            "-w",
            "-R",
            f"write {host_path} {guest_path}",
            str(image_path),
        ],
        cwd=MYCPU_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if install_proc.returncode != 0:
        raise AssertionError(
            f"failed to install {host_path} into {image_path} at {guest_path}:\n"
            f"stdout:\n{install_proc.stdout}\n"
            f"stderr:\n{install_proc.stderr}"
        )

    if mode is not None:
        chmod_proc = subprocess.run(
            [
                "debugfs",
                "-w",
                "-R",
                f"sif {guest_path} mode {mode}",
                str(image_path),
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if chmod_proc.returncode != 0:
            raise AssertionError(
                f"failed to chmod {guest_path} in {image_path} to {mode}:\n"
                f"stdout:\n{chmod_proc.stdout}\n"
                f"stderr:\n{chmod_proc.stderr}"
            )


STATIC_SURFACE_MNEMONIC_RE = re.compile(r"^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2,8}\s+)+([a-z0-9_.]+)\b")
STATIC_SURFACE_IGNORE_MNEMONICS = {"fence", "fence.i"}


def supported_riscv_userland_fp_mnemonics() -> set[str]:
    floating_ops_header = (MYCPU_DIR / "src" / "exec" / "floating_ops.h").read_text()
    supported = {
        mnemonic.replace("_", ".")
        for mnemonic in re.findall(r"bool is_([a-z0-9_]+)\(const Insn& insn\);", floating_ops_header)
    }
    supported.update(
        {
            "fabs.d",
            "fabs.s",
            "c.fld",
            "c.fldsp",
            "c.fsd",
            "c.fsdsp",
            "fld",
            "flw",
            "fsd",
            "fsw",
            "fneg.s",
            "fmv.s",
            "frcsr",
            "frflags",
            "frrm",
            "fscsr",
            "fsflags",
            "fsrm",
        }
    )
    return supported


def extract_riscv_static_surface_mnemonics(binary_path: pathlib.Path) -> set[str]:
    objdump_proc = subprocess.run(
        ["riscv64-unknown-elf-objdump", "-d", str(binary_path)],
        cwd=MYCPU_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if objdump_proc.returncode != 0:
        raise AssertionError(
            f"failed to disassemble {binary_path}:\n"
            f"stdout:\n{objdump_proc.stdout}\n"
            f"stderr:\n{objdump_proc.stderr}"
        )

    mnemonics: set[str] = set()
    for line in objdump_proc.stdout.splitlines():
        match = STATIC_SURFACE_MNEMONIC_RE.match(line)
        if not match:
            continue
        mnemonic = match.group(1)
        if mnemonic in STATIC_SURFACE_IGNORE_MNEMONICS:
            continue
        if mnemonic.startswith("f") or mnemonic.startswith("c.f"):
            mnemonics.add(mnemonic)
    return mnemonics


class RunDebugCliProbeTest(unittest.TestCase):
    def test_curated_linux_distro_runtime_matrix_declares_alpine_and_debian_shell_routes(self) -> None:
        self.assertEqual(
            tuple(sorted(CURATED_LINUX_DISTRO_RUNTIME_MATRIX)),
            ("alpine", "debian"),
        )
        self.assertEqual(CURATED_LINUX_DISTRO_RUNTIME_MATRIX["alpine"]["expected"], "ID=alpine")
        self.assertEqual(CURATED_LINUX_DISTRO_RUNTIME_MATRIX["debian"]["expected"], "ID=debian")
        self.assertIn(
            "filesystem_consistency",
            CURATED_LINUX_DISTRO_RUNTIME_MATRIX["alpine"]["validated_profiles"],
        )
        self.assertIn(
            "fs_state_guardrail",
            CURATED_LINUX_DISTRO_RUNTIME_MATRIX["alpine"]["validated_profiles"],
        )
        self.assertEqual(
            CURATED_LINUX_DISTRO_RUNTIME_MATRIX["debian"]["validated_profiles"],
            ("shell",),
        )

    def test_resolve_linux_distro_shell_contract_defaults_to_linux_proto_shell_for_repo_rootfs(self) -> None:
        with unittest.mock.patch.dict(os.environ, {}, clear=True):
            contract = resolve_linux_distro_shell_contract()

        self.assertEqual(contract["image"], DEFAULT_LINUX_PROTO_RUNTIME_IMAGE)
        self.assertEqual(contract["disk"], DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS)
        self.assertEqual(contract["prompt"], "mycpu-linux# ")
        self.assertEqual(contract["command"], "help")
        self.assertEqual(contract["expected"], "commands: help uptime exit")
        self.assertEqual(contract["bootargs"], "")

    def test_resolve_linux_distro_shell_contract_defaults_to_os_release_for_external_rootfs(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            external_rootfs = pathlib.Path(temp_dir) / "debian-rootfs.ext4"
            with unittest.mock.patch.dict(
                os.environ,
                {
                    "MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS": str(external_rootfs),
                },
                clear=True,
            ):
                contract = resolve_linux_distro_shell_contract()

        self.assertEqual(contract["disk"], external_rootfs)
        self.assertEqual(contract["command"], "cat /etc/os-release")
        self.assertEqual(contract["expected"], "ID=")

    def test_resolve_linux_distro_shell_contract_prefers_explicit_shell_over_external_rootfs(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            external_rootfs = pathlib.Path(temp_dir) / "alpine-rootfs.ext4"
            with unittest.mock.patch.dict(
                os.environ,
                {
                    "MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS": str(external_rootfs),
                    "MYCPU_LINUX_DISTRO_RUNTIME_COMMAND": "cat /etc/os-release",
                    "MYCPU_LINUX_DISTRO_RUNTIME_EXPECT": "ID=alpine",
                },
                clear=True,
            ):
                contract = resolve_linux_distro_shell_contract()

        self.assertEqual(contract["disk"], external_rootfs)
        self.assertEqual(contract["command"], "cat /etc/os-release")
        self.assertEqual(contract["expected"], "ID=alpine")

    def test_resolve_linux_distro_shell_contract_uses_curated_alpine_defaults(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = pathlib.Path(temp_dir)
            image_path = temp_path / "Image"
            rootfs_path = temp_path / "alpine-rootfs.ext4"
            with unittest.mock.patch.dict(
                os.environ,
                {
                    "MYCPU_LINUX_DISTRO_RUNTIME_DISTRO": "alpine",
                    "MYCPU_LINUX_DISTRO_CURATED_IMAGE": str(image_path),
                    "MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS": str(rootfs_path),
                },
                clear=True,
            ):
                contract = resolve_linux_distro_shell_contract()

        self.assertEqual(contract["image"], image_path)
        self.assertEqual(contract["disk"], rootfs_path)
        self.assertEqual(contract["prompt"], "~ # ")
        self.assertEqual(contract["command"], "cat /etc/os-release")
        self.assertEqual(contract["expected"], "ID=alpine")
        self.assertEqual(
            contract["bootargs"],
            "console=ttyS0,115200 earlycon=ns16550a,mmio,0x10000000 "
            "root=/dev/vda rw rootfstype=ext4 rootwait init=/bin/sh loglevel=8 ignore_loglevel",
        )

    def test_resolve_linux_distro_shell_contract_uses_curated_debian_defaults(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = pathlib.Path(temp_dir)
            image_path = temp_path / "Image"
            rootfs_path = temp_path / "debian-rootfs.ext4"
            with unittest.mock.patch.dict(
                os.environ,
                {
                    "MYCPU_LINUX_DISTRO_RUNTIME_DISTRO": "debian",
                    "MYCPU_LINUX_DISTRO_CURATED_IMAGE": str(image_path),
                    "MYCPU_LINUX_DISTRO_CURATED_DEBIAN_ROOTFS": str(rootfs_path),
                },
                clear=True,
            ):
                contract = resolve_linux_distro_shell_contract()

        self.assertEqual(contract["image"], image_path)
        self.assertEqual(contract["disk"], rootfs_path)
        self.assertEqual(contract["prompt"], "mycpu-debian# ")
        self.assertEqual(contract["command"], "cat /etc/os-release")
        self.assertEqual(contract["expected"], "ID=debian")
        self.assertEqual(
            contract["bootargs"],
            "console=ttyS0,115200 earlycon=ns16550a,mmio,0x10000000 "
            "root=/dev/vda rw rootfstype=ext4 rootwait init=/mycpu-debian-init "
            "loglevel=8 ignore_loglevel",
        )

    def test_linux_distro_command_contracts_defaults_to_single_command(self) -> None:
        with unittest.mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(
                linux_distro_command_contracts("cat /etc/os-release", "ID=alpine"),
                [("cat /etc/os-release", "ID=alpine")],
            )

    def test_linux_distro_command_contracts_parses_multiline_sequence(self) -> None:
        with unittest.mock.patch.dict(
            os.environ,
            {
                "MYCPU_LINUX_DISTRO_RUNTIME_COMMANDS": (
                    "cat /etc/os-release=>ID=alpine\n"
                    "ls -l /bin/sh=>busybox\n"
                    "printf ok >/tmp/mycpu-smoke; cat /tmp/mycpu-smoke=>ok"
                )
            },
            clear=True,
        ):
            self.assertEqual(
                linux_distro_command_contracts("ignored", "ignored"),
                [
                    ("cat /etc/os-release", "ID=alpine"),
                    ("ls -l /bin/sh", "busybox"),
                    ("printf ok >/tmp/mycpu-smoke; cat /tmp/mycpu-smoke", "ok"),
                ],
            )

    def test_linux_distro_command_contracts_uses_filesystem_consistency_profile(self) -> None:
        with unittest.mock.patch.dict(
            os.environ,
            {"MYCPU_LINUX_DISTRO_RUNTIME_PROFILE": "filesystem_consistency"},
            clear=True,
        ):
            self.assertEqual(
                linux_distro_command_contracts("ignored", "ignored"),
                [
                    ("cat /etc/os-release", "ID=alpine"),
                    ('printf "cwd:"; pwd', "cwd:/"),
                    (
                        "mkdir -p /tmp/mycpu-smoke; test -d /tmp/mycpu-smoke; "
                        'printf "mkdir-status:"; printf "%s" "$?"',
                        "mkdir-status:0",
                    ),
                    (
                        "printf '\\150\\145\\154\\154\\157' >/tmp/mycpu-smoke/file; "
                        "cat /tmp/mycpu-smoke/file",
                        "hello",
                    ),
                    (
                        "printf '\\167\\157\\162\\154\\144' >>/tmp/mycpu-smoke/file; "
                        "cat /tmp/mycpu-smoke/file",
                        "helloworld",
                    ),
                    ("wc -c </tmp/mycpu-smoke/file", "10"),
                    (
                        "rm /tmp/mycpu-smoke/file; test ! -e /tmp/mycpu-smoke/file; "
                        'printf "delete-status:"; printf "%s" "$?"',
                        "delete-status:0",
                    ),
                    (
                        "rmdir /tmp/mycpu-smoke; test ! -e /tmp/mycpu-smoke; "
                        'printf "rmdir-status:"; printf "%s" "$?"',
                        "rmdir-status:0",
                    ),
                    ("printf '\\163\\164\\151\\154\\154\\055\\141\\154\\151\\166\\145'", "still-alive"),
                ],
            )

    def test_linux_distro_command_contracts_uses_tty_login_probe_profile(self) -> None:
        with unittest.mock.patch.dict(
            os.environ,
            {"MYCPU_LINUX_DISTRO_RUNTIME_PROFILE": "tty_login_probe"},
            clear=True,
        ):
            self.assertEqual(
                linux_distro_command_contracts("ignored", "ignored"),
                [
                    ("cat /etc/os-release", "ID=alpine"),
                    (
                        "for x in getty login stty setsid tty; do "
                        'command -v "$x" >/dev/null 2>&1 && printf "%s=present\\n" "$x" || '
                        'printf "%s=missing\\n" "$x"; '
                        "done",
                        "tty=present",
                    ),
                    ('printf "tty-path:"; tty || true', "tty-path:"),
                    ("stty -a 2>&1 | sed -n '1p'", "speed"),
                    (
                        "setsid sh -c 'printf setsid-status:; tty || true' 2>&1",
                        "setsid-status:",
                    ),
                    (
                        "setsid /sbin/getty -n -l /bin/sh -L 115200 ttyS0 vt100",
                        "~ # ",
                    ),
                    ('printf "getty-roundtrip-ok"', "getty-roundtrip-ok"),
                ],
            )

    def test_linux_distro_command_contracts_uses_process_control_profile(self) -> None:
        with unittest.mock.patch.dict(
            os.environ,
            {"MYCPU_LINUX_DISTRO_RUNTIME_PROFILE": "process_control"},
            clear=True,
        ):
            self.assertEqual(
                linux_distro_command_contracts("ignored", "ignored"),
                [
                    ("cat /etc/os-release", "ID=alpine"),
                    ("sleep 1; printf 'sleep-ok'", "sleep-ok"),
                    (
                        "sh -c 'sleep 1; exit 7' & pid=$!; wait $pid; "
                        'printf "wait-status:%s" "$?"',
                        "wait-status:7",
                    ),
                    (
                        "trap 'printf trap-hit' TERM; kill -TERM $$",
                        "trap-hit",
                    ),
                    (
                        "false || printf 'or-ok'; true && printf ':and-ok'; "
                        "false; printf ':status:%s' \"$?\"",
                        "or-ok:and-ok:status:1",
                    ),
                ],
            )

    def test_linux_distro_command_contracts_uses_filesystem_persistence_profile(self) -> None:
        with unittest.mock.patch.dict(
            os.environ,
            {"MYCPU_LINUX_DISTRO_RUNTIME_PROFILE": "filesystem_persistence"},
            clear=True,
        ):
            self.assertEqual(
                linux_distro_command_contracts("ignored", "ignored"),
                [
                    ("cat /etc/os-release", "ID=alpine"),
                    (
                        "rm -rf /root/mycpu-persist; mkdir -p /root/mycpu-persist/sub; "
                        'printf "persist-dir-status:%s" "$?"',
                        "persist-dir-status:0",
                    ),
                    (
                        "printf alpha >/root/mycpu-persist/file; "
                        "sync /root/mycpu-persist/file 2>/dev/null || sync; "
                        "cat /root/mycpu-persist/file",
                        "alpha",
                    ),
                    (
                        "printf beta >/root/mycpu-persist/tmp; "
                        "mv -f /root/mycpu-persist/tmp /root/mycpu-persist/file; "
                        "cat /root/mycpu-persist/file",
                        "beta",
                    ),
                    (
                        "find /root/mycpu-persist -maxdepth 2 -type f | sort",
                        "/root/mycpu-persist/file",
                    ),
                    (
                        "dd if=/dev/zero of=/root/mycpu-persist/large bs=1024 count=64 2>/dev/null; "
                        "sync /root/mycpu-persist/large 2>/dev/null || sync; "
                        'printf "large-size:"; wc -c </root/mycpu-persist/large',
                        "large-size:65536",
                    ),
                    (
                        "rm -rf /root/mycpu-persist; sync; "
                        'printf "persist-cleanup:%s" "$?"',
                        "persist-cleanup:0",
                    ),
                ],
            )

    def test_linux_distro_command_contracts_uses_fs_state_guardrail_profile(self) -> None:
        with unittest.mock.patch.dict(
            os.environ,
            {"MYCPU_LINUX_DISTRO_RUNTIME_PROFILE": "fs_state_guardrail"},
            clear=True,
        ):
            self.assertEqual(
                linux_distro_command_contracts("ignored", "ignored"),
                [
                    ("cat /etc/os-release", "ID=alpine"),
                    ("awk 'BEGIN{print sqrt(2)}'", "1.41421"),
                    ("sleep 1; printf 'timer-roundtrip-ok'", "timer-roundtrip-ok"),
                    (
                        "sh -c 'awk \"BEGIN{printf \\\"%.1f\\\\n\\\", 2.25+0.25}\"; exit 7' & pid=$!; wait $pid; "
                        'printf " child-status:%s" "$?"',
                        "2.5 child-status:7",
                    ),
                    (
                        "awk 'BEGIN{printf \"%.1f %.1f\\n\", 2.25+0.25, sqrt(2)*sqrt(2)}'",
                        "2.5 2.0",
                    ),
                ],
            )

    def test_linux_distro_command_contracts_explicit_sequence_overrides_profile(self) -> None:
        with unittest.mock.patch.dict(
            os.environ,
            {
                "MYCPU_LINUX_DISTRO_RUNTIME_PROFILE": "filesystem_consistency",
                "MYCPU_LINUX_DISTRO_RUNTIME_COMMANDS": "cat /etc/os-release=>ID=alpine",
            },
            clear=True,
        ):
            self.assertEqual(
                linux_distro_command_contracts("ignored", "ignored"),
                [("cat /etc/os-release", "ID=alpine")],
            )

    def test_linux_distro_command_contracts_rejects_unknown_profile(self) -> None:
        with unittest.mock.patch.dict(
            os.environ,
            {"MYCPU_LINUX_DISTRO_RUNTIME_PROFILE": "unknown"},
            clear=True,
        ):
            with self.assertRaisesRegex(AssertionError, "unknown linux distribution runtime profile"):
                linux_distro_command_contracts("ignored", "ignored")

    def test_missing_input_paths_reports_primary_image_and_payloads(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = pathlib.Path(temp_dir)
            initrd = temp_path / "rootfs.cpio"
            initrd.write_bytes(b"initrd")
            args = PROBE.parse_args(
                [
                    "--target",
                    "./mycpu",
                    "--image",
                    str(temp_path / "Image"),
                    "--flat",
                    "--addr",
                    "0x80200000",
                    "--payload",
                    str(temp_path / "board.dtb"),
                    "0x88000000",
                    "--payload",
                    str(initrd),
                    "0x84000000",
                ]
            )

            missing = PROBE.missing_input_paths(args)

        self.assertEqual(
            missing,
            [
                ("image", str(temp_path / "Image")),
                ("payload", str(temp_path / "board.dtb")),
            ],
        )

    def test_main_fails_closed_before_probe_when_inputs_are_missing(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = pathlib.Path(temp_dir)
            missing_image = temp_path / "Image"
            missing_dtb = temp_path / "board.dtb"
            called = False

            def fake_run_probe(*_args, **_kwargs):
                nonlocal called
                called = True
                raise AssertionError("main should not invoke run_probe when input files are missing")

            stdout = io.StringIO()
            stderr = io.StringIO()
            original_run_probe = PROBE.run_probe
            PROBE.run_probe = fake_run_probe
            try:
                with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                    rc = PROBE.main(
                        [
                            "--target",
                            "./mycpu",
                            "--image",
                            str(missing_image),
                            "--flat",
                            "--addr",
                            "0x80200000",
                            "--payload",
                            str(missing_dtb),
                            "0x88000000",
                        ]
                    )
            finally:
                PROBE.run_probe = original_run_probe

        self.assertEqual(rc, 1)
        self.assertFalse(called)
        self.assertIn("missing input files:", stderr.getvalue())
        self.assertIn(str(missing_image), stderr.getvalue())
        self.assertIn(str(missing_dtb), stderr.getvalue())
        self.assertEqual(stdout.getvalue(), "")

    def test_flat_probe_load_contract_preserves_addr_without_disk(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--backend",
                "pipeline",
                "--block-transport",
                "virtio-blk",
                "--flat",
                "--addr",
                "0x80200000",
                "--step-cycles",
                "32",
            ]
        )

        commands = PROBE.build_commands(args)

        self.assertEqual(
            commands,
            [
                {
                    "cmd": "load",
                    "image": "Image",
                    "backend": "pipeline",
                    "block_transport": "virtio-blk",
                    "flat": True,
                    "addr": 0x80200000,
                },
                {"cmd": "step_cycle", "count": 32},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_default_probe_keeps_single_step_cycle_command(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "kernel.elf",
                "--step-cycles",
                "64",
            ]
        )

        commands = PROBE.build_commands(args)

        self.assertEqual(
            commands,
            [
                {"cmd": "load", "image": "kernel.elf", "backend": "functional"},
                {"cmd": "step_cycle", "count": 64},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_l1d_probe_load_contract_is_explicit_opt_in(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "kernel.elf",
                "--l1d",
                "--step-cycles",
                "64",
            ]
        )

        commands = PROBE.build_commands(args)

        self.assertEqual(
            commands,
            [
                {
                    "cmd": "load",
                    "image": "kernel.elf",
                    "backend": "functional",
                    "l1d": True,
                },
                {"cmd": "step_cycle", "count": 64},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_translation_plan_probe_command_is_explicit_opt_in(self) -> None:
        default_args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "kernel.elf",
                "--step-cycles",
                "64",
            ]
        )
        opt_in_args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "kernel.elf",
                "--translation-plan",
                "--step-cycles",
                "64",
            ]
        )

        self.assertFalse(default_args.translation_plan)
        self.assertTrue(opt_in_args.translation_plan)
        self.assertNotIn({"cmd": "translation_plan"}, PROBE.build_commands(default_args))
        self.assertEqual(
            PROBE.build_commands(opt_in_args),
            [
                {"cmd": "load", "image": "kernel.elf", "backend": "functional"},
                {"cmd": "step_cycle", "count": 64},
                {"cmd": "translation_plan"},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_jit_dispatch_probe_command_is_explicit_opt_in(self) -> None:
        default_args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "kernel.elf",
                "--step-cycles",
                "64",
            ]
        )
        opt_in_args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "kernel.elf",
                "--jit-dispatch",
                "--step-cycles",
                "64",
            ]
        )

        self.assertFalse(default_args.jit_dispatch)
        self.assertTrue(opt_in_args.jit_dispatch)
        self.assertNotIn({"cmd": "jit_dispatch"}, PROBE.build_commands(default_args))
        self.assertEqual(
            PROBE.build_commands(opt_in_args),
            [
                {"cmd": "load", "image": "kernel.elf", "backend": "functional"},
                {"cmd": "step_cycle", "count": 64},
                {"cmd": "jit_dispatch"},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_uart_wait_and_input_actions_preserve_cli_order(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "kernel.elf",
                "--uart-wait",
                "$ ",
                "100",
                "--uart-input",
                "echo SHELL_OK\r",
                "--uart-wait",
                "SHELL_OK",
                "200",
            ]
        )

        commands = PROBE.build_commands(args)

        self.assertEqual(
            commands,
            [
                {"cmd": "load", "image": "kernel.elf", "backend": "functional"},
                {"cmd": "run_until_uart_contains", "text": "$ ", "max_steps": 100},
                {"cmd": "uart_input", "text": "echo SHELL_OK\r"},
                {"cmd": "run_until_uart_contains", "text": "SHELL_OK", "max_steps": 200},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_make_run_workload_keeps_xv6_elf_disk_contract(self) -> None:
        proc = subprocess.run(
            ["make", "-n", "run-workload-xv6"],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("--image external/xv6-riscv/kernel/kernel", proc.stdout)
        self.assertIn("--disk external/xv6-riscv/fs.img", proc.stdout)
        self.assertIn("--block-transport virtio-blk", proc.stdout)
        self.assertNotIn("--flat", proc.stdout)

    def test_default_make_test_includes_xv6_observation_guardrails(self) -> None:
        proc = subprocess.run(
            ["make", "-n", "test"],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("=== host:xv6_boot_smoke ===", proc.stdout)
        self.assertIn("=== host:run_debug_cli_probe ===", proc.stdout)
        self.assertNotIn("--l1d", proc.stdout)
        self.assertNotIn("test-host-run_debug_cli_probe_linux_proto_runtime", proc.stdout)

    def test_default_make_test_pipeline_includes_xv6_observation_guardrails(self) -> None:
        proc = subprocess.run(
            ["make", "-n", "test-pipeline"],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("=== host:xv6_boot_smoke ===", proc.stdout)
        self.assertIn("=== host:run_debug_cli_probe ===", proc.stdout)
        self.assertNotIn("--l1d", proc.stdout)
        self.assertNotIn("test-host-run_debug_cli_probe_linux_proto_runtime", proc.stdout)

    def test_real_xv6_probe_emits_functional_profile_summary(self) -> None:
        proc = subprocess.run(
            [
                "python3",
                "workloads/run_debug_cli_probe.py",
                "--target",
                "./mycpu",
                "--image",
                "external/xv6-riscv/kernel/kernel",
                "--disk",
                "external/xv6-riscv/fs.img",
                "--block-transport",
                "virtio-blk",
                "--step-cycles",
                "5000",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn(
            "summary: cycle=5000 instret=5000 pc=0x800010dc privilege=S backend=functional",
            proc.stdout,
        )
        self.assertIn("profile: retirements=5000 traps=0 memory=1570", proc.stdout)
        self.assertIn(
            "shadow-cache: line_size=64 capacity_lines=64 resident_lines=20 line_accesses=1515 hits=1495 misses=20 evictions=0 bypasses=55",
            proc.stdout,
        )
        self.assertIn(
            "memory-top: label=ram kind=ram accesses=1515 reads=621 writes=894 faults=0 bytes=8562",
            proc.stdout,
        )
        self.assertIn("xv6 kernel is booting", proc.stdout)

    def test_real_xv6_probe_emits_pipeline_memory_signal(self) -> None:
        proc = subprocess.run(
            [
                "python3",
                "workloads/run_debug_cli_probe.py",
                "--target",
                "./mycpu",
                "--image",
                "external/xv6-riscv/kernel/kernel",
                "--disk",
                "external/xv6-riscv/fs.img",
                "--block-transport",
                "virtio-blk",
                "--backend",
                "pipeline",
                "--step-cycles",
                "5000",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn(
            "summary: cycle=5000 instret=278 pc=0x0 privilege=S backend=pipeline",
            proc.stdout,
        )
        self.assertIn("profile: retirements=279 traps=4593 memory=85", proc.stdout)
        self.assertIn(
            "shadow-cache: line_size=64 capacity_lines=64 resident_lines=11 line_accesses=78 hits=67 misses=11 evictions=0 bypasses=7",
            proc.stdout,
        )
        self.assertIn(
            "memory-top: label=ram kind=ram accesses=78 reads=21 writes=57 faults=0 bytes=588",
            proc.stdout,
        )

    def test_linux_proto_dummy_payload_probe_emits_functional_profile_summary(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = pathlib.Path(temp_dir)
            dummy_image = build_linux_dummy_flat_image(temp_path)

            build_proc = subprocess.run(
                [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                    f"LINUX_PROTO_IMAGE={dummy_image}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(build_proc.returncode, 0, msg=build_proc.stderr)

            probe_proc = subprocess.run(
                [
                    "python3",
                    "workloads/run_debug_cli_probe.py",
                    "--target",
                    "./mycpu",
                    "--image",
                    "workloads/linux_proto/linux_sbi_shim.bin",
                    "--flat",
                    "--addr",
                    "0x80000000",
                    "--payload",
                    str(dummy_image),
                    "0x80200000",
                    "--payload",
                    "workloads/linux_proto/mycpu_virt.dtb",
                    "0x87f00000",
                    "--payload",
                    "workloads/linux_proto/rootfs.cpio",
                    "0x84000000",
                    "--set-reg",
                    "a0",
                    "0x0",
                    "--set-reg",
                    "a1",
                    "0x87f00000",
                    "--set-reg",
                    "a2",
                    "0x80200000",
                    "--step-cycles",
                    "64",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(probe_proc.returncode, 0, msg=probe_proc.stderr)
        self.assertIn(
            "load: image=workloads/linux_proto/linux_sbi_shim.bin format=flat addr=0x80000000 disk=none block_transport=default",
            probe_proc.stdout,
        )
        self.assertIn(f"payloads: {dummy_image}@0x80200000", probe_proc.stdout)
        self.assertIn("workloads/linux_proto/mycpu_virt.dtb@0x87f00000", probe_proc.stdout)
        self.assertIn("workloads/linux_proto/rootfs.cpio@0x84000000", probe_proc.stdout)
        self.assertIn("gpr-seeds: a0=0x0 a1=0x87f00000 a2=0x80200000", probe_proc.stdout)
        self.assertIn(
            "summary: cycle=64 instret=64 pc=0x80200024 privilege=S backend=functional",
            probe_proc.stdout,
        )
        self.assertIn("profile: retirements=64 traps=0 memory=19", probe_proc.stdout)
        self.assertIn(
            "shadow-cache: line_size=64 capacity_lines=64 resident_lines=3 line_accesses=19 hits=16 misses=3 evictions=0 bypasses=0",
            probe_proc.stdout,
        )
        self.assertIn(
            "memory-top: label=ram kind=ram accesses=19 reads=10 writes=9 faults=0 bytes=76",
            probe_proc.stdout,
        )

    def test_make_run_workload_can_emit_flat_load_contract(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "run-workload",
                "WORKLOAD_NAME=xv6",
                "WORKLOAD_IMAGE=tests/data/linux_like_image.bin",
                "WORKLOAD_BUILD_COMMAND=:",
                "WORKLOAD_IMAGE_FORMAT=flat",
                "WORKLOAD_LOAD_ADDR=0x80200000",
                "WORKLOAD_DISK=",
                "BOARD_BLOCK_TRANSPORT=virtio-blk",
                "WORKLOAD_PROBE_CYCLES=32",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("--image tests/data/linux_like_image.bin", proc.stdout)
        self.assertIn("--flat", proc.stdout)
        self.assertIn("--addr 0x80200000", proc.stdout)
        self.assertIn("--block-transport virtio-blk", proc.stdout)
        self.assertIn("--step-cycles 32", proc.stdout)
        self.assertNotIn("--disk ", proc.stdout)

    def test_emit_probe_summary_exposes_linux_facing_checkpoint_fields(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--disk",
                "rootfs.img",
                "--block-transport",
                "virtio-blk",
                "--flat",
                "--addr",
                "0x80200000",
                "--payload",
                "board.dtb",
                "0x88000000",
                "--payload",
                "initrd.img",
                "0x84000000",
                "--set-reg",
                "a0",
                "0x0",
                "--set-reg",
                "a1",
                "0x88000000",
            ]
        )
        lines = [
            {
                "type": "snapshot",
                "summary": {
                    "cycle": 8192,
                    "instret": 4096,
                    "pc": "0x80200010",
                    "privilege": "S",
                    "backend": "functional",
                },
                "csrs": {
                    "mcause": "0x0",
                    "mepc": "0x80200000",
                    "mtval": "0x0",
                    "scause": "0xd",
                    "sepc": "0x80200010",
                    "stval": "0x80201000",
                    "stvec": "0x80200100",
                    "satp": "0x8000000000080200",
                },
                "profile": {
                    "total_retirements": 4096,
                    "total_traps": 1,
                    "total_memory_observations": 5,
                    "shadow_cache": {
                        "line_size_bytes": 64,
                        "capacity_lines": 64,
                        "resident_lines": 2,
                        "line_accesses": 5,
                        "hits": 3,
                        "misses": 2,
                        "evictions": 0,
                        "bypasses": 0,
                    },
                    "hot_paths": [
                        {
                            "start_pc": "0x80200100",
                            "end_pc": "0x80200108",
                            "executions": 1,
                            "retired_instructions": 100,
                        },
                        {
                            "start_pc": "0x80200000",
                            "end_pc": "0x80200020",
                            "executions": 3,
                            "retired_instructions": 9,
                        }
                    ],
                    "traps": [
                        {
                            "pc": "0x80200010",
                            "cause": "0xd",
                            "privilege": "S",
                            "interrupt": False,
                            "count": 1,
                        }
                    ],
                    "memory_regions": [
                        {
                            "label": "ram",
                            "kind": "ram",
                            "accesses": 5,
                            "reads": 3,
                            "writes": 2,
                            "faults": 0,
                            "bytes": 20,
                        }
                    ],
                    "pc_costs": [
                        {
                            "pc": "0x80200010",
                            "raw": "0x28303",
                            "retirements": 4,
                            "cycles": 7,
                            "memory_observations": 3,
                            "memory_reads": 2,
                            "memory_writes": 1,
                            "memory_faults": 0,
                            "memory_bytes": 12,
                        }
                    ],
                    "branch_targets": [
                        {
                            "pc": "0x80200020",
                            "raw": "0xfe000ee3",
                            "target_pc": "0x80200000",
                            "executions": 3,
                            "redirects": 3,
                        }
                    ],
                },
                "devices": {
                    "uart": {
                        "output_size": 14,
                        "recent_output": "Booting Linux\n",
                    }
                },
            },
            {
                "type": "uart_output",
                "text": "Booting Linux\n",
            },
        ]

        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            rc = PROBE.emit_probe_summary(args, lines)

        self.assertEqual(rc, 0)
        self.assertIn(
            "load: image=Image format=flat addr=0x80200000 disk=rootfs.img block_transport=virtio-blk",
            stdout.getvalue(),
        )
        self.assertIn(
            "payloads: board.dtb@0x88000000 initrd.img@0x84000000",
            stdout.getvalue(),
        )
        self.assertIn("gpr-seeds: a0=0x0 a1=0x88000000", stdout.getvalue())
        self.assertIn("profile: retirements=4096 traps=1 memory=5", stdout.getvalue())
        self.assertIn(
            "shadow-cache: line_size=64 capacity_lines=64 resident_lines=2 line_accesses=5 hits=3 misses=2 evictions=0 bypasses=0",
            stdout.getvalue(),
        )
        self.assertIn(
            "hot-path: start=0x80200100 end=0x80200108 executions=1 retired=100",
            stdout.getvalue(),
        )
        self.assertIn(
            "translation-candidate: start=0x80200000 end=0x80200020 executions=3 retired=9",
            stdout.getvalue(),
        )
        self.assertIn(
            "trap-top: pc=0x80200010 cause=0xd privilege=S interrupt=false count=1",
            stdout.getvalue(),
        )
        self.assertIn(
            "memory-top: label=ram kind=ram accesses=5 reads=3 writes=2 faults=0 bytes=20",
            stdout.getvalue(),
        )
        self.assertIn(
            "pc-cost: pc=0x80200010 raw=0x28303 retirements=4 cycles=7 memory=3 reads=2 writes=1 faults=0 bytes=12",
            stdout.getvalue(),
        )
        self.assertIn(
            "branch-target: pc=0x80200020 raw=0xfe000ee3 target=0x80200000 executions=3 redirects=3",
            stdout.getvalue(),
        )
        self.assertNotIn("translation-plan:", stdout.getvalue())
        self.assertIn('uart-tail: bytes=14 recent="Booting Linux\\n"', stdout.getvalue())
        self.assertIn("uart: Booting Linux", stdout.getvalue())

    def test_emit_probe_summary_reports_empty_translation_candidate_fallback(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--step-cycles",
                "4",
            ]
        )
        lines = [
            {
                "type": "snapshot",
                "summary": {
                    "cycle": 4,
                    "instret": 0,
                    "pc": "0x80000000",
                    "privilege": "M",
                    "backend": "functional",
                },
                "csrs": {
                    "mcause": "0x0",
                    "mepc": "0x0",
                    "mtval": "0x0",
                    "scause": "0x0",
                    "sepc": "0x0",
                    "stval": "0x0",
                    "stvec": "0x0",
                    "satp": "0x0",
                },
                "profile": {
                    "total_retirements": 0,
                    "total_traps": 0,
                    "total_memory_observations": 0,
                    "hot_paths": [],
                },
                "devices": {
                    "uart": {
                        "output_size": 0,
                        "recent_output": "",
                    }
                },
            }
        ]

        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            rc = PROBE.emit_probe_summary(args, lines)

        self.assertEqual(rc, 0)
        self.assertIn(
            "translation-candidate: none reason=no-hot-paths",
            stdout.getvalue(),
        )
        self.assertNotIn("translation-plan:", stdout.getvalue())

    def test_emit_probe_summary_reports_opt_in_translation_plan_fallback(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--translation-plan",
                "--step-cycles",
                "16",
            ]
        )
        lines = [
            {
                "type": "translation_plan",
                "status": "fallback",
                "start_pc": "0x80200000",
                "end_pc": "0x80200008",
                "executions": 3,
                "retired_instructions": 6,
                "inlineable_instructions": 1,
                "fallback_pc": "0x80200004",
                "reason": "helper-required",
                "boundary_kind": "memory-load",
            },
            {
                "type": "snapshot",
                "summary": {
                    "cycle": 16,
                    "instret": 16,
                    "pc": "0x80200010",
                    "privilege": "M",
                    "backend": "functional",
                },
                "csrs": {
                    "mcause": "0x0",
                    "mepc": "0x0",
                    "mtval": "0x0",
                    "scause": "0x0",
                    "sepc": "0x0",
                    "stval": "0x0",
                    "stvec": "0x0",
                    "satp": "0x0",
                },
                "profile": {
                    "total_retirements": 16,
                    "total_traps": 0,
                    "total_memory_observations": 1,
                    "hot_paths": [
                        {
                            "start_pc": "0x80200000",
                            "end_pc": "0x80200008",
                            "executions": 3,
                            "retired_instructions": 6,
                        }
                    ],
                },
                "devices": {
                    "uart": {
                        "output_size": 0,
                        "recent_output": "",
                    }
                },
            },
        ]

        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            rc = PROBE.emit_probe_summary(args, lines)

        self.assertEqual(rc, 0)
        self.assertIn(
            "translation-plan: fallback start=0x80200000 end=0x80200008 executions=3 retired=6 inlineable=1 fallback_pc=0x80200004 reason=helper-required boundary=memory-load",
            stdout.getvalue(),
        )

    def test_emit_probe_summary_reports_opt_in_translation_plan_none(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--translation-plan",
                "--step-cycles",
                "4",
            ]
        )
        lines = [
            {
                "type": "translation_plan",
                "status": "none",
                "reason": "no-hot-paths",
            },
            {
                "type": "snapshot",
                "summary": {
                    "cycle": 4,
                    "instret": 0,
                    "pc": "0x80000000",
                    "privilege": "M",
                    "backend": "functional",
                },
                "csrs": {
                    "mcause": "0x0",
                    "mepc": "0x0",
                    "mtval": "0x0",
                    "scause": "0x0",
                    "sepc": "0x0",
                    "stval": "0x0",
                    "stvec": "0x0",
                    "satp": "0x0",
                },
                "profile": {
                    "total_retirements": 0,
                    "total_traps": 0,
                    "total_memory_observations": 0,
                    "hot_paths": [],
                },
                "devices": {
                    "uart": {
                        "output_size": 0,
                        "recent_output": "",
                    }
                },
            },
        ]

        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            rc = PROBE.emit_probe_summary(args, lines)

        self.assertEqual(rc, 0)
        self.assertIn(
            "translation-plan: none reason=no-hot-paths",
            stdout.getvalue(),
        )

    def test_emit_probe_summary_reports_opt_in_jit_dispatch(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--jit-dispatch",
                "--step-cycles",
                "16",
            ]
        )
        lines = [
            {
                "type": "jit_dispatch",
                "ok": False,
                "source": "hot-path-profile",
                "action": "reference-fallback",
                "start_pc": "0x80200000",
                "end_pc": "0x80200008",
                "cache_state": "miss",
                "planned": True,
                "translated": True,
                "lowered": False,
                "fallback_to_reference": True,
                "lowered_instruction_count": 0,
                "candidate_executions": 3,
                "candidate_retired_instructions": 6,
                "reject_kind": "control-flow",
                "reject_reason": "fallback-required",
                "helper_replay_kind": "none",
                "host_code": False,
                "executable_memory": False,
                "guest_execution": False,
            },
            {
                "type": "snapshot",
                "summary": {
                    "cycle": 16,
                    "instret": 16,
                    "pc": "0x80200010",
                    "privilege": "M",
                    "backend": "functional",
                },
                "csrs": {
                    "mcause": "0x0",
                    "mepc": "0x0",
                    "mtval": "0x0",
                    "scause": "0x0",
                    "sepc": "0x0",
                    "stval": "0x0",
                    "stvec": "0x0",
                    "satp": "0x0",
                },
                "profile": {
                    "total_retirements": 16,
                    "total_traps": 0,
                    "total_memory_observations": 0,
                    "hot_paths": [],
                },
                "devices": {
                    "uart": {
                        "output_size": 0,
                        "recent_output": "",
                    }
                },
            },
        ]

        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            rc = PROBE.emit_probe_summary(args, lines)

        self.assertEqual(rc, 0)
        self.assertIn(
            "jit-dispatch: source=hot-path-profile action=reference-fallback ok=false start=0x80200000 end=0x80200008 cache=miss planned=true translated=true lowered=false fallback=true lowered_ops=0 executions=3 retired=6 reject=control-flow reason=fallback-required helper=none host_code=false executable_memory=false guest_execution=false",
            stdout.getvalue(),
        )

    def test_emit_probe_summary_exposes_l1d_cache_when_enabled(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--flat",
                "--addr",
                "0x80000000",
                "--l1d",
            ]
        )
        lines = [
            {
                "type": "snapshot",
                "summary": {
                    "cycle": 16,
                    "instret": 6,
                    "pc": "0x80000014",
                    "privilege": "M",
                    "backend": "functional",
                },
                "csrs": {
                    "mcause": "0x0",
                    "mepc": "0x0",
                    "mtval": "0x0",
                    "scause": "0x0",
                    "sepc": "0x0",
                    "stval": "0x0",
                    "stvec": "0x0",
                    "satp": "0x0",
                },
                "profile": {},
                "l1_data_cache": {
                    "enabled": True,
                    "line_size_bytes": 64,
                    "capacity_lines": 64,
                    "loads": 2,
                    "stores": 1,
                    "hits": 2,
                    "misses": 1,
                    "evictions": 0,
                    "bypasses": 0,
                    "write_through_stores": 1,
                },
                "devices": {
                    "uart": {
                        "output_size": 0,
                        "recent_output": "",
                    }
                },
            }
        ]

        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            rc = PROBE.emit_probe_summary(args, lines)

        self.assertEqual(rc, 0)
        self.assertIn(
            "load: image=Image format=flat addr=0x80000000 disk=none block_transport=default l1d=on",
            stdout.getvalue(),
        )
        self.assertIn(
            "l1d-cache: enabled=true line_size=64 capacity_lines=64 loads=2 stores=1 hits=2 misses=1 evictions=0 bypasses=0 write_through_stores=1",
            stdout.getvalue(),
        )

    def test_l1d_opt_in_flat_probe_exposes_cache_counters(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = pathlib.Path(temp_dir)
            image = build_l1d_probe_flat_image(temp_path)

            proc = subprocess.run(
                [
                    "python3",
                    "workloads/run_debug_cli_probe.py",
                    "--target",
                    "./mycpu",
                    "--image",
                    str(image),
                    "--flat",
                    "--addr",
                    "0x80000000",
                    "--l1d",
                    "--step-cycles",
                    "16",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("l1d=on", proc.stdout)
        self.assertIn(
            "l1d-cache: enabled=true line_size=64 capacity_lines=64 loads=2 stores=1 hits=2 misses=1 evictions=0 bypasses=0 write_through_stores=1",
            proc.stdout,
        )

    def test_translation_plan_opt_in_flat_probe_reports_memory_boundary(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = pathlib.Path(temp_dir)
            image = build_translation_plan_probe_flat_image(temp_path)

            proc = subprocess.run(
                [
                    "python3",
                    "workloads/run_debug_cli_probe.py",
                    "--target",
                    "./mycpu",
                    "--image",
                    str(image),
                    "--flat",
                    "--addr",
                    "0x80000000",
                    "--translation-plan",
                    "--step-cycles",
                    "32",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("translation-candidate:", proc.stdout)
        self.assertIn("translation-plan: fallback", proc.stdout)
        self.assertIn("inlineable=1", proc.stdout)
        self.assertIn("reason=helper-required", proc.stdout)
        self.assertIn("boundary=memory-load", proc.stdout)

    def test_payload_actions_follow_primary_load_command(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--flat",
                "--addr",
                "0x80200000",
                "--payload",
                "board.dtb",
                "0x88000000",
                "--payload",
                "initrd.img",
                "0x84000000",
                "--step-cycles",
                "8",
            ]
        )

        commands = PROBE.build_commands(args)

        self.assertEqual(
            commands,
            [
                {
                    "cmd": "load",
                    "image": "Image",
                    "backend": "functional",
                    "flat": True,
                    "addr": 0x80200000,
                },
                {"cmd": "load_payload", "image": "board.dtb", "addr": 0x88000000},
                {"cmd": "load_payload", "image": "initrd.img", "addr": 0x84000000},
                {"cmd": "step_cycle", "count": 8},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_set_reg_actions_follow_load_stage_before_probe_steps(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--flat",
                "--addr",
                "0x80200000",
                "--payload",
                "board.dtb",
                "0x88000000",
                "--set-reg",
                "a0",
                "0x0",
                "--set-reg",
                "a1",
                "0x88000000",
                "--step-cycles",
                "8",
            ]
        )

        commands = PROBE.build_commands(args)

        self.assertEqual(
            commands,
            [
                {
                    "cmd": "load",
                    "image": "Image",
                    "backend": "functional",
                    "flat": True,
                    "addr": 0x80200000,
                },
                {"cmd": "load_payload", "image": "board.dtb", "addr": 0x88000000},
                {"cmd": "set_gpr", "reg": "a0", "value": 0x0},
                {"cmd": "set_gpr", "reg": "a1", "value": 0x88000000},
                {"cmd": "step_cycle", "count": 8},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_make_run_workload_forwards_payload_args(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "run-workload",
                "WORKLOAD_NAME=xv6",
                "WORKLOAD_IMAGE=tests/data/linux_like_image.bin",
                "WORKLOAD_BUILD_COMMAND=:",
                "WORKLOAD_PAYLOAD_ARGS=--payload board.dtb 0x88000000 --payload initrd.img 0x84000000",
                "WORKLOAD_PROBE_CYCLES=8",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("--payload board.dtb 0x88000000", proc.stdout)
        self.assertIn("--payload initrd.img 0x84000000", proc.stdout)

    def test_make_run_workload_forwards_gpr_seed_args(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "run-workload",
                "WORKLOAD_NAME=xv6",
                "WORKLOAD_IMAGE=tests/data/linux_like_image.bin",
                "WORKLOAD_BUILD_COMMAND=:",
                "WORKLOAD_GPR_SEED_ARGS=--set-reg a0 0x0 --set-reg a1 0x88000000",
                "WORKLOAD_PROBE_CYCLES=8",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("--set-reg a0 0x0", proc.stdout)
        self.assertIn("--set-reg a1 0x88000000", proc.stdout)

    def test_make_run_workload_linux_proto_derives_boot_contract_from_profile(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "-n",
                    "run-workload",
                    "WORKLOAD_NAME=linux_proto",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn(
            "make workloads/linux_proto/linux_sbi_shim.bin workloads/linux_proto/rootfs.cpio workloads/linux_proto/mycpu_virt.dtb",
            proc.stdout,
        )
        self.assertIn("--image workloads/linux_proto/linux_sbi_shim.bin", proc.stdout)
        self.assertIn("--flat", proc.stdout)
        self.assertIn("--addr 0x80000000", proc.stdout)
        self.assertIn(f"--payload {temp_dir}/arch/riscv/boot/Image 0x80200000", proc.stdout)
        self.assertIn("--payload workloads/linux_proto/mycpu_virt.dtb 0x87f00000", proc.stdout)
        self.assertIn("--payload workloads/linux_proto/rootfs.cpio 0x84000000", proc.stdout)
        self.assertIn("--set-reg a0 0x0", proc.stdout)
        self.assertIn("--set-reg a1 0x87f00000", proc.stdout)
        self.assertIn("--set-reg a2 0x80200000", proc.stdout)

    def test_make_build_workload_linux_proto_generates_fallback_initrd_and_dtb(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                "make",
                "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)

        generated_initrd = MYCPU_DIR / "workloads" / "linux_proto" / "rootfs.cpio"
        generated_dts = MYCPU_DIR / "workloads" / "linux_proto" / "mycpu_virt.dts"
        self.assertTrue(generated_initrd.exists())
        self.assertTrue(generated_dts.exists())
        initrd_listing = subprocess.run(
            ["cpio", "-it"],
            cwd=MYCPU_DIR,
            input=generated_initrd.read_bytes(),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        self.assertEqual(initrd_listing.returncode, 0, msg=initrd_listing.stderr.decode())
        self.assertIn("init", initrd_listing.stdout.decode())
        self.assertIn("post-init-smoke", initrd_listing.stdout.decode())
        self.assertIn("post-init-data.txt", initrd_listing.stdout.decode())
        generated_text = generated_dts.read_text()
        self.assertIn("linux,initrd-start = <0x0 0x84000000>;", generated_text)
        self.assertNotIn("linux,initrd-end = <0x0 0x84000000>;", generated_text)
        self.assertIn(f'riscv,isa = "{CURATED_ALPINE_GUEST_RISCV_ISA}";', generated_text)

    def test_make_run_workload_linux_proto_forwards_optional_disk_alias(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            disk_path = pathlib.Path(temp_dir) / "rootfs.img"
            disk_path.write_bytes(b"")
            proc = subprocess.run(
                [
                    "make",
                    "-n",
                    "run-workload",
                    "WORKLOAD_NAME=linux_proto",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                    f"LINUX_PROTO_DISK={disk_path}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn(f"--disk {disk_path}", proc.stdout)

    def test_make_build_workload_linux_proto_honors_bootargs_alias(self) -> None:
        bootargs = "console=ttyS0,115200 root=/dev/vda rw loglevel=7"
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                    f"LINUX_PROTO_BOOTARGS={bootargs}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        generated_dts = MYCPU_DIR / "workloads" / "linux_proto" / "mycpu_virt.dts"
        self.assertIn(bootargs, generated_dts.read_text())

    def test_make_build_workload_linux_proto_block_mode_generates_fallback_disk(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    "LINUX_PROTO_ROOTFS_MODE=block",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        generated_disk = MYCPU_DIR / "workloads" / "linux_proto" / "rootfs.ext4"
        self.assertTrue(generated_disk.exists())
        stat_init = subprocess.run(
            ["debugfs", "-R", "stat /init", str(generated_disk)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(stat_init.returncode, 0, msg=stat_init.stderr)
        stat_post_init = subprocess.run(
            ["debugfs", "-R", "stat /post-init-smoke", str(generated_disk)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(stat_post_init.returncode, 0, msg=stat_post_init.stderr)
        cat_post_init_data = subprocess.run(
            ["debugfs", "-R", "cat /post-init-data.txt", str(generated_disk)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(cat_post_init_data.returncode, 0, msg=cat_post_init_data.stderr)
        self.assertEqual(cat_post_init_data.stdout, "post-init-data-ok\n")
        generated_dts = MYCPU_DIR / "workloads" / "linux_proto" / "mycpu_virt.dts"
        generated_text = generated_dts.read_text()
        self.assertIn("root=/dev/vda", generated_text)
        self.assertNotIn("rdinit=/init", generated_text)
        self.assertIn("linux,initrd-end = <0x0 0x84000000>;", generated_text)
        self.assertIn(f'riscv,isa = "{CURATED_ALPINE_GUEST_RISCV_ISA}";', generated_text)

    def test_make_run_workload_linux_proto_block_mode_uses_generated_disk(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "-n",
                    "run-workload",
                    "WORKLOAD_NAME=linux_proto",
                    "LINUX_PROTO_ROOTFS_MODE=block",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("make workloads/linux_proto/linux_sbi_shim.bin workloads/linux_proto/rootfs.ext4 workloads/linux_proto/mycpu_virt.dtb", proc.stdout)
        self.assertIn("--disk workloads/linux_proto/rootfs.ext4", proc.stdout)
        self.assertNotIn("--payload workloads/linux_proto/rootfs.cpio 0x84000000", proc.stdout)

    def test_make_test_host_run_debug_cli_probe_linux_proto_runtime_target_requests_real_linux_runtime(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_proto_runtime",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_RUN_LINUX_PROTO_RUNTIME=1", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_proto_block_mode_runtime_reaches_fourth_stage_when_requested",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_runtime_target_requests_real_linux_distribution_runtime(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_runtime",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_filesystem_target_requests_profile(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_filesystem",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=filesystem_consistency", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_tty_login_target_requests_profile(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_tty_login",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=tty_login_probe", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_tty_login_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_tty_login",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS must point to an external distribution rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_process_control_target_requests_profile(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_process_control",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=process_control", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_process_control_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_process_control",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS must point to an external distribution rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_fs_state_guardrail_target_requests_profile(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_fs_state_guardrail",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=fs_state_guardrail", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_fs_state_guardrail_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_fs_state_guardrail",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS must point to an external distribution rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_filesystem_persistence_target_requests_temp_rootfs_profile(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_filesystem_persistence",
                "MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS=/tmp/source-rootfs.ext4",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS", proc.stdout)
        self.assertIn("mktemp", proc.stdout)
        self.assertIn('cp "/tmp/source-rootfs.ext4" "$temp_rootfs"', proc.stdout)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=filesystem_persistence", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_filesystem_persistence_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_filesystem_persistence",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS must point to an external distribution rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_shell_target_requests_curated_env(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_shell",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_IMAGE", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine", proc.stdout)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_debian_shell_target_requests_curated_env(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_curated_debian_shell",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_IMAGE", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_DEBIAN_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=debian", proc.stdout)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_shell_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_shell",
                "MYCPU_LINUX_DISTRO_CURATED_IMAGE=/tmp/Image",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_debian_shell_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_curated_debian_shell",
                "MYCPU_LINUX_DISTRO_CURATED_IMAGE=/tmp/Image",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_CURATED_DEBIAN_ROOTFS must point to an external Debian rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_matrix_target_runs_both_distros(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_curated_matrix",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("test-host-run_debug_cli_probe_linux_distribution_curated_alpine_shell", proc.stdout)
        self.assertIn("test-host-run_debug_cli_probe_linux_distribution_curated_debian_shell", proc.stdout)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_isa_advertisement_target_requests_curated_env(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_isa_advertisement",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_IMAGE", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine", proc.stdout)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_curated_alpine_boot_log_reports_compressed_isa",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_isa_advertisement_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_isa_advertisement",
                "MYCPU_LINUX_DISTRO_CURATED_IMAGE=/tmp/Image",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_busybox_awk_fp_target_requests_curated_env(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_busybox_awk_fp",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_IMAGE", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine", proc.stdout)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_curated_alpine_busybox_awk_fp_matrix",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_busybox_awk_fp_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_busybox_awk_fp",
                "MYCPU_LINUX_DISTRO_CURATED_IMAGE=/tmp/Image",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_proc_cpuinfo_isa_view_target_requests_curated_env(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_proc_cpuinfo_isa_view",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_IMAGE", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine", proc.stdout)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_curated_alpine_proc_cpuinfo_isa_view",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_proc_cpuinfo_isa_view_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_proc_cpuinfo_isa_view",
                "MYCPU_LINUX_DISTRO_CURATED_IMAGE=/tmp/Image",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_auxv_hwcap_view_target_requests_curated_env(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_auxv_hwcap_view",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_IMAGE", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine", proc.stdout)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_curated_alpine_auxv_hwcap_view",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_auxv_hwcap_view_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_auxv_hwcap_view",
                "MYCPU_LINUX_DISTRO_CURATED_IMAGE=/tmp/Image",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_busybox_userland_abi_view_target_requests_curated_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_busybox_userland_abi_view",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertNotIn("MYCPU_LINUX_DISTRO_CURATED_IMAGE", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_curated_alpine_busybox_userland_abi_view",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_busybox_userland_abi_view_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_busybox_userland_abi_view",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_fp_static_surface_view_target_requests_curated_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_fp_static_surface_view",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertNotIn("MYCPU_LINUX_DISTRO_CURATED_IMAGE", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_curated_alpine_fp_static_surface_view",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_fp_static_surface_view_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_fp_static_surface_view",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image",
            combined_output,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_fcsr_syscall_roundtrip_target_requests_curated_env(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_fcsr_syscall_roundtrip",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_IMAGE", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS", proc.stdout)
        self.assertIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", proc.stdout)
        self.assertIn("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine", proc.stdout)
        self.assertIn(
            "tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_distribution_runtime_curated_alpine_fcsr_syscall_roundtrip_probe",
            proc.stdout,
        )

    def test_make_test_host_run_debug_cli_probe_linux_distribution_curated_alpine_fcsr_syscall_roundtrip_target_fails_closed_without_rootfs(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "test-host-run_debug_cli_probe_linux_distribution_curated_alpine_fcsr_syscall_roundtrip",
                "MYCPU_LINUX_DISTRO_CURATED_IMAGE=/tmp/Image",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn(
            "MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS must point to an external Alpine rootfs image",
            combined_output,
        )
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)
        self.assertNotIn("MYCPU_RUN_LINUX_DISTRO_RUNTIME=1", combined_output)

    def test_make_build_workload_linux_proto_block_mode_embeds_mininit_stage_markers(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    "LINUX_PROTO_ROOTFS_MODE=block",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        mininit_elf = MYCPU_DIR / "workloads" / "linux_proto" / "linux_mininit.elf"
        strings_proc = subprocess.run(
            ["strings", str(mininit_elf)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(strings_proc.returncode, 0, msg=strings_proc.stderr)
        self.assertIn("mycpu linux initrd: stage=console-opened", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: stage=rootfs-rw-ok", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: stage=proc-readable", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: stage=sys-readable", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: stage=execve-post-init", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: write short", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: rootfs smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: proc smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: sys smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: exec post-init failed", strings_proc.stdout)
        self.assertNotIn("mycpu linux initrd: devtmpfs mount failed", strings_proc.stdout)

    def test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    "LINUX_PROTO_ROOTFS_MODE=block",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        post_init_elf = MYCPU_DIR / "workloads" / "linux_proto" / "linux_postinit_smoke.elf"
        strings_proc = subprocess.run(
            ["strings", str(post_init_elf)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(strings_proc.returncode, 0, msg=strings_proc.stderr)
        self.assertIn("mycpu linux userland: stage=file-readable", strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=rootfs-rw-roundtrip-ok", strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fork-child-wrote", strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=parent-wait4-ok", strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=execve-third-stage", strings_proc.stdout)
        self.assertIn("mycpu linux userland: file smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux userland: rootfs rw smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux userland: process smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux userland: exec third-stage failed", strings_proc.stdout)
        self.assertIn("mycpu linux userland: post-init reached", strings_proc.stdout)

        third_stage_elf = MYCPU_DIR / "workloads" / "linux_proto" / "linux_postinit_exec_smoke.elf"
        third_stage_strings_proc = subprocess.run(
            ["strings", str(third_stage_elf)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(third_stage_strings_proc.returncode, 0, msg=third_stage_strings_proc.stderr)
        self.assertIn("mycpu linux userland: stage=mkdir-chdir-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=nested-file-roundtrip-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=getdents64-nested-visible", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fstatat-nested-stat-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=renameat2-syscall-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=renameat2-nested-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=renameat2-dirent-updated", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=renameat2-cleanup-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-parent-dirent-gone", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=mkdirat-dir-name-reusable", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=mkdirat-reused-dir-empty", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=mkdirat-reused-dir-dot-only", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=mkdirat-reused-dir-parent-stat-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage dir smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage nested file smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage getdents64 smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage fstatat smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage renameat2 smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage renameat2 dirent smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage renameat2 cleanup failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage unlinkat dirent smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage mkdirat reuse smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage mkdirat reused dir smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage mkdirat reused dir dot-only smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage mkdirat reused dir parent stat smoke failed", third_stage_strings_proc.stdout)

        fourth_stage_elf = MYCPU_DIR / "workloads" / "linux_proto" / "linux_postinit_cleanup_smoke.elf"
        fourth_stage_strings_proc = subprocess.run(
            ["strings", str(fourth_stage_elf)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(fourth_stage_strings_proc.returncode, 0, msg=str(fourth_stage_strings_proc.stderr))
        self.assertIn("mycpu linux userland: stage=fourth-stage-entered", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fourth-stage-console-opened", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fourth-stage-console-fallback", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fourth-stage-root-chdir-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fourth-stage-unlinkat-reused-dir-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=symlinkat-target-readable", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fstatat-symlink-nofollow-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=readlinkat-target-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-symlink-dirent-gone", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-openat-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-fstatat-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-linkat-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-unlinkat-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-reopen-gone", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-fstatat-gone", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-getdents-gone", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=linkat-target-readable", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=linkat-shared-inode-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-origin-hardlink-survives", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-origin-link-count-dropped", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-hardlink-dirent-gone", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-nlink-zero", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-survives", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-anon-write-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-anon-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-survives", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-write-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-truncate-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-truncate-roundtrip-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-append-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-append-truncate-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-grow-zero-fill-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-grow-tail-write-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-fork-child-write-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-fork-parent-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-procfd-reopen-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-procfd-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-execve-child-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-execve-child-write-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-execve-parent-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-cloexec-child-closed-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-cloexec-parent-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=execveat-self-fd-child-entered-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=execveat-self-fd-parent-wait-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-write-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-private-cow-write-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-private-fd-unchanged-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-fork-child-write-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-fork-parent-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=pipe2-fork-child-write-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=pipe2-fork-parent-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=futex-shared-child-wake-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=futex-shared-parent-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=socketpair-fork-child-write-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=socketpair-fork-parent-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=openat2-self-beneath-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=pidfd-open-ppoll-waitid-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=signalfd-sigchld-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=renameat2-exchange-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=eventfd-epoll-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=scm-rights-parent-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=copy-file-range-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=splice-pipe-file-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=statx-size-mode-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=inotify-close-write-event-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=timerfd-one-shot-readback-ok", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-reused-dirent-gone", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fourth-stage-reached", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage reused dir cleanup failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage reused dir dirent smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage symlink smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage symlink stat smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage readlinkat smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage symlink cleanup failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage symlink dirent smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage dirfd openat smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage dirfd fstatat smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage dirfd linkat smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage dirfd unlinkat smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage dirfd reopen smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage dirfd gone-fstatat smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage dirfd getdents smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage hardlink smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage hardlink stat smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage hardlink survival smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage hardlink nlink smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage hardlink cleanup failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage hardlink dirent smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd fstat smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd survival smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd anon overwrite failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd anon stat smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd anon write smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd anon readback smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd dup smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd dup write smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd dup readback smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd dup truncate smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd dup truncate roundtrip failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd dup append failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd dup append truncate failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd dup grow zero-fill failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd dup grow tail write failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd dup fork smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd procfd smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd execve smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd cloexec smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage execveat self-fd smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd mmap smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd mmap private smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage open fd mmap fork smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage pipe2 smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage futex shared smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage socketpair smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage openat2 smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage pidfd smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage signalfd smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage renameat2 smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage eventfd epoll smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage scm-rights smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage copy-file-range smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage splice smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage statx smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage inotify smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: fourth-stage timerfd smoke failed", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: post-init reached", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: interactive console ready", fourth_stage_strings_proc.stdout)
        self.assertIn("mycpu-linux# ", fourth_stage_strings_proc.stdout)
        self.assertIn("commands: help uptime exit", fourth_stage_strings_proc.stdout)
        self.assertIn("uptime: post-init smoke complete", fourth_stage_strings_proc.stdout)
        self.assertIn("unknown command", fourth_stage_strings_proc.stdout)
        self.assertIn("mini_shell_exit", fourth_stage_strings_proc.stdout)

    def test_linux_proto_block_mode_runtime_reaches_fourth_stage_when_requested(self) -> None:
        if os.environ.get("MYCPU_RUN_LINUX_PROTO_RUNTIME") != "1":
            self.skipTest("set MYCPU_RUN_LINUX_PROTO_RUNTIME=1 to run the real linux_proto runtime guardrail")

        image_path = pathlib.Path(
            os.environ.get("MYCPU_LINUX_PROTO_RUNTIME_IMAGE", str(DEFAULT_LINUX_PROTO_RUNTIME_IMAGE))
        )
        if not image_path.is_file():
            self.fail(f"missing linux_proto runtime Image: {image_path}")

        build_proc = subprocess.run(
            [
                "make",
                "build-workload",
                "WORKLOAD_NAME=linux_proto",
                "LINUX_PROTO_ROOTFS_MODE=block",
                f"LINUX_PROTO_IMAGE={image_path}",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(build_proc.returncode, 0, msg=build_proc.stderr)

        probe_proc = subprocess.run(
            [
                "python3",
                "workloads/run_debug_cli_probe.py",
                "--target",
                "./mycpu",
                "--image",
                "workloads/linux_proto/linux_sbi_shim.bin",
                "--flat",
                "--addr",
                "0x80000000",
                "--payload",
                str(image_path),
                "0x80200000",
                "--payload",
                "workloads/linux_proto/mycpu_virt.dtb",
                "0x87f00000",
                "--disk",
                "workloads/linux_proto/rootfs.ext4",
                "--block-transport",
                "virtio-blk",
                "--set-reg",
                "a0",
                "0x0",
                "--set-reg",
                "a1",
                "0x87f00000",
                "--set-reg",
                "a2",
                "0x80200000",
                "--uart-wait",
                "mycpu linux userland: post-init reached",
                "300000000",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(probe_proc.returncode, 0, msg=probe_proc.stderr)
        self.assertIn("mycpu linux userland: stage=fourth-stage-entered", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=symlinkat-target-readable", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fstatat-symlink-nofollow-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=readlinkat-target-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-symlink-dirent-gone", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-openat-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-fstatat-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-linkat-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-unlinkat-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-reopen-gone", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-fstatat-gone", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=dirfd-relative-getdents-gone", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=linkat-target-readable", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=linkat-shared-inode-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-origin-hardlink-survives", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-origin-link-count-dropped", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-hardlink-dirent-gone", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-nlink-zero", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-survives", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-anon-write-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-anon-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-survives", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-write-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-truncate-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-truncate-roundtrip-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-append-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-append-truncate-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-grow-zero-fill-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-grow-tail-write-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-fork-child-write-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-dup-fork-parent-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-procfd-reopen-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-procfd-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-execve-child-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-execve-child-write-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-execve-parent-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-cloexec-child-closed-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-cloexec-parent-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=execveat-self-fd-child-entered-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=execveat-self-fd-parent-wait-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-write-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-private-cow-write-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-private-fd-unchanged-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-fork-child-write-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-fork-parent-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=pipe2-fork-child-write-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=pipe2-fork-parent-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=futex-shared-child-wake-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=futex-shared-parent-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=socketpair-fork-child-write-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=socketpair-fork-parent-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=openat2-self-beneath-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=pidfd-open-ppoll-waitid-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=signalfd-sigchld-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=renameat2-exchange-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=eventfd-epoll-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=scm-rights-parent-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=copy-file-range-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=splice-pipe-file-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=statx-size-mode-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=inotify-close-write-event-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=timerfd-one-shot-readback-ok", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-reused-dirent-gone", probe_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fourth-stage-reached", probe_proc.stdout)
        self.assertIn("mycpu linux userland: post-init reached", probe_proc.stdout)

    def test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested(self) -> None:
        if os.environ.get("MYCPU_RUN_LINUX_DISTRO_RUNTIME") != "1":
            self.skipTest("set MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 to run the real Linux distribution shell guardrail")

        contract = resolve_linux_distro_shell_contract()
        image_path = contract["image"]
        disk_path = contract["disk"]
        prompt = str(contract["prompt"])
        command = str(contract["command"])
        expected = str(contract["expected"])
        bootargs = str(contract["bootargs"])
        command_contracts = linux_distro_command_contracts(command, expected)

        if not image_path.is_file():
            self.fail(f"missing linux distribution runtime Image: {image_path}")
        if disk_path == DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS:
            self.fail(
                "linux distribution runtime requires explicit external MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS; "
                "repo linux_proto rootfs is only for the mini shell guardrail"
            )

        build_command = [
            "make",
            "build-workload",
            "WORKLOAD_NAME=linux_proto",
            "LINUX_PROTO_ROOTFS_MODE=block",
            f"LINUX_PROTO_IMAGE={image_path}",
            f"LINUX_PROTO_DISK={disk_path}",
        ]
        if bootargs:
            build_command.append(f"LINUX_PROTO_BOOTARGS={bootargs}")

        build_proc = subprocess.run(
            build_command,
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(build_proc.returncode, 0, msg=build_proc.stderr)

        with subprocess.Popen(
            ["./mycpu", "--debug-cli"],
            cwd=MYCPU_DIR,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        ) as proc:
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load",
                    "image": "workloads/linux_proto/linux_sbi_shim.bin",
                    "backend": "functional",
                    "disk": str(disk_path),
                    "block_transport": "virtio-blk",
                    "flat": True,
                    "addr": 0x80000000,
                },
            )
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load_payload",
                    "image": str(image_path),
                    "addr": 0x80200000,
                },
            )
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load_payload",
                    "image": "workloads/linux_proto/mycpu_virt.dtb",
                    "addr": 0x87F00000,
                },
            )
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a0", "value": 0x0})
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a1", "value": 0x87F00000})
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a2", "value": 0x80200000})
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "run_until_uart_contains",
                    "text": prompt,
                    "max_steps": 300000000,
                },
            )
            boot_chunk = debug_cli_roundtrip(proc, {"cmd": "uart_output", "offset": 0})
            self.assertIn(prompt, boot_chunk.get("text", ""))
            offset = normalize_next_offset(boot_chunk)

            for command_text, expected_text in command_contracts:
                shell_input = command_text if command_text.endswith(("\r", "\n")) else f"{command_text}\r"
                debug_cli_roundtrip(proc, {"cmd": "uart_input", "text": shell_input})
                try:
                    command_chunk = debug_cli_roundtrip(
                        proc,
                        {
                            "cmd": "run_until_new_uart_contains",
                            "offset": offset,
                            "text": prompt,
                            "max_steps": 50000000,
                        },
                    )
                except AssertionError as exc:
                    recent_text = ""
                    with contextlib.suppress(AssertionError):
                        recent_output = debug_cli_roundtrip(proc, {"cmd": "uart_output", "offset": offset})
                        recent_text = recent_output.get("text", "")
                    self.fail(
                        f"linux distribution command did not return to prompt: {command_text!r}\n"
                        f"recent UART output:\n{recent_text}\n"
                        f"error: {exc}"
                    )
                command_text_output = command_chunk.get("text", "")
                self.assertIn(expected_text, command_text_output)
                self.assertTrue(
                    command_text_output.endswith(prompt),
                    msg=f"expected shell chunk to end with prompt {prompt!r}, got {command_text_output!r}",
                )
                offset = normalize_next_offset(command_chunk)

            debug_cli_roundtrip(proc, {"cmd": "quit"})
            proc.wait(timeout=5)
            stderr = proc.stderr.read() if proc.stderr is not None else ""
            self.assertEqual(proc.returncode, 0, msg=stderr)

    def test_linux_distribution_runtime_curated_alpine_boot_log_reports_compressed_isa(self) -> None:
        if os.environ.get("MYCPU_RUN_LINUX_DISTRO_RUNTIME") != "1":
            self.skipTest("set MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 to run the real Linux distribution ISA advertisement guardrail")

        contract = resolve_linux_distro_shell_contract()
        image_path = contract["image"]
        disk_path = contract["disk"]
        prompt = str(contract["prompt"])
        bootargs = str(contract["bootargs"])

        if os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO") != "alpine":
            self.skipTest("set MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine for the Alpine ISA advertisement guardrail")
        if not image_path.is_file():
            self.fail(f"missing linux distribution runtime Image: {image_path}")
        if disk_path == DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS:
            self.fail(
                "linux distribution runtime requires explicit external MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS; "
                "repo linux_proto rootfs is only for the mini shell guardrail"
            )

        build_command = [
            "make",
            "build-workload",
            "WORKLOAD_NAME=linux_proto",
            "LINUX_PROTO_ROOTFS_MODE=block",
            f"LINUX_PROTO_IMAGE={image_path}",
            f"LINUX_PROTO_DISK={disk_path}",
        ]
        if bootargs:
            build_command.append(f"LINUX_PROTO_BOOTARGS={bootargs}")

        build_proc = subprocess.run(
            build_command,
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(build_proc.returncode, 0, msg=build_proc.stderr)

        with subprocess.Popen(
            ["./mycpu", "--debug-cli"],
            cwd=MYCPU_DIR,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        ) as proc:
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load",
                    "image": "workloads/linux_proto/linux_sbi_shim.bin",
                    "backend": "functional",
                    "disk": str(disk_path),
                    "block_transport": "virtio-blk",
                    "flat": True,
                    "addr": 0x80000000,
                },
            )
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load_payload",
                    "image": str(image_path),
                    "addr": 0x80200000,
                },
            )
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load_payload",
                    "image": "workloads/linux_proto/mycpu_virt.dtb",
                    "addr": 0x87F00000,
                },
            )
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a0", "value": 0x0})
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a1", "value": 0x87F00000})
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a2", "value": 0x80200000})
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "run_until_uart_contains",
                    "text": prompt,
                    "max_steps": 300000000,
                },
            )
            boot_chunk = debug_cli_roundtrip(proc, {"cmd": "uart_output", "offset": 0})
            boot_text = boot_chunk.get("text", "")
            base_isa_match = re.search(r"riscv: base ISA extensions ([a-z0-9_]+)", boot_text)
            self.assertIsNotNone(base_isa_match, msg=boot_text)
            base_isa = base_isa_match.group(1)
            self.assertEqual(set(base_isa), CURATED_ALPINE_BASE_ISA_EXTENSIONS)

            elf_caps_match = re.search(r"riscv: ELF capabilities ([a-z0-9_]+)", boot_text)
            self.assertIsNotNone(elf_caps_match, msg=boot_text)
            elf_caps = elf_caps_match.group(1)
            self.assertEqual(set(elf_caps), CURATED_ALPINE_BASE_ISA_EXTENSIONS)
            self.assertIn(prompt, boot_text)

            debug_cli_roundtrip(proc, {"cmd": "quit"})
            proc.wait(timeout=5)
            stderr = proc.stderr.read() if proc.stderr is not None else ""
            self.assertEqual(proc.returncode, 0, msg=stderr)

    def test_linux_distribution_runtime_curated_alpine_busybox_awk_fp_matrix(self) -> None:
        if os.environ.get("MYCPU_RUN_LINUX_DISTRO_RUNTIME") != "1":
            self.skipTest("set MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 to run the real Linux distribution BusyBox awk FP guardrail")

        contract = resolve_linux_distro_shell_contract()
        image_path = contract["image"]
        disk_path = contract["disk"]
        prompt = str(contract["prompt"])
        bootargs = str(contract["bootargs"])

        if os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO") != "alpine":
            self.skipTest("set MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine for the Alpine BusyBox awk FP guardrail")
        if not image_path.is_file():
            self.fail(f"missing linux distribution runtime Image: {image_path}")
        if disk_path == DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS:
            self.fail(
                "linux distribution runtime requires explicit external MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS; "
                "repo linux_proto rootfs is only for the mini shell guardrail"
            )

        build_command = [
            "make",
            "build-workload",
            "WORKLOAD_NAME=linux_proto",
            "LINUX_PROTO_ROOTFS_MODE=block",
            f"LINUX_PROTO_IMAGE={image_path}",
            f"LINUX_PROTO_DISK={disk_path}",
        ]
        if bootargs:
            build_command.append(f"LINUX_PROTO_BOOTARGS={bootargs}")

        build_proc = subprocess.run(
            build_command,
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(build_proc.returncode, 0, msg=build_proc.stderr)

        with subprocess.Popen(
            ["./mycpu", "--debug-cli"],
            cwd=MYCPU_DIR,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        ) as proc:
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load",
                    "image": "workloads/linux_proto/linux_sbi_shim.bin",
                    "backend": "functional",
                    "disk": str(disk_path),
                    "block_transport": "virtio-blk",
                    "flat": True,
                    "addr": 0x80000000,
                },
            )
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load_payload",
                    "image": str(image_path),
                    "addr": 0x80200000,
                },
            )
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load_payload",
                    "image": "workloads/linux_proto/mycpu_virt.dtb",
                    "addr": 0x87F00000,
                },
            )
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a0", "value": 0x0})
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a1", "value": 0x87F00000})
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a2", "value": 0x80200000})
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "run_until_uart_contains",
                    "text": prompt,
                    "max_steps": 300000000,
                },
            )
            boot_chunk = debug_cli_roundtrip(proc, {"cmd": "uart_output", "offset": 0})
            self.assertIn(prompt, boot_chunk.get("text", ""))
            offset = normalize_next_offset(boot_chunk)
            awk_contracts = [
                ("awk 'BEGIN{print 1.5+2.25}'", "3.75"),
                ("awk 'BEGIN{ if ((1.5+2.25)==3.75) print 11; else print 22 }'", "11"),
                ("awk 'BEGIN{print 7/2}'", "3.5"),
                ("awk 'BEGIN{printf \"%d\\n\", 7/2}'", "3"),
                ("awk 'BEGIN{printf \"%u\\n\", 7/2}'", "3"),
                ("awk 'BEGIN{printf \"%x\\n\", 15/2}'", "7"),
                ("awk 'BEGIN{printf \"%.0f %.0f\\n\", 2.5, 3.5}'", "2 4"),
                ("awk 'BEGIN{printf \"%.1f %.1f %.1f\\n\", 2.25, 2.35, -2.35}'", "2.2 2.4 -2.4"),
                ("awk 'BEGIN{print 1e308*1e308}'", "inf"),
                ("awk 'BEGIN{if ((1e-308*1e-308)==0) print 1; else print 0}'", "1"),
                ("awk 'BEGIN{if (1.5 < 2.25) print 1; else print 0}'", "1"),
                ("awk 'BEGIN{if (2.25 <= 2.25) print 1; else print 0}'", "1"),
                ("awk 'BEGIN{if (2.25 > 1.5) print 1; else print 0}'", "1"),
                ("awk 'BEGIN{printf \"%u\\n\", -1}'", "4294967295"),
                ("awk 'BEGIN{printf \"%x\\n\", -1}'", "ffffffff"),
                ("awk 'BEGIN{print sqrt(2)}'", "1.41421"),
                ("awk 'BEGIN{print sqrt(-1)}'", "nan"),
            ]
            for command_text, expected_text in awk_contracts:
                debug_cli_roundtrip(proc, {"cmd": "uart_input", "text": f"{command_text}\r"})
                command_chunk = debug_cli_roundtrip(
                    proc,
                    {
                        "cmd": "run_until_new_uart_contains",
                        "offset": offset,
                        "text": prompt,
                        "max_steps": 50000000,
                    },
                )
                command_output = command_chunk.get("text", "")
                self.assertIn(expected_text, command_output)
                self.assertTrue(
                    command_output.endswith(prompt),
                    msg=f"expected shell chunk to end with prompt {prompt!r}, got {command_output!r}",
                )
                offset = normalize_next_offset(command_chunk)

            debug_cli_roundtrip(proc, {"cmd": "quit"})
            proc.wait(timeout=5)
            stderr = proc.stderr.read() if proc.stderr is not None else ""
            self.assertEqual(proc.returncode, 0, msg=stderr)

    def test_linux_distribution_runtime_curated_alpine_proc_cpuinfo_isa_view(self) -> None:
        if os.environ.get("MYCPU_RUN_LINUX_DISTRO_RUNTIME") != "1":
            self.skipTest("set MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 to run the real Linux distribution procfs ISA-view guardrail")

        contract = resolve_linux_distro_shell_contract()
        image_path = contract["image"]
        disk_path = contract["disk"]
        prompt = str(contract["prompt"])
        bootargs = str(contract["bootargs"])

        if os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO") != "alpine":
            self.skipTest("set MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine for the Alpine procfs ISA-view guardrail")
        if not image_path.is_file():
            self.fail(f"missing linux distribution runtime Image: {image_path}")
        if disk_path == DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS:
            self.fail(
                "linux distribution runtime requires explicit external MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS; "
                "repo linux_proto rootfs is only for the mini shell guardrail"
            )

        build_command = [
            "make",
            "build-workload",
            "WORKLOAD_NAME=linux_proto",
            "LINUX_PROTO_ROOTFS_MODE=block",
            f"LINUX_PROTO_IMAGE={image_path}",
            f"LINUX_PROTO_DISK={disk_path}",
        ]
        if bootargs:
            build_command.append(f"LINUX_PROTO_BOOTARGS={bootargs}")

        build_proc = subprocess.run(
            build_command,
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(build_proc.returncode, 0, msg=build_proc.stderr)

        with subprocess.Popen(
            ["./mycpu", "--debug-cli"],
            cwd=MYCPU_DIR,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        ) as proc:
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load",
                    "image": "workloads/linux_proto/linux_sbi_shim.bin",
                    "backend": "functional",
                    "disk": str(disk_path),
                    "block_transport": "virtio-blk",
                    "flat": True,
                    "addr": 0x80000000,
                },
            )
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load_payload",
                    "image": str(image_path),
                    "addr": 0x80200000,
                },
            )
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load_payload",
                    "image": "workloads/linux_proto/mycpu_virt.dtb",
                    "addr": 0x87F00000,
                },
            )
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a0", "value": 0x0})
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a1", "value": 0x87F00000})
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a2", "value": 0x80200000})
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "run_until_uart_contains",
                    "text": prompt,
                    "max_steps": 300000000,
                },
            )
            boot_chunk = debug_cli_roundtrip(proc, {"cmd": "uart_output", "offset": 0})
            self.assertIn(prompt, boot_chunk.get("text", ""))
            offset = normalize_next_offset(boot_chunk)

            procfs_contracts = [
                ("mount -t proc proc /proc", prompt),
                ("grep '^isa' /proc/cpuinfo", CURATED_ALPINE_PROC_CPUINFO_ISA),
            ]
            for command_text, expected_text in procfs_contracts:
                debug_cli_roundtrip(proc, {"cmd": "uart_input", "text": f"{command_text}\r"})
                command_chunk = debug_cli_roundtrip(
                    proc,
                    {
                        "cmd": "run_until_new_uart_contains",
                        "offset": offset,
                        "text": prompt,
                        "max_steps": 50000000,
                    },
                )
                command_output = command_chunk.get("text", "")
                self.assertIn(expected_text, command_output)
                self.assertTrue(
                    command_output.endswith(prompt),
                    msg=f"expected shell chunk to end with prompt {prompt!r}, got {command_output!r}",
                )
                offset = normalize_next_offset(command_chunk)

            debug_cli_roundtrip(proc, {"cmd": "quit"})
            proc.wait(timeout=5)
            stderr = proc.stderr.read() if proc.stderr is not None else ""
            self.assertEqual(proc.returncode, 0, msg=stderr)

    def test_linux_distribution_runtime_curated_alpine_auxv_hwcap_view(self) -> None:
        if os.environ.get("MYCPU_RUN_LINUX_DISTRO_RUNTIME") != "1":
            self.skipTest("set MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 to run the real Linux distribution auxv HWCAP guardrail")

        contract = resolve_linux_distro_shell_contract()
        image_path = contract["image"]
        disk_path = contract["disk"]
        prompt = str(contract["prompt"])
        bootargs = str(contract["bootargs"])

        if os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO") != "alpine":
            self.skipTest("set MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine for the Alpine auxv HWCAP guardrail")
        if not image_path.is_file():
            self.fail(f"missing linux distribution runtime Image: {image_path}")
        if disk_path == DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS:
            self.fail(
                "linux distribution runtime requires explicit external MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS; "
                "repo linux_proto rootfs is only for the mini shell guardrail"
            )

        build_command = [
            "make",
            "build-workload",
            "WORKLOAD_NAME=linux_proto",
            "LINUX_PROTO_ROOTFS_MODE=block",
            f"LINUX_PROTO_IMAGE={image_path}",
            f"LINUX_PROTO_DISK={disk_path}",
        ]
        if bootargs:
            build_command.append(f"LINUX_PROTO_BOOTARGS={bootargs}")

        build_proc = subprocess.run(
            build_command,
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(build_proc.returncode, 0, msg=build_proc.stderr)

        with subprocess.Popen(
            ["./mycpu", "--debug-cli"],
            cwd=MYCPU_DIR,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        ) as proc:
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load",
                    "image": "workloads/linux_proto/linux_sbi_shim.bin",
                    "backend": "functional",
                    "disk": str(disk_path),
                    "block_transport": "virtio-blk",
                    "flat": True,
                    "addr": 0x80000000,
                },
            )
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load_payload",
                    "image": str(image_path),
                    "addr": 0x80200000,
                },
            )
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "load_payload",
                    "image": "workloads/linux_proto/mycpu_virt.dtb",
                    "addr": 0x87F00000,
                },
            )
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a0", "value": 0x0})
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a1", "value": 0x87F00000})
            debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a2", "value": 0x80200000})
            debug_cli_roundtrip(
                proc,
                {
                    "cmd": "run_until_uart_contains",
                    "text": prompt,
                    "max_steps": 300000000,
                },
            )
            boot_chunk = debug_cli_roundtrip(proc, {"cmd": "uart_output", "offset": 0})
            self.assertIn(prompt, boot_chunk.get("text", ""))
            offset = normalize_next_offset(boot_chunk)

            auxv_contracts = [
                ("mount -t proc proc /proc", prompt),
                ('od -An -tx8 -w16 /proc/self/auxv | sed -n "1,32p"', CURATED_ALPINE_AT_HWCAP_AUXV_LINE),
            ]
            for command_text, expected_text in auxv_contracts:
                debug_cli_roundtrip(proc, {"cmd": "uart_input", "text": f"{command_text}\r"})
                command_chunk = debug_cli_roundtrip(
                    proc,
                    {
                        "cmd": "run_until_new_uart_contains",
                        "offset": offset,
                        "text": prompt,
                        "max_steps": 50000000,
                    },
                )
                command_output = command_chunk.get("text", "")
                self.assertIn(expected_text, command_output)
                self.assertTrue(
                    command_output.endswith(prompt),
                    msg=f"expected shell chunk to end with prompt {prompt!r}, got {command_output!r}",
                )
                offset = normalize_next_offset(command_chunk)

            debug_cli_roundtrip(proc, {"cmd": "quit"})
            proc.wait(timeout=5)
            stderr = proc.stderr.read() if proc.stderr is not None else ""
            self.assertEqual(proc.returncode, 0, msg=stderr)

    def test_linux_distribution_curated_alpine_busybox_userland_abi_view(self) -> None:
        if os.environ.get("MYCPU_RUN_LINUX_DISTRO_RUNTIME") != "1":
            self.skipTest(
                "set MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 to run the Alpine userland ABI guardrail"
            )

        rootfs_text = os.environ.get("MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS", "").strip()
        if not rootfs_text:
            self.skipTest(
                "set MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS to run the Alpine userland ABI guardrail"
            )

        disk_path = pathlib.Path(rootfs_text)
        if not disk_path.is_file():
            self.fail(f"missing external Alpine rootfs image: {disk_path}")

        with extract_file_from_ext4_image(disk_path, "/bin/busybox", "busybox") as busybox_path:
            header_proc = subprocess.run(
                ["readelf", "-h", str(busybox_path)],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(header_proc.returncode, 0, msg=header_proc.stderr)
            self.assertIn("Machine:                           RISC-V", header_proc.stdout)
            self.assertIn("Flags:                             0x5, RVC, double-float ABI", header_proc.stdout)

            attribute_proc = subprocess.run(
                ["readelf", "-A", str(busybox_path)],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(attribute_proc.returncode, 0, msg=attribute_proc.stderr)
            self.assertIn(
                'Tag_RISCV_arch: "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0',
                attribute_proc.stdout,
            )

    def test_linux_distribution_curated_alpine_fp_static_surface_view(self) -> None:
        if os.environ.get("MYCPU_RUN_LINUX_DISTRO_RUNTIME") != "1":
            self.skipTest(
                "set MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 to run the Alpine FP static-surface guardrail"
            )

        rootfs_text = os.environ.get("MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS", "").strip()
        if not rootfs_text:
            self.skipTest(
                "set MYCPU_LINUX_DISTRO_CURATED_ALPINE_ROOTFS to run the Alpine FP static-surface guardrail"
            )

        disk_path = pathlib.Path(rootfs_text)
        if not disk_path.is_file():
            self.fail(f"missing external Alpine rootfs image: {disk_path}")

        with extract_file_from_ext4_image(disk_path, "/bin/busybox", "busybox") as busybox_path:
            with extract_file_from_ext4_image(
                disk_path,
                "/lib/ld-musl-riscv64.so.1",
                "ld-musl-riscv64.so.1",
            ) as ld_musl_path:
                observed_surfaces = {
                    "/bin/busybox": extract_riscv_static_surface_mnemonics(busybox_path),
                    "/lib/ld-musl-riscv64.so.1": extract_riscv_static_surface_mnemonics(ld_musl_path),
                }

        supported_surface = supported_riscv_userland_fp_mnemonics()
        required_surface = {
            "/bin/busybox": {"fcvt.wu.d", "fcvt.lu.d", "flt.d", "fmadd.d"},
            "/lib/ld-musl-riscv64.so.1": {"fadd.s", "fclass.d", "fsqrt.s", "frcsr", "fscsr"},
        }
        for guest_path, required_mnemonics in required_surface.items():
            observed = observed_surfaces[guest_path]
            self.assertTrue(
                required_mnemonics.issubset(observed),
                msg=(
                    f"{guest_path} no longer exposes the expected Alpine userland FP anchors; "
                    f"missing={sorted(required_mnemonics - observed)} observed={sorted(observed)}"
                ),
            )
            unsupported = sorted(observed - supported_surface)
            self.assertEqual(
                unsupported,
                [],
                msg=(
                    f"{guest_path} uses FP/userland mnemonics outside myCPU's current declared support surface: "
                    f"{unsupported}"
                ),
            )

    def test_linux_distribution_curated_alpine_capability_alignment_contract(self) -> None:
        supported_surface = supported_riscv_userland_fp_mnemonics()

        self.assertIn("fadd.s", supported_surface)
        self.assertIn("fadd.d", supported_surface)
        self.assertIn("c.fld", supported_surface)
        self.assertIn("c.fldsp", supported_surface)
        self.assertIn("c.fsd", supported_surface)
        self.assertIn("c.fsdsp", supported_surface)
        self.assertIn("fld", supported_surface)
        self.assertIn("fsd", supported_surface)
        self.assertEqual(CURATED_ALPINE_GUEST_RISCV_ISA, "rv64imafdc_zicsr_zifencei")
        self.assertEqual(CURATED_ALPINE_PROC_CPUINFO_ISA, "rv64imafdc_zicntr_zicsr_zifencei_zihpm")
        self.assertEqual(CURATED_ALPINE_AT_HWCAP, 0x112D)
        self.assertEqual(
            CURATED_ALPINE_AT_HWCAP,
            (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 8) | (1 << 12),
        )
        self.assertEqual(CURATED_ALPINE_BASE_ISA_EXTENSIONS, set("acdfim"))

    def test_linux_distribution_runtime_curated_alpine_fcsr_syscall_roundtrip_probe(self) -> None:
        if os.environ.get("MYCPU_RUN_LINUX_DISTRO_RUNTIME") != "1":
            self.skipTest(
                "set MYCPU_RUN_LINUX_DISTRO_RUNTIME=1 to run the real Linux distribution fcsr syscall roundtrip guardrail"
            )

        contract = resolve_linux_distro_shell_contract()
        image_path = contract["image"]
        disk_path = contract["disk"]
        prompt = str(contract["prompt"])
        bootargs = str(contract["bootargs"])

        if os.environ.get("MYCPU_LINUX_DISTRO_RUNTIME_DISTRO") != "alpine":
            self.skipTest(
                "set MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine for the Alpine fcsr syscall roundtrip guardrail"
            )
        if not image_path.is_file():
            self.fail(f"missing linux distribution runtime Image: {image_path}")
        if disk_path == DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS:
            self.fail(
                "linux distribution runtime requires explicit external MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS; "
                "repo linux_proto rootfs is only for the mini shell guardrail"
            )

        with tempfile.TemporaryDirectory(prefix="mycpu-fcsr-probe.") as temp_dir:
            temp_path = pathlib.Path(temp_dir)
            probe_path = build_linux_fcsr_syscall_roundtrip_probe(temp_path)
            with mutable_ext4_image_copy(disk_path) as temp_rootfs:
                install_file_into_ext4_image(
                    temp_rootfs,
                    probe_path,
                    "/tmp/mycpu-fcsr-syscall-roundtrip",
                    mode="0100755",
                )

                build_command = [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    "LINUX_PROTO_ROOTFS_MODE=block",
                    f"LINUX_PROTO_IMAGE={image_path}",
                    f"LINUX_PROTO_DISK={temp_rootfs}",
                ]
                if bootargs:
                    build_command.append(f"LINUX_PROTO_BOOTARGS={bootargs}")

                build_proc = subprocess.run(
                    build_command,
                    cwd=MYCPU_DIR,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    check=False,
                )
                self.assertEqual(build_proc.returncode, 0, msg=build_proc.stderr)

                with subprocess.Popen(
                    ["./mycpu", "--debug-cli"],
                    cwd=MYCPU_DIR,
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    bufsize=1,
                ) as proc:
                    debug_cli_roundtrip(
                        proc,
                        {
                            "cmd": "load",
                            "image": "workloads/linux_proto/linux_sbi_shim.bin",
                            "backend": "functional",
                            "disk": str(temp_rootfs),
                            "block_transport": "virtio-blk",
                            "flat": True,
                            "addr": 0x80000000,
                        },
                    )
                    debug_cli_roundtrip(
                        proc,
                        {
                            "cmd": "load_payload",
                            "image": str(image_path),
                            "addr": 0x80200000,
                        },
                    )
                    debug_cli_roundtrip(
                        proc,
                        {
                            "cmd": "load_payload",
                            "image": "workloads/linux_proto/mycpu_virt.dtb",
                            "addr": 0x87F00000,
                        },
                    )
                    debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a0", "value": 0x0})
                    debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a1", "value": 0x87F00000})
                    debug_cli_roundtrip(proc, {"cmd": "set_gpr", "reg": "a2", "value": 0x80200000})
                    debug_cli_roundtrip(
                        proc,
                        {
                            "cmd": "run_until_uart_contains",
                            "text": prompt,
                            "max_steps": 300000000,
                        },
                    )
                    boot_chunk = debug_cli_roundtrip(proc, {"cmd": "uart_output", "offset": 0})
                    self.assertIn(prompt, boot_chunk.get("text", ""))
                    offset = normalize_next_offset(boot_chunk)

                    command_text = (
                        "/tmp/mycpu-fcsr-syscall-roundtrip; "
                        'printf " probe-status:%s" "$?"'
                    )
                    debug_cli_roundtrip(proc, {"cmd": "uart_input", "text": f"{command_text}\r"})
                    command_chunk = debug_cli_roundtrip(
                        proc,
                        {
                            "cmd": "run_until_new_uart_contains",
                            "offset": offset,
                            "text": prompt,
                            "max_steps": 50000000,
                        },
                    )
                    command_output = command_chunk.get("text", "")
                    self.assertIn("mycpu-fcsr-syscall-roundtrip:ok", command_output)
                    self.assertIn("probe-status:0", command_output)
                    self.assertTrue(
                        command_output.endswith(prompt),
                        msg=f"expected shell chunk to end with prompt {prompt!r}, got {command_output!r}",
                    )

                    debug_cli_roundtrip(proc, {"cmd": "quit"})
                    proc.wait(timeout=5)
                    stderr = proc.stderr.read() if proc.stderr is not None else ""
                    self.assertEqual(proc.returncode, 0, msg=stderr)

    def test_make_build_workload_linux_proto_block_mode_mininit_message_lengths_exclude_nul(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    "LINUX_PROTO_ROOTFS_MODE=block",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        mininit_elf = MYCPU_DIR / "workloads" / "linux_proto" / "linux_mininit.elf"
        nm_proc = subprocess.run(
            ["riscv64-unknown-elf-nm", "-n", str(mininit_elf)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(nm_proc.returncode, 0, msg=nm_proc.stderr)
        expected_lengths = {
            "stage_console_opened_msg_len": len("mycpu linux initrd: stage=console-opened\n".encode()),
            "stage_rootfs_rw_ok_msg_len": len("mycpu linux initrd: stage=rootfs-rw-ok\n".encode()),
            "stage_proc_readable_msg_len": len("mycpu linux initrd: stage=proc-readable\n".encode()),
            "stage_sys_readable_msg_len": len("mycpu linux initrd: stage=sys-readable\n".encode()),
            "boot_ok_msg_len": len("mycpu linux initrd: /init reached\n".encode()),
        }
        observed_lengths = {}
        for line in nm_proc.stdout.splitlines():
            parts = line.split()
            if len(parts) != 3:
                continue
            symbol = parts[2]
            if symbol not in expected_lengths:
                continue
            observed_lengths[symbol] = int(parts[0], 16)

        self.assertEqual(observed_lengths, expected_lengths)

    def test_make_run_workload_linux_proto_reports_missing_assets_via_probe(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "run-workload",
                    "WORKLOAD_NAME=linux_proto",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn("missing input files:", combined_output)
        self.assertIn(f"payload: {temp_dir}/arch/riscv/boot/Image", combined_output)
        self.assertNotIn("No rule to make target", combined_output)


if __name__ == "__main__":
    unittest.main()
