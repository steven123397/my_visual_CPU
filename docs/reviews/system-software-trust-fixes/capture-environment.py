"""Capture tool versions and generated asset hashes without retaining binaries."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

out = Path(__file__).resolve().parent
record = {"captured_unix": time.time(), "tools": {}, "assets": {}}
for tool, option in [("g++", "--version"), ("riscv64-unknown-elf-gcc", "--version"),
                     ("riscv64-unknown-elf-ld", "--version"), ("riscv64-unknown-elf-nm", "--version"),
                     ("gdb", "--version"), ("python3", "--version"),
                     ("/home/liangjiaqi/.local/riscv-spike/bin/spike", "--help")]:
    path = shutil.which(tool)
    if path is None:
        record["tools"][tool] = {"available": False}
        continue
    result = subprocess.run([path, option], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10)
    record["tools"][tool] = {
        "path": path, "argv": [path, option], "exit_code": result.returncode,
        "version_output": result.stdout, "sha256": hashlib.sha256(Path(path).read_bytes()).hexdigest(),
    }
paths = ["myCPU/mycpu", "myCPU/tests/host/spike_differential_smoke",
         "myCPU/tests/host/xv6_boot_smoke", "myCPU/tests/host/xv6_shell_smoke",
         "myCPU/tests/host/system_software_fork_evidence", "myCPU/external/xv6-riscv/kernel/kernel",
         "myCPU/external/xv6-riscv/fs.img", "myCPU/external/xv6-riscv/user/_init",
         "myCPU/external/xv6-riscv/user/_sh", "myCPU/external/xv6-riscv/user/_forktest",
         "/tmp/mycpu-trust-fork/_forkev", "/tmp/mycpu-trust-fork/fs.img", "/tmp/mycpu-trust-fork/layout.txt"]
paths.extend(str(path) for path in Path("myCPU/tests/asm").glob("sv39_tlb_*.elf"))
for name in paths:
    path = Path(name)
    record["assets"][name] = {"exists": path.is_file()}
    if path.is_file():
        record["assets"][name]["sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
(out / f"{sys.argv[1]}.json").write_text(json.dumps(record, indent=2) + "\n")
