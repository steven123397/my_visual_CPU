#!/usr/bin/env python3
import argparse
import pathlib
import subprocess
import sys


RECOMMENDED_MANIFESTS = [
    "guest_ai_accel_demo",
    "custom_dynamic_gemm",
    "custom_dynamic_cnn",
    "custom_dynamic_tiny_model",
]

OPTIONAL_MANIFESTS = [
    "custom_tiny_attention_static",
]

FAIL_CLOSED_TASK_SPEC = "custom_dynamic_gemm_fail_closed.task_spec.json"
FAIL_CLOSED_ERROR = "bounded_dynamic_gemm_v1 task spec has unexpected top-level key: unexpected_extra"


def run_command(argv: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(argv, text=True, capture_output=True, check=False)


def print_block(title: str, text: str) -> None:
    print(f"== {title} ==")
    print(text.rstrip())


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Pack and run the fixed Post-Wave 7 AI demo v1 walkthrough"
    )
    parser.add_argument(
        "--out-dir",
        default="workloads/ai_proto/generated/demo_v1",
        help="output directory for demo_v1 assets",
    )
    parser.add_argument(
        "--mycpu",
        default="./mycpu",
        help="path to mycpu executable used for manifest runs",
    )
    parser.add_argument(
        "--include-attention",
        action="store_true",
        help="also run the optional static_tiny_attention_v1 positive sample",
    )
    args = parser.parse_args(argv)

    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    pack = run_command(
        ["python3", "workloads/ai_proto/pack_graph.py", "--demo-v1", "--out-dir", str(out_dir)]
    )
    if pack.returncode != 0:
        print_block("demo_v1 pack failed", pack.stdout + pack.stderr)
        return pack.returncode
    print_block("demo_v1 pack", pack.stdout)

    manifest_names = list(RECOMMENDED_MANIFESTS)
    if args.include_attention:
        manifest_names.extend(OPTIONAL_MANIFESTS)

    for manifest_name in manifest_names:
        manifest = out_dir / f"{manifest_name}.manifest"
        run = run_command([args.mycpu, "--ai-profile-manifest", str(manifest)])
        if run.returncode != 0:
            print_block(f"{manifest_name} run failed", run.stdout + run.stderr)
            return run.returncode
        print_block(f"{manifest_name} summary", run.stdout)

    fail_closed = run_command(
        [
            "python3",
            "workloads/ai_proto/pack_graph.py",
            "--task-spec",
            str(out_dir / FAIL_CLOSED_TASK_SPEC),
            "--out-dir",
            str(out_dir),
        ]
    )
    if fail_closed.returncode == 0:
        print_block(
            "demo_v1 fail-closed unexpected success",
            fail_closed.stdout + fail_closed.stderr,
        )
        return 1
    if FAIL_CLOSED_ERROR not in (fail_closed.stdout + fail_closed.stderr):
        print_block("demo_v1 fail-closed wrong error", fail_closed.stdout + fail_closed.stderr)
        return 1
    print_block("demo_v1 fail-closed", fail_closed.stdout + fail_closed.stderr)

    print(
        "demo_v1 summary: packed fixed assets, ran "
        f"{len(manifest_names)} positive samples, and verified 1 fail-closed sample"
    )
    if not args.include_attention:
        print("demo_v1 note: custom_tiny_attention_static.manifest was packed but not run")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
