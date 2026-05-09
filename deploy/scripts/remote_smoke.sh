#!/usr/bin/env bash
set -u
set -o pipefail

ROOT=/srv/apps/my_visual_CPU
REPO="$ROOT/repo"
LINUX_DIR="$ROOT/runtime-assets/linux"
SPIKE_BIN="$ROOT/runtime-assets/spike/bin/spike"
TMP_DIR="$ROOT/tmp"
LOG_DIR="$ROOT/logs"

export MYCPU_RUNTIME_TMPDIR="$TMP_DIR"

mkdir -p "$LOG_DIR" "$TMP_DIR"

failures=0
blockers=()
skips=()

run_required() {
  local label="$1"
  shift
  printf '\n== required: %s ==\n' "$label"
  if "$@"; then
    printf 'PASS: %s\n' "$label"
  else
    local status=$?
    printf 'FAIL: %s (exit %s)\n' "$label" "$status" >&2
    failures=$((failures + 1))
  fi
}

run_optional() {
  local label="$1"
  shift
  printf '\n== optional: %s ==\n' "$label"
  if "$@"; then
    printf 'PASS: %s\n' "$label"
  else
    local status=$?
    printf 'FAIL: %s (exit %s)\n' "$label" "$status" >&2
    failures=$((failures + 1))
  fi
}

require_file() {
  local label="$1"
  local path="$2"
  if [ ! -f "$path" ]; then
    blockers+=("$label missing: $path")
    return 1
  fi
  if [ ! -r "$path" ]; then
    blockers+=("$label not readable: $path")
    return 1
  fi
  return 0
}

require_exec() {
  local label="$1"
  local path="$2"
  if [ ! -x "$path" ]; then
    blockers+=("$label missing or not executable: $path")
    return 1
  fi
  return 0
}

if [ ! -d "$REPO/.git" ]; then
  printf 'FAIL: repository is not present at %s\n' "$REPO" >&2
  exit 1
fi

cd "$REPO"

printf 'checkout: %s\n' "$(git rev-parse --short HEAD)"
printf 'branch: %s\n' "$(git branch --show-current)"

run_required "git diff --check" git diff --check
run_required "myCPU frontend e2e prerequisites" \
  bash -lc 'cd /srv/apps/my_visual_CPU/repo/myCPU && make mycpu tests/asm/hello.elf guest/interactive_os.elf'
run_required "frontend node --test" bash -lc 'cd /srv/apps/my_visual_CPU/repo/frontend && unset MYCPU_LINUX_PROTO_CONSOLE_IMAGE MYCPU_LINUX_PROTO_RUNTIME_IMAGE MYCPU_RUN_LINUX_PROTO_CONSOLE_E2E && node --test'
run_required "myCPU debug_cli_smoke" bash -lc 'cd /srv/apps/my_visual_CPU/repo/myCPU && make test-host-debug_cli_smoke'
run_required "myCPU interactive_terminal_smoke" bash -lc 'cd /srv/apps/my_visual_CPU/repo/myCPU && make test-host-interactive_terminal_smoke'
run_required "myCPU ai_accelerator_profile_smoke" bash -lc 'cd /srv/apps/my_visual_CPU/repo/myCPU && make test-host-ai_accelerator_profile_smoke'
run_required "myCPU spike_differential_smoke" bash -lc 'cd /srv/apps/my_visual_CPU/repo/myCPU && make test-host-spike_differential_smoke'

if require_exec "Spike" "$SPIKE_BIN"; then
  run_optional "myCPU real spike_differential" \
    bash -lc 'cd /srv/apps/my_visual_CPU/repo/myCPU && SPIKE_PATH=/srv/apps/my_visual_CPU/runtime-assets/spike/bin/spike make test-host-spike_differential'
else
  skips+=("real Spike differential skipped because $SPIKE_BIN is not executable")
fi

linux_ready=1
require_file "Linux Image" "$LINUX_DIR/Image" || linux_ready=0
require_file "Linux rootfs" "$LINUX_DIR/rootfs.ext4" || linux_ready=0
require_file "Linux DTB" "$LINUX_DIR/linux.dtb" || linux_ready=0

if [ "$linux_ready" -eq 1 ]; then
  run_optional "frontend Linux console readiness diagnostic" bash -lc '
    cd /srv/apps/my_visual_CPU/repo
    export MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/srv/apps/my_visual_CPU/runtime-assets/linux/Image
    export MYCPU_LINUX_PROTO_RUNTIME_IMAGE=/srv/apps/my_visual_CPU/runtime-assets/linux/Image
    node --input-type=module -e "
      import { linuxConsoleDiagnostic } from \"./frontend/server/tests_manifest.mjs\";
      const diagnostic = linuxConsoleDiagnostic();
      console.log(JSON.stringify(diagnostic));
      if (!diagnostic.ready) process.exit(1);
    "
  '
  run_optional "frontend Linux console opt-in e2e" \
    bash -lc 'cd /srv/apps/my_visual_CPU/repo/frontend && MYCPU_RUN_LINUX_PROTO_CONSOLE_E2E=1 MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/srv/apps/my_visual_CPU/runtime-assets/linux/Image node --test tests/debug_server_e2e.test.mjs'
else
  skips+=("Linux console ready/e2e skipped because Image/rootfs/DTB assets are incomplete")
fi

printf '\n== asset blockers ==\n'
if [ "${#blockers[@]}" -eq 0 ]; then
  printf 'none\n'
else
  printf '%s\n' "${blockers[@]}"
fi

printf '\n== skipped optional checks ==\n'
if [ "${#skips[@]}" -eq 0 ]; then
  printf 'none\n'
else
  printf '%s\n' "${skips[@]}"
fi

if [ "$failures" -ne 0 ]; then
  printf '\nremote smoke finished with %s failure(s)\n' "$failures" >&2
  exit 1
fi

printf '\nremote smoke finished successfully\n'
