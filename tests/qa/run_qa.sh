#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="$(pwd)/build"
IMG="${BUILD_DIR}/myos.img"
LOG="${BUILD_DIR}/qa.log"
SCRATCH="/tmp/qa_scratch.img"

# Build
make -j >/dev/null

# Create a fresh scratch image with known signature
dd if=/dev/zero of="$SCRATCH" bs=1M count=32 2>/dev/null
python3 -c "
import os
d=bytearray(os.urandom(32*1024*1024))
d[0:8]=b'MYOSVIRT'
for i in range(8,512): d[i]=(i*7+3)&0xff
open('$SCRATCH','wb').write(bytes(d))
"

# Boot with scratch disk on virtio-blk + virtio-net
qemu-system-x86_64 \
    -drive file="$IMG",format=raw,if=ide \
    -drive file="$SCRATCH",format=raw,if=none,id=vd0 \
    -device virtio-blk-pci,drive=vd0 \
    -netdev user,id=n0 \
    -device virtio-net-pci,netdev=n0 \
    -m 1G -smp 1 \
    -serial file:"$LOG" \
    -vga std -display none -no-reboot \
    -cpu max &
QEMU_PID=$!

# Wait for boot completion or timeout
for i in {1..60}; do
    if grep -q '\[PHASE8\] Init complete' "$LOG" 2>/dev/null; then
        break
    fi
    sleep 1
done

# Give userspace time to run and exit
sleep 10

# Clean shutdown
kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true

echo "=== QA Checks ==="
PASS=0
FAIL=0

check() {
    local pattern="$1"
    local desc="$2"
    if grep -q "$pattern" "$LOG"; then
        echo "PASS: $desc"
        PASS=$((PASS+1))
    else
        echo "FAIL: $desc (missing: $pattern)"
        FAIL=$((FAIL+1))
    fi
}

check_absent() {
    local pattern="$1"
    local desc="$2"
    if ! grep -q "$pattern" "$LOG"; then
        echo "PASS: $desc"
        PASS=$((PASS+1))
    else
        echo "FAIL: $desc (found unexpected: $pattern)"
        FAIL=$((FAIL+1))
    fi
}

# Core boot sequence
check '\[PMM\] init:' 'PMM initialized'
check '\[PAGING\] Initialized 4-level tables' '4-level paging active'
check '\[SYSCALL\] System call interface initialized' 'Syscall interface ready'
check '\[CPU\] NX=1' 'NX bit enabled'
check '\[VIRTIO\]' 'Virtio subsystem probed'
check '\[VIRTIO-BLK\] ready' 'Virtio block device ready'
check '\[VIRTIO-BLK\] selftest sector0 read ok' 'Block device read verified'
check '\[VIRTIO-NET\] init' 'Virtio net enumerated'
check '\[PHASE8\] Spawning user-space' 'Userspace spawner active'
check '\[PHASE8\] Init complete' 'Phase 8 init complete'

# Userspace programs (via kernel's clean log messages)
check 'Created user process' 'userspace processes spawned'
check 'exited with code 0' 'processes exit cleanly'
check 'exited with code 7' 'forkdemo child exits with expected code'
check 'exited with code 0' 'init processes exit cleanly'

# No failures (inverted)
check_absent 'panic' 'no kernel panic'
check_absent 'Exception' 'no unhandled exception'
check_absent 'page fault' 'no page fault'

echo "=== Summary: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] || exit 1