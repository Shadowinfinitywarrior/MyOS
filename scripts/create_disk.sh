#!/bin/bash
# Create disk image for MyOS
set -e
IMG=build/myos.img
dd if=/dev/zero of=$IMG bs=512 count=40960
echo "Disk image created: $IMG"
