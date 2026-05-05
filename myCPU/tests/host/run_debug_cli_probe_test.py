#!/usr/bin/env python3
import contextlib
import importlib.util
import io
import json
import os
import pathlib
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


def resolve_linux_distro_shell_contract() -> dict:
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


class RunDebugCliProbeTest(unittest.TestCase):
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
