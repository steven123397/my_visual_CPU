#!/bin/sh

set -eu

target="$1"

output="$(
  printf '%s\n' \
    '{"cmd":"load","image":"tests/asm/hello.elf","backend":"pipeline"}' \
    '{"cmd":"snapshot"}' \
    '{"cmd":"step_cycle"}' \
    '{"cmd":"snapshot"}' \
    '{"cmd":"quit"}' \
  | "./$target" --debug-cli
)"

case "$output" in
  *'"type":"snapshot"'*'"cycle":0'*'"pipeline"'*'"devices"'*'"bus"'*)
    ;;
  *)
    echo "missing initial snapshot fields" >&2
    printf '%s\n' "$output" >&2
    exit 1
    ;;
esac

case "$output" in
  *'"type":"snapshot"'*'"cycle":1'*)
    ;;
  *)
    echo "missing stepped snapshot" >&2
    printf '%s\n' "$output" >&2
    exit 1
    ;;
esac
