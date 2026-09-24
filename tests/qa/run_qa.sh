#!/usr/bin/env bash
set -euo pipefail
BUILD_DIR="$(pwd)/build"
IMG="${BUILD_DIR}/myos.img"
LOG="${BUILD_DIR}/qa.log"

make -j
qemu-system-x86_64 -drive file="$IMG",format=raw,if=ide -m 256M -serial file:"$LOG" -display none -no-reboot &
QEMU_PID=$!
sleep 30
# Simple scripted input placeholder
# In real test, sendkey/mouse via monitor
sleep 10
kill $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true

echo "=== QA Checks ==="
grep -c "\[COMP\]" "$LOG" && echo "FAIL: COMP logs present" || echo "PASS: no COMP logs"
grep -c "\[INPUT\] ring overflow" "$LOG" && echo "FAIL: input overflow" || echo "PASS: no input overflow"
grep -c "\[RTFIX\] legacy path entered" "$LOG" && echo "FAIL: legacy path" || echo "PASS: no legacy path"
if grep -q "fps=" "$LOG"; then echo "PASS: F11 stats present"; else echo "FAIL: no stats"; fi
