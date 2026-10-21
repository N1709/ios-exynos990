#!/bin/bash
# Collect the Exynos 990 board facts that the XNU port still needs.
#
# Run it in TWRP (adb shell) or any root shell on the phone:
#
#   adb push tools/exynos990-collect.sh /tmp/ && adb shell sh /tmp/exynos990-collect.sh > exynos990-facts.txt
#
# Everything in the output is read out of the running kernel (procfs, devicetree,
# sysfs); nothing is written to the phone and no partition is touched.
set -u
out=exynos990-facts.txt
: > "$out"

say() { echo "===== $* =====" | tee -a "$out"; }
run() { echo "\$ $*" | tee -a "$out"; "$@" 2>&1 | tee -a "$out"; echo | tee -a "$out"; }

say "model and build"
for p in ro.product.model ro.product.device ro.product.vendor.model \
         ro.boot.hardware ro.product.board ro.build.fingerprint \
         ro.crypto.type ro.crypto.state ro.crypto.updates_supported; do
	run getprop "$p"
done
run cat /proc/device-tree/model
run cat /proc/device-tree/compatible

say "cpus (MPIDR, cores)"
run cat /proc/cpuinfo
run cat /sys/devices/system/cpu/present
run cat /sys/devices/system/cpu/possible
run cat /sys/devices/system/cpu/cpu0/topology/core_id
run cat /sys/devices/system/cpu/cpu0/topology/physical_package_id
run cat /sys/devices/system/cpu/cpu0/topology/cluster_id

say "timer and interrupts (PPI number)"
run cat /proc/interrupts
run cat /proc/timer_list

say "memory layout (no memory node in the 9830 device tree)"
run cat /proc/iomem
run cat /proc/meminfo
run cat /proc/cmdline

say "device tree memory nodes, if any"
run ls /proc/device-tree/ | head -80
for d in /proc/device-tree/memory*; do
	[ -e "$d" ] || continue
	echo "--- $d/reg ---" | tee -a "$out"
	od -An -tx4 "$d/reg" 2>&1 | tee -a "$out"
done

say "pstore / ramoops (log RAM address for the kernel klog sink)"
run ls /sys/fs/pstore
run cat /proc/config.gz
run ls /proc/device-tree/reserved-memory
for d in /proc/device-tree/reserved-memory/*; do
	[ -e "$d" ] || continue
	echo "--- $d ---" | tee -a "$out"
	od -An -tx4 "$d/reg" 2>&1 | tee -a "$out"
	cat "$d/name" 2>&1 | tee -a "$out"
	echo | tee -a "$out"
done

say "partitions (GPT: userdata, boot, recovery, pmsg)"
run cat /proc/partitions
run ls -l /dev/block/by-name/
run blockdev --getsize64 /dev/block/by-name/userdata

say "ufs host"
run ls /sys/class/scsi_host/
for h in /sys/class/scsi_host/host*; do
	[ -e "$h" ] || continue
	echo "--- $h ---" | tee -a "$out"
	cat "$h/device/model" 2>&1 | tee -a "$out"
	cat "$h/device/vendor" 2>&1 | tee -a "$out"
	cat "$h/device/rev" 2>&1 | tee -a "$out"
	cat "$h/unique_numa_node" 2>&1 | tee -a "$out"
done
run dmesg

say "display panel"
for d in /proc/device-tree/panel* /proc/device-tree/disp*; do
	[ -e "$d" ] || continue
	echo "--- $d ---" | tee -a "$out"
	cat "$d/compatible" 2>&1 | tee -a "$out"
	od -An -tx4 "$d/reg" 2>&1 | tee -a "$out"
	echo | tee -a "$out"
done
run wm size
run wm density

say "touch controller"
run ls /sys/class/input/
run cat /proc/bus/input/devices
run ls /proc/device-tree/ | grep -i -E "touch|ts"

say "gic"
for d in /proc/device-tree/intc* /proc/device-tree/gic*; do
	[ -e "$d" ] || continue
	echo "--- $d ---" | tee -a "$out"
	cat "$d/compatible" 2>&1 | tee -a "$out"
	od -An -tx4 "$d/reg" 2>&1 | tee -a "$out"
	echo | tee -a "$out"
done

say "done"
echo "saved to $out" | tee -a "$out"
