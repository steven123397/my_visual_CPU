import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


def make(*args, cwd=ROOT):
    env = os.environ.copy()
    env.pop("MAKEFLAGS", None)
    return subprocess.run(
        ["make", "--no-print-directory", *args], cwd=cwd, env=env,
        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60,
    )


class CiTargetsTest(unittest.TestCase):
    def checked_make(self, *args):
        result = make(*args)
        self.assertEqual(result.returncode, 0, result.stdout)
        return result.stdout

    def test_default_units_are_executed(self):
        names = self.checked_make(
            "--eval=ci-print-units:;@echo $(UNIT_TEST_NAMES)", "ci-print-units"
        ).split()
        self.assertTrue(names)
        output = self.checked_make("-nB", "test-ci-core")
        executed = re.findall(r"=== unit:([^ ]+) ===", output)
        self.assertCountEqual(executed, names)

    def test_build_covers_core_and_excludes_external_gates(self):
        build = self.checked_make("-nB", "build-ci-core")
        tests = self.checked_make("-nB", "test-ci-core")
        outputs = lambda text: set(re.findall(r"(?:^|\s)-o ([\w./-]+)", text))
        self.assertTrue(outputs(tests))
        self.assertFalse(outputs(tests) - outputs(build))
        for binary in (
            "mycpu", "guest/interactive_os.elf", "guest/ai_accel_demo.elf",
            "tests/host/instruction_semantics_smoke_fp_compare_convert",
            "tests/host/instruction_semantics_smoke_fp_fma_flags",
            "tests/host/pipeline_backend_smoke_fp_arith_fma",
            "tests/host/pipeline_backend_smoke_fp_convert_tail",
        ):
            self.assertIn(binary, outputs(build))
        for forbidden in ("external/xv6", "run_debug_cli_probe", "spike_differential",
                          "MYCPU_LINUX_DISTRO_RUNTIME=1", "run-workload-xv6"):
            self.assertNotIn(forbidden, build + tests)
        self.assertNotRegex(build, r"=== (unit|host):")

    def test_failure_and_timeout_propagate(self):
        with tempfile.TemporaryDirectory(prefix="mycpu-ci-targets-") as directory:
            root = Path(directory)
            shutil.copy2(ROOT / "Makefile", root / "Makefile")
            shutil.copytree(ROOT / "workloads", root / "workloads")
            binary = root / "tests/unit/ci_failure_probe"
            binary.parent.mkdir(parents=True)
            for body, marker in (("exit 7", "Error 7"),
                                 ("sleep 2", "Unit test timed out: ci_failure_probe")):
                binary.write_text("#!/bin/sh\n" + body + "\n")
                binary.chmod(0o755)
                for target in ("test-unit-all", "test-ci-core"):
                    result = make(target, "UNIT_TEST_NAMES=ci_failure_probe",
                                  "CI_HOST_TEST_NAMES=", "TEST_TIMEOUT=0.1s", cwd=root)
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertIn("=== unit:ci_failure_probe ===", result.stdout)
                    self.assertIn(marker, result.stdout)

    def test_guest_and_xv6_flags_compile_csr_and_fence(self):
        with tempfile.TemporaryDirectory(prefix="mycpu-ci-isa-") as directory:
            source = Path(directory) / "probe.S"
            source.write_text(".text\n_start:\n csrr a0, mstatus\n fence.i\n")
            for flags in ("$(RV_FLAGS)",
                          "$(BOARD_XV6_ARCH_MARCH) $(BOARD_XV6_ARCH_MABI)"):
                self.checked_make(
                    f"--eval=ci-isa-probe:;$(RV_CC) {flags} -c {source} -o {source.with_suffix('.o')}",
                    "ci-isa-probe",
                )


if __name__ == "__main__":
    unittest.main()
