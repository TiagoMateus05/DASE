#!/usr/bin/env bash
set -euo pipefail

QEMU_SHARE=/opt/homebrew/Cellar/qemu/11.1.0/share/qemu
DISKS=build/disks
mkdir -p "$DISKS"

mk() {  # mk <name> <size>
    local f="$DISKS/$1.img"
    [ -f "$f" ] || { echo "creating $f ($2)"; qemu-img create -f raw "$f" "$2" >/dev/null; }
}

# Partitioned test images. Built once; delete the file to rebuild.
mk_gpt() {
    local f="$DISKS/gpt.img"
    [ -f "$f" ] && return
    echo "creating $f (GPT, 3 partitions)"
    qemu-img create -f raw "$f" 1G >/dev/null
    sgdisk -n 1:0:+200M -t 1:ef00 -c 1:"EFI System" \
           -n 2:0:+300M -t 2:0700 -c 2:"Data1" \
           -n 3:0:0     -t 3:0700 -c 3:"Data2" \
           "$f" >/dev/null
}

mk_mbr() {
    local f="$DISKS/mbr.img"
    [ -f "$f" ] && return
    echo "creating $f (MBR, 2 partitions)"
    qemu-img create -f raw "$f" 512M >/dev/null
    # macOS has no sfdisk; use the build container's.
    docker run --rm -v "$(pwd):/work" dase-build bash -c "
        sfdisk /work/$f >/dev/null <<'PART'
label: dos
,200M,0c
,,0c
PART
"
}

mk hdd_big    2G
mk hdd_small  512M
mk ssd        1G
mk nvme       4G
mk usb        256M
mk raid       1G
mk_gpt
mk_mbr

qemu-system-x86_64 \
  -M q35 -m 512 \
  -drive if=pflash,format=raw,readonly=on,file="$QEMU_SHARE/edk2-x86_64-code.fd" \
  -drive if=pflash,format=raw,file=./OVMF_VARS.fd \
  \
  `# --- boot device (SATA) ---` \
  -drive if=none,id=boot,format=raw,file=build/dase.img \
  -device ide-hd,drive=boot,bus=ide.0,bootindex=0,serial=DASE-BOOT-0001 \
  \
  `# --- SATA spinning disks (rotation_rate -> IDENTIFY word 217) ---` \
  -drive if=none,id=hdd0,format=raw,file="$DISKS/hdd_big.img" \
  -device ide-hd,drive=hdd0,bus=ide.1,rotation_rate=7200,serial=WD-HDD-7200-A,model=DASE_TEST_HDD_2G \
  \
  -drive if=none,id=hdd1,format=raw,file="$DISKS/hdd_small.img" \
  -device ide-hd,drive=hdd1,bus=ide.2,rotation_rate=5400,serial=SG-HDD-5400-B,model=DASE_TEST_HDD_512M \
  \
  `# --- SATA SSD (rotation_rate=1 == non-rotating) ---` \
  -drive if=none,id=ssd0,format=raw,file="$DISKS/ssd.img" \
  -device ide-hd,drive=ssd0,bus=ide.3,rotation_rate=1,serial=SSD-SATA-0001,model=DASE_TEST_SSD_1G \
  \
  `# --- partitioned test disks (SATA) ---` \
  -drive if=none,id=gpt0,format=raw,file="$DISKS/gpt.img" \
  -device ide-hd,drive=gpt0,bus=ide.4,serial=GPT-PARTS-0001,model=DASE_TEST_GPT_1G \
  \
  -drive if=none,id=mbr0,format=raw,file="$DISKS/mbr.img" \
  -device ide-hd,drive=mbr0,bus=ide.5,serial=MBR-PARTS-0001,model=DASE_TEST_MBR_512M \
  \
  `# --- NVMe ---` \
  -drive if=none,id=nvme0,format=raw,file="$DISKS/nvme.img" \
  -device nvme,drive=nvme0,serial=NVME-0001 \
  \
  `# --- USB mass storage ---` \
  -device qemu-xhci,id=xhci \
  -drive if=none,id=usb0,format=raw,file="$DISKS/usb.img" \
  -device usb-storage,bus=xhci.0,drive=usb0,serial=USB-0001 \
  \
  `# --- RAID controller: visible on PCI bus, NO BlockIo (no OVMF driver) ---` \
  -device megasas,id=raidhba \
  -drive if=none,id=raid0,format=raw,file="$DISKS/raid.img" \
  -device scsi-hd,bus=raidhba.0,drive=raid0,serial=RAID-VD-0001 \
  \
  -device VGA,xres=1024,yres=768,vgamem_mb=64 \
  -display cocoa,zoom-to-fit=on \
  -net none