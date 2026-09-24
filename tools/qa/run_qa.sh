#!/usr/bin/env bash
set -euo pipefail

# MyOS QA harness per plan.md Agent 11
# Builds image, boots QEMU headless, drives minimal input, logs serial, reports FAIL/WARN/stub

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[QA] Building MyOS..."
make -j"$(nproc)" >/dev/null

# Prefer ISO if built, fall back to raw image
if [[ -f "$ROOT/build/myos.iso" ]]; then
  IMG="$ROOT/build/myos.iso"
  QEMU_DRIVE="-cdrom \"$IMG\""
elif [[ -f "$ROOT/build/myos.img" ]]; then
  IMG="$ROOT/build/myos.img"
  QEMU_DRIVE="-drive file=\"$IMG\",format=raw,if=ide"
else
  echo "ERROR: build/myos.iso or build/myos.img not found after build" >&2
  exit 1
fi

LOG="$ROOT/tools/qa/qa.log"
rm -f "$LOG"

echo "[QA] Starting QEMU headless, serial -> $LOG"
# Start QEMU with monitor on telnet for sendkey/mouse injection
eval qemu-system-x86_64 \
  $QEMU_DRIVE \
  -m 1G -smp 2 \
  -serial file:"$LOG" \
  -display none \
  -monitor telnet:127.0.0.1:4444,server,nowait \
  -no-reboot \
  >/dev/null 2>&1 &
QEMU_PID=$!

# Wait for boot
sleep 12

# Simple input drive via QEMU monitor
# Note: sendkey syntax is monitor-specific; we attempt a few events
send_monitor() {
  printf "%s\n" "$1" | nc -w1 127.0.0.1 4444 || true
}

START_TIME=$(date +%s)
echo "[QA] Driving input..."
send_monitor "sendkey q"
sleep 1
send_monitor "sendkey down"
sleep 1
send_monitor "sendkey enter"
sleep 2
# Mouse move example (relative) - QEMU monitor mouse injection
# Using sendkey for modifier; real mouse events would use QEMU monitor sendmouse if available
send_monitor "sendkey ctrl-1"
sleep 1

# Run for total 120s
echo "[QA] Running for 120s..."
ELAPSED=$(( $(date +%s) - START_TIME ))
REMAIN=$(( 120 - ELAPSED ))
if [[ $REMAIN -gt 0 ]]; then
  sleep $REMAIN
fi

echo "[QA] Stopping QEMU..."
# Try graceful quit via monitor
send_monitor "quit" || true
sleep 1
kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true

echo "[QA] Grepping log for issues..."
if [[ -f "$LOG" ]]; then
  MATCHES=$(grep -iE "FAIL|WARN|stub" "$LOG" || true)
  if [[ -n "$MATCHES" ]]; then
    echo "=== ISSUES FOUND ==="
    echo "$MATCHES"
    echo "=== END ==="
    exit 1
  else
    echo "No FAIL/WARN/stub markers found."
  fi
  echo "Log size: $(stat -c%s "$LOG") bytes"
else
  echo "ERROR: log not created" >&2
  exit 1
fi

echo "[QA] Done."
