set -xue

CC=/opt/homebrew/opt/llvm/bin/clang

CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fuse-ld=lld -fno-stack-protector -ffreestanding -nostdlib"

OBJCOPY=/opt/homebrew/opt/llvm/bin/llvm-objcopy

$CC $CFLAGS -Wl,-Tuser.ld -Wl,-Map=shell.map -o user/shell.elf user/shell.c user/user.c common/common.c
$OBJCOPY --set-section-flags .bss=alloc,contents -O binary user/shell.elf user/shell.bin
$OBJCOPY -Ibinary -Oelf32-littleriscv user/shell.bin user/shell.bin.o

$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel/kernel.elf \
    kernel/kernel.c common/common.c kernel/drivers/virtio.c kernel/filesystem/fat16.c user/shell.bin.o

qemu-system-riscv32 -machine virt -bios default -nographic -serial mon:stdio --no-reboot \
    -drive id=drive0,file=fat16.img,format=raw,if=none \
    -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
    -kernel kernel/kernel.elf