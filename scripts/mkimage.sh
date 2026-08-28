#!/usr/bin/env bash
set -euo pipefail

IMG=build/dase.img
EFI=build/BOOTX64.EFI

[ -f "$EFI" ] || { echo "ERROR: $EFI not found — run 'make' first"; exit 1; }

echo "Creating 64 MB FAT32 image..."
dd if=/dev/zero of="$IMG" bs=1M count=64 status=none
mkfs.vfat -F 32 "$IMG" >/dev/null

echo "Installing EFI/BOOT/BOOTX64.EFI..."
mmd  -i "$IMG" ::/EFI
mmd  -i "$IMG" ::/EFI/BOOT
mcopy -i "$IMG" "$EFI" ::/EFI/BOOT/BOOTX64.EFI

echo "Done: $IMG"
