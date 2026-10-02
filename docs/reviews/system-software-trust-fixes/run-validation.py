"""Record bounded local validation; run from the repository root."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


parser = argparse.ArgumentParser()
parser.add_argument("label")
parser.add_argument("--timeout", type=int, required=True)
parser.add_argument("--cwd", default=".")
parser.add_argument("--asset", action="append", default=[])
parser.add_argument("command", nargs=argparse.REMAINDER)
args = parser.parse_args()
command = args.command
if command and command[0] == "--":
    command = command[1:]
if not command:
    parser.error("command required")
root = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip())
out = Path(__file__).resolve().parent
checkout = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
paths = subprocess.check_output(
    ["git", "ls-files", "-z", "myCPU/src", "myCPU/tests", "myCPU/external/xv6-riscv", "myCPU/Makefile"], cwd=root
).decode().split("\0")
sources = {path: sha256(root / path) for path in paths if path and (root / path).is_file()}
source_text = json.dumps(sources, indent=2, sort_keys=True) + "\n"
source_id = hashlib.sha256(source_text.encode()).hexdigest()[:12]
manifest = out / f"sources-{source_id}.json"
manifest.write_text(source_text)
argv = ["timeout", f"{args.timeout}s", *command]
input_files = {item: sha256(Path(args.cwd) / item) for item in [*command, *args.asset] if (Path(args.cwd) / item).is_file()}
spike_temps_before = set(Path("/tmp").glob("spike_diff_*"))
started = time.time()
with (out / f"{args.label}.stdout").open("wb") as stdout, (out / f"{args.label}.stderr").open("wb") as stderr:
    result = subprocess.run(argv, cwd=args.cwd, stdout=stdout, stderr=stderr)
record = {
    "checkout_sha": checkout,
    "product_diff": subprocess.check_output(["git", "diff", "HEAD", "--", "myCPU"], cwd=root, text=True),
    "argv": argv, "cwd": str(Path(args.cwd).resolve()),
    "environment": {key: os.environ[key] for key in ("SPIKE_PATH", "XV6_EVIDENCE_MARCH") if key in os.environ},
    "started_unix": started, "wall_seconds": round(time.time() - started, 3),
    "exit_code": result.returncode, "wall_budget_seconds": args.timeout,
    "source_manifest": manifest.name, "source_manifest_sha256": sha256(manifest),
    "logs": {suffix: {"path": f"{args.label}.{suffix}", "sha256": sha256(out / f"{args.label}.{suffix}")}
             for suffix in ("stdout", "stderr")},
    "input_files_before_sha256": input_files,
    "input_files_after_sha256": {item: sha256(Path(args.cwd) / item) for item in input_files},
}
if os.environ.get("SPIKE_DIFF_KEEP_TEMPS"):
    spike_temps = sorted(set(Path("/tmp").glob("spike_diff_*")) - spike_temps_before)
    record["spike_generated_files"] = {
        str(path): {"sha256": sha256(path), **({"script": path.read_text()} if path.name.startswith("spike_diff_cmd_") else {})}
        for path in spike_temps
    }
(out / f"{args.label}.json").write_text(json.dumps(record, indent=2) + "\n")
print(f"{args.label}: exit={result.returncode}, wall={record['wall_seconds']}s", flush=True)
raise SystemExit(result.returncode)
