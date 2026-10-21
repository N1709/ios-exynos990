#!/bin/bash
# Build the Exynos 990 XNU loader (Samsung S-Boot "kernel" Image).
# Optionally pack an Android boot image with the XNU payload as its ramdisk:
#   build.sh [PAYLOAD.bin [OUT.boot.img]]
# PAYLOAD comes from tools/mkpayload.py (a "SWEETXNU" container). It is stored
# uncompressed as the ramdisk of the boot image, so the loader finds it by
# scanning the ramdisk region S-Boot loaded it into.
set -euo pipefail
cd "$(dirname "$0")"
CC=aarch64-linux-gnu-gcc
OBJCOPY=aarch64-linux-gnu-objcopy
CFLAGS="-O2 -ffreestanding -nostdlib -fno-pic -mgeneral-regs-only -mstrict-align -fno-stack-protector -fno-tree-loop-distribute-patterns -march=armv8-a -Wall -Wextra -Werror"
if [ $# -gt 2 ]; then
	echo "usage: build.sh [PAYLOAD.bin [OUT.boot.img]]" >&2
	exit 1
fi
mkdir -p out
$CC $CFLAGS -c start.S -o out/start.o
$CC $CFLAGS -c loader.c -o out/loader.o
$CC $CFLAGS -c console.c -o out/console.o
$CC $CFLAGS -c ufs_probe.c -o out/ufs_probe.o
$CC $CFLAGS -c ../fdt.c -o out/fdt.o
objs="out/loader.o out/console.o out/ufs_probe.o out/fdt.o"
# S-Boot runs the image wherever it puts it, with no relocation pass: an
# absolute relocation would need a fixup that nobody performs. start.S is
# left out, it may hold the absolute image size of the arm64 Image header.
abs=$(aarch64-linux-gnu-readelf -r $objs | grep -c ABS64 || true)
[ "$abs" = 0 ] || { echo "error: $abs absolute relocations" >&2; exit 1; }
$CC $CFLAGS -static -T loader.ld -Wl,--build-id=none -Wl,--no-warn-rwx-segments -Wl,-N \
	out/start.o $objs -o out/loader.elf
$OBJCOPY -O binary out/loader.elf out/Image
# S-Boot copies the file; pad to _end so .bss and the stack exist as zeroed bytes.
size=$(aarch64-linux-gnu-nm out/loader.elf | awk '$3=="_end"{print strtonum("0x"$1)}')
truncate -s "$size" out/Image
echo "out/Image: $(stat -c %s out/Image) bytes"
if [ $# -ge 1 ]; then
	[ -f "$1" ] || { echo "error: no such payload: $1" >&2; exit 1; }
	boot=${2:-out/boot.img}
	python3 ../../tools/mkboot_exynos990.py --kernel out/Image --ramdisk "$1" --out "$boot"
	sha256sum "$boot"
fi
