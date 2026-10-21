#!/bin/bash
# Kernel Mach-O -> Galaxy S20 family (Exynos 990) boot image v2.
#
#   make-boot.sh KERNEL OUT.img [boot-args...]
#
# The XNU kernel (Mach-O) plus the Exynos 990 ADT are packed into the
# "SWEETXNU" payload container (tools/mkpayload.py); the loader in
# shim/exynos990/ is the boot image's "kernel" and the payload container is
# its "ramdisk" (tools/mkboot_exynos990.py).
#
# Default boot-args: one CPU, verbose console, no watchdog, and the A5 touch
# probe off - it maps Exynos7580 I2C/GPIO and aborts on this board (the same
# reason the S8 port passes a5touchprobe=0).
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
kernel=$(realpath "$1")
out=$(realpath -m "$2")
shift 2
args=${*:-debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress wdt=-1 wdt_test=no_user_checkin a5touchprobe=0}
model=${EXYNOS990_MODEL:-z3s}
work=$repo/shim/exynos990/out
mkdir -p "$work"
python3 "$repo/tools/mkadt_exynos990.py" "$work/exynos990.adt" --model "$model"
python3 "$repo/tools/mkpayload.py" "$kernel" "$work/exynos990.adt" "$work/payload.bin" --cmdline "$args"
bash "$repo/shim/exynos990/build.sh" "$work/payload.bin" "$out"
