#!/bin/sh
# exynos990-readlog.sh - capture the RAM log the instant TWRP comes up.
#
# The sink is Samsung's pstore pmsg ring (0xeb30c000, 16 KiB).  TWRP itself
# writes a "recoverycharge_status" line every second (~75 bytes), so it
# overwrites the whole ring in about three and a half minutes.  Waiting
# until TWRP is idle is therefore enough to lose the failed boot's text;
# this script blocks on adb and dumps the file the second adbd appears.
#
# Usage:  tools/exynos990-readlog.sh [output-file]
#   run it BEFORE pressing Vol Up + Power, leave it waiting, then boot TWRP.
set -e

out=${1:-out/exynos990/last-pmsg.txt}
mkdir -p "$(dirname "$out")"

echo "waiting for adbd (boot TWRP now, do not wait after it appears)..."
adb wait-for-device

raw=$(mktemp)
adb exec-out 'cat /sys/fs/pstore/pmsg-ramoops-0' > "$raw" || true
tr -d '\000' < "$raw" > "$out" 2>/dev/null || cp "$raw" "$out"
rm -f "$raw"

echo "saved $out ($(wc -c < "$out" | tr -d ' ') bytes)"

if grep -aq 'EXYNOS990' "$out"; then
	echo "--- loader/XNU text found ---"
	tr -d '\000' < "$out" | grep -a -A40 'EXYNOS990' | head -60
	exit 0
fi

if grep -aq 'XNU console' "$out"; then
	echo "--- XNU started, but the banner line is outside the window ---"
	tr -d '\000' < "$out" | grep -a -A40 'XNU console' | head -60
	exit 0
fi

echo "no EXYNOS990 marker in the ring. oldest bytes first:"
tr -d '\000' < "$out" | head -c 1200
echo
echo
echo "If this is only TWRP's own 'recoverycharge_status' spam, the log was"
echo "already overwritten (or the loader never ran): rerun and dump sooner."
