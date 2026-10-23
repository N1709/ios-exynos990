#!/sbin/sh
# update-binary for xnu-kernel.zip
#
# TWRP runs this as a shell script (the file starts with #!, so the recovery
# executes it with sh instead of expecting an ELF binary).  It writes xnu.img
# to the "boot" partition and touches nothing else: no recovery, no modem, no
# persist, no EFS.
#
# Arguments from the recovery: <api version> <output fd> <zip path>

OUTFD=$2
ZIP=$3
MODEL=z3s

ui_print() {
	echo "ui_print $1" >&"$OUTFD"
	echo "ui_print" >&"$OUTFD"
}

DEV=$(getprop ro.product.device)
ui_print "xnu-kernel: device $DEV"
if [ "$DEV" != "$MODEL" ]; then
	ui_print "refusing: this image is for $MODEL (SM-G988B), not $DEV"
	exit 1
fi

BLK=/dev/block/by-name/boot
[ -b "$BLK" ] || BLK=/dev/block/sda10
if [ ! -b "$BLK" ]; then
	ui_print "refusing: no boot partition at /dev/block/by-name/boot"
	exit 1
fi

cd /tmp || exit 1
rm -rf xnu-flash
mkdir -p xnu-flash || exit 1
cd xnu-flash || exit 1
if ! unzip -o "$ZIP" xnu.img >/dev/null 2>&1; then
	ui_print "refusing: cannot extract xnu.img from the zip"
	exit 1
fi

# Sanity check before writing: Android boot images start with "ANDROID!".
magic=$(dd if=xnu.img bs=8 count=1 2>/dev/null)
if [ "$magic" != "ANDROID!" ]; then
	ui_print "refusing: xnu.img does not start with ANDROID!"
	exit 1
fi
size=$(wc -c < xnu.img)
ui_print "writing $size bytes to $BLK"
if ! dd if=xnu.img of="$BLK" bs=4M conv=fsync 2>/dev/null; then
	ui_print "dd failed"
	exit 1
fi
sync
ui_print "done: boot partition written, reboot to run it"
exit 0
