#!/usr/bin/env bash
# Build the flashable boot image for the Exynos 990 (Galaxy S20 family).
#
#   exynos990-make-boot.sh KERNEL_MACHO [MODEL] [OUTDIR]
#
# KERNEL_MACHO is the kernel the macOS CI built (the artifact "xnu-kernel",
# kernel.development.*), MODEL is one of x1s y2s z3s r8s c1s c2s
# (default z3s = Galaxy S20 Ultra) and OUTDIR defaults to out/exynos990.
#
# The script does the whole chain:
#   1. ADT      - tools/mkadt_exynos990.py (per model, board facts of the
#                 Exynos 9830)
#   2. payload - tools/mkpayload.py (the "SWEETXNU" container the loader
#                 reads: kernel segments, ADT, command line)
#   3. loader  - shim/exynos990/build.sh (the arm64 Image S-Boot loads as
#                 the "kernel" of the boot image)
#   4. boot img - tools/mkboot_exynos990.py (Android boot image v2, the
#                 kernel slot holds the loader, the ramdisk slot the
#                 payload)
#
# The result is OUTDIR/boot-<model>.img, ready to be flashed to the "boot"
# partition.  Nothing here touches the phone.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$(pwd)

if [ $# -lt 1 ]; then
	echo "usage: $0 KERNEL_MACHO [MODEL] [OUTDIR]" >&2
	exit 2
fi
kernel=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
model=${2:-z3s}
outdir=${3:-out/exynos990}
case "$model" in
x1s|y2s|z3s|r8s|c1s|c2s) ;;
*)
	echo "error: unknown model '$model' (x1s y2s z3s r8s c1s c2s)" >&2
	exit 2
	;;
esac
[ -f "$kernel" ] || { echo "error: no such kernel: $kernel" >&2; exit 1; }

mkdir -p "$outdir"
outdir=$(cd "$outdir" && pwd)

echo "== 1. ADT ($model)"
python3 tools/mkadt_exynos990.py --model "$model" "$outdir/$model.adt"

echo "== 2. payload"
# The command line: serial=3 is the framebuffer console, debug=0xc keeps the
# kernel symbols.  The klog sink is added once its RAM address is known
# (sweetklog=0x...).
python3 tools/mkpayload.py "$kernel" "$outdir/$model.adt" "$outdir/$model.payload" \
	--cmdline "debug=0xc serial=3 cpus=1 keepsyms=1"

echo "== 3. loader"
bash shim/exynos990/build.sh
cp shim/exynos990/out/Image "$outdir/loader-$model.img"

echo "== 4. boot image"
python3 tools/mkboot_exynos990.py --kernel "$outdir/loader-$model.img" \
	--ramdisk "$outdir/$model.payload" --out "$outdir/boot-$model.img"

echo
echo "boot image : $outdir/boot-$model.img"
sha256sum "$outdir/boot-$model.img"
echo
echo "flash it to the 'boot' partition (backup first):"
echo "  adb push $outdir/boot-$model.img /tmp/boot.img"
echo "  adb shell dd if=/tmp/boot.img of=/dev/block/by-name/boot bs=4M conv=fsync"
echo
echo "collect the board facts while the phone is in TWRP:"
echo "  adb push tools/exynos990-collect.sh /tmp/ && adb shell sh /tmp/exynos990-collect.sh"
echo "watch the loader on the debug UART (0x10540000, 115200 8N1):"
echo "  picocom -b 115200 /dev/ttyUSB0"
