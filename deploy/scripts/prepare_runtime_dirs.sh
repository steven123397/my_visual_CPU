#!/usr/bin/env sh
set -eu

ROOT=/srv/apps/my_visual_CPU

mkdir -p \
  "$ROOT/runtime-assets/linux" \
  "$ROOT/runtime-assets/spike/bin" \
  "$ROOT/logs" \
  "$ROOT/tmp"

printf 'prepared remote runtime directories under %s\n' "$ROOT"
printf 'expected Linux Image: %s/runtime-assets/linux/Image\n' "$ROOT"
printf 'expected Linux rootfs: %s/runtime-assets/linux/rootfs.ext4\n' "$ROOT"
printf 'expected Linux DTB: %s/runtime-assets/linux/linux.dtb\n' "$ROOT"
printf 'expected Spike binary: %s/runtime-assets/spike/bin/spike\n' "$ROOT"
