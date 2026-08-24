#!/usr/bin/env bash
set -euo pipefail

image=${1:-build/mvh-betriebsystem.iso}
serial_log=$(mktemp)
error_log=$(mktemp)
trap 'rm -f "$serial_log" "$error_log"' EXIT
status=0
timeout 15s qemu-system-x86_64 -accel tcg -machine q35 -cpu qemu64 \
  -m 512 -smp 2 -display none -monitor none -no-reboot -no-shutdown \
  -serial "file:$serial_log" -cdrom "$image" -boot d 2>"$error_log" || status=$?
if [[ $status -ne 0 && $status -ne 124 ]]; then cat "$error_log" >&2; exit "$status"; fi
grep -F "linear framebuffer mapped for graphical console" "$serial_log"
grep -F "device manager initialized" "$serial_log"
printf 'QEMU graphical desktop boot passed\n'
