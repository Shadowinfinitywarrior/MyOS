#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="$(pwd)/build"
IMG="${BUILD_DIR}/myos.img"
LOG="${BUILD_DIR}/ktest.log"

if [ ! -f "$IMG" ]; then
    echo "Kernel image $IMG does not exist. Please run 'make' first."
    exit 1
fi

echo "=== Running MyOS Kernel Automated Smoke & Unit Tests ==="

rm -f "$LOG"

# Boot QEMU headlessly
qemu-system-x86_64 \
    -drive file="$IMG",format=raw,if=ide \
    -m 1G -smp 1 \
    -serial file:"$LOG" \
    -vga std -display none -no-reboot \
    -cpu max &
QEMU_PID=$!

# Wait for boot milestones or timeout
BOOTED=0
for i in {1..30}; do
    if grep -q "\[PHASE8\] Init complete" "$LOG" 2>/dev/null; then
        BOOTED=1
        break
    fi
    sleep 1
done

kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true

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

check '\[SECURITY\]' 'CPU Security Configuration (SMEP/SMAP/UMIP/NX)'
check '\[ASLR\]' 'Address Space Layout Randomization (ASLR)'
check '\[SLAB\] Initialized kmem caches' 'Slab Allocator (orders 5..12)'
check '\[SYSCALL\] System call interface initialized' 'Syscall ABI v1.0.0 Interface'
check '\[PAGING\]' '4-level Paging Subsystem'
check '\[NOVAFS\] Initializing Next-Gen Extent Filesystem Subsystem' 'NovaFS Extent Filesystem Initialization'
check '\[NOVAFS\] Mounted volume.*on /nova' 'NovaFS Volume Mount at /nova'
check '\[PKG\] Initializing Debian apt/dpkg package subsystem' 'Debian Package Management Subsystem (APT/DPKG)'
check '\[PKG\] Package database initialized with 8 packages' 'Package Database & /var/lib/dpkg/status Synchronization'

echo "=== Kernel Test Summary: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] || exit 1
