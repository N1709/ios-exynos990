#!/usr/bin/env bash
# Flash out/exynos990/boot-<model>.img to the phone's "boot" partition.
#
#   exynos990-flash-boot.sh [MODEL] [-r]
#
# MODEL defaults to z3s (Galaxy S20 Ultra, SM-G988B) and must match the
# device that is actually connected: these images carry an Exynos 990
# loader, and flashing one to another Samsung model leaves that phone with
# a boot partition it cannot boot.  The script therefore refuses to run
# unless ro.product.device is exactly MODEL.
#
# Nothing but the "boot" partition is written.  -r reboots afterwards.
set -euo pipefail
cd "$(dirname "$0")/.."

model=z3s
reboot=0
for arg in "$@"; do
	case $arg in
	-r) reboot=1 ;;
	-*) echo "usage: $0 [MODEL] [-r]" >&2; exit 2 ;;
	*) model=$arg ;;
	esac
done

img=out/exynos990/boot-$model.img
[ -f "$img" ] || { echo "missing $img (run tools/exynos990-make-boot.sh)" >&2; exit 1; }

prop() { adb shell getprop "$1" 2>/dev/null | tr -d '\r'; }

dev=$(prop ro.product.device)
prod=$(prop ro.product.model)
hw=$(prop ro.hardware)
board=$(prop ro.product.board)

if [ -z "$dev" ]; then
	echo "no device on adb" >&2
	exit 1
fi

echo "connected : device=$dev model=$prod board=$board hw=$hw"
echo "image     : $img ($(stat -c %s "$img") bytes)"

case $prod in
SM-S9*|SM-S7*)
	echo "REFUSING: $prod is a 2024/2025 Galaxy, not an Exynos 990 device." >&2
	exit 1
	;;
esac

if [ "$dev" != "$model" ]; then
	echo "REFUSING: expected ro.product.device=$model, found '$dev' ($prod)." >&2
	echo "These boot images only run on Exynos 990 (x1s y2s z3s r8s c1s c2s)." >&2
	exit 1
fi

echo "writing the boot partition only"
adb push "$img" /tmp/boot.img
adb shell 'dd if=/tmp/boot.img of=/dev/block/by-name/boot bs=4M conv=fsync'
adb shell sync

if [ "$reboot" = 1 ]; then
	echo "rebooting; expect no display output, wait ~90 s, then force off"
	adb reboot
fi
