qemu-system-x86_64 \
  -drive if=pflash,format=raw,readonly=on,file=/opt/homebrew/Cellar/qemu/11.1.0/share/qemu/edk2-x86_64-code.fd \
  -drive if=pflash,format=raw,file=./OVMF_VARS.fd \
  -drive format=raw,file=build/dase.img \
  -device VGA,xres=1024,yres=768,vgamem_mb=64 \
  -display cocoa,zoom-to-fit=on \
  -full-screen \
  -net none