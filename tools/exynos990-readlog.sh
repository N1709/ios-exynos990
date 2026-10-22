#!/bin/sh
# exynos990-readlog.sh - capture the boot log from the phone.
#
# Two sinks are checked, because they have very different lifetimes:
#
#   /sys/fs/pstore/pmsg-ramoops-0  the 16 KiB pmsg ring at 0xeb30c000.  The
#       loader and XNU both write here, but TWRP adds a line per second, so
#       the text survives only ~3 minutes after TWRP starts.
#
#   /proc/last_kmsg  Samsung's 2 MiB log_kernel buffer (0xFD900000+0x10000,
#       exynos9830-rmem.dtsi:79-81).  The loader mirrors its log 1 MiB into
#       it, where neither S-Boot nor TWRP writes, so it can be read at
#       leisure; it does not contain XNU's own output though.
#
# Usage:  tools/exynos990-readlog.sh [prefix]
#   Run it before pressing Vol Up + Power: it blocks on adb and dumps the
#   moment TWRP's adbd appears.
set -e

prefix=${1:-out/exynos990/lastlog}
mkdir -p "$(dirname "$prefix")"

echo "waiting for adbd (boot TWRP now)..."
adb wait-for-device

grab() {
	# $1 = device path, $2 = output file
	adb exec-out "cat $1" > "$2.raw" 2>/dev/null || rm -f "$2.raw"
	if [ -s "$2.raw" ]; then
		tr -d '\000' < "$2.raw" > "$2" 2>/dev/null || cp "$2.raw" "$2"
		rm -f "$2.raw"
		echo "saved $2 ($(wc -c < "$2" | tr -d ' ') bytes)"
	else
		rm -f "$2.raw"
		echo "no $1"
	fi
}

grab /sys/fs/pstore/pmsg-ramoops-0 "$prefix.pmsg.txt"
grab /proc/last_kmsg "$prefix.lastkmsg.txt"

show() {
	[ -f "$1" ] || return 0
	# "EXYNOS990" alone also matches the kernel's "Machine model: ... based
	# on EXYNOS990" line, so only the loader's own banner counts.
	if grep -aq 'EXYNOS990 LOADER BEGIN' "$1"; then
		echo "--- $1 ---"
		grep -a -A40 'EXYNOS990 LOADER BEGIN' "$1" | head -50
		return 0
	fi
	if grep -aq 'XNU console' "$1"; then
		echo "--- $1 ---"
		grep -a -B3 -A40 'XNU console' "$1" | head -50
		return 0
	fi
	return 1
}

found=0
show "$prefix.lastkmsg.txt" && found=1
show "$prefix.pmsg.txt" && found=1

if [ "$found" = 0 ]; then
	echo
	echo "no loader text in either sink."
	echo "pmsg head (oldest bytes):"
	[ -f "$prefix.pmsg.txt" ] && head -c 600 "$prefix.pmsg.txt"
	echo
	echo "last_kmsg head:"
	[ -f "$prefix.lastkmsg.txt" ] && head -c 400 "$prefix.lastkmsg.txt"
	echo
	echo "If pmsg is only TWRP's recoverycharge_status and last_kmsg only"
	echo "S-Boot text, the loader did not run: the reset is before it."
fi
