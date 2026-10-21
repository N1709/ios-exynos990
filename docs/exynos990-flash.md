# Flashing the Exynos 990 port (Galaxy S20 family)

Everything below is read-only with respect to the phone except the `dd` in
step 3, and it only touches the `boot` partition.  Back up `boot` first
(TWRP: Backup) and keep the recovery, modem, persist and EFS partitions
untouched.

## 0. What is needed

- a USB-TTL adapter on the phone's **debug UART** pins (the USI debug UART
  at `0x10540000`, 115200 8N1, 3.3 V), for example `/dev/ttyUSB0`
- the XNU kernel the macOS CI built (the `xnu-kernel` artifact,
  `kernel.development.*`)
- `adb` with the phone in TWRP

## 1. Build the boot image

One command builds everything from the CI kernel:

```bash
tools/exynos990-make-boot.sh <kernel.development.arm64> z3s
```

It writes `out/exynos990/boot-z3s.img` and prints the sha256.  The model is
one of `x1s y2s z3s r8s c1s c2s win2` (default `z3s` = Galaxy S20 Ultra);
the model decides the panel, the canvas size and the cpu list of the ADT.

The steps of that script, if you want to run them by hand:

| Step | Tool | Result |
|---|---|---|
| ADT | `tools/mkadt_exynos990.py --model z3s z3s.adt` | the Apple device tree the kernel parses |
| payload | `tools/mkpayload.py kernel z3s.adt z3s.payload` | the `SWEETXNU` container (segments + ADT + cmdline) |
| loader | `shim/exynos990/build.sh` | the arm64 `Image` S-Boot loads |
| boot image | `tools/mkboot_exynos990.py --kernel Image --ramdisk z3s.payload --out boot.img` | the `ANDROID!` image for the `boot` partition |

The chain is covered by `python3 tools/exynos990-selftest.py`, which builds
the same chain with a stand-in kernel and checks every field the loader
reads at run time (no phone needed).

## 2. Collect the board facts (before or after the flash)

The Exynos 9830 device tree has no memory node, so the DRAM layout, the
timer interrupt number, the log RAM address and the partition list can only
be read off the running phone:

```bash
adb push tools/exynos990-collect.sh /tmp/
adb shell sh /tmp/exynos990-collect.sh > exynos990-facts.txt
```

Everything it prints is read-only (procfs, sysfs, devicetree).

## 3. Flash

```bash
adb push out/exynos990/boot-z3s.img /tmp/boot.img
adb shell dd if=/tmp/boot.img of=/dev/block/by-name/boot bs=4M conv=fsync
adb reboot
```

## 4. Read the UART log

```bash
picocom -b 115200 /dev/ttyUSB0
```

The loader prints, in this order:

```
EXYNOS990 LOADER BEGIN (ios-galaxy-s8 exynos990 port)
EL=1 SCTLR=... DTB=0
MIDR=... MPIDR=...
CNTFRQ=... GICD_TYPER=... irq lines=...
no DTB in x0 (expected on Exynos)
payload=0x11000000
segs=... virt=... entry=...
UFS probe (read-only)
CAP=... VER=... HCS=... HCE=...
UTRLBA=... DBR=... RSR=...
TXPRDT=... RXPRDT=... NEXUS=... UFSPRSEC=... prdt_stride=16
zeroing XNU RAM...
seg 0 -> 0x880000000 ...
adt_pa=... boot_args=...
entry_pa=...
cmdline: ...
JUMP TO XNU
```

What to read from it:

- **MPIDR** - the boot core; it becomes `SWEET_BOOT_MPIDR`.
- **CNTFRQ** - the architected timer frequency (26 MHz expected).
- **GICD_TYPER / irq lines** - the GIC revision and the number of
  interrupt lines.
- **prdt_stride** - 16 (plain PRDT) or 128 (FMP, `DESCTYPE 3`): this
  decides how the UFS driver has to build its descriptor table.
- **DBR** - non-zero means the bootloader still had requests outstanding.
- after `JUMP TO XNU` nothing more is printed: the kernel console needs
  the framebuffer (E4) or the log RAM (E3).

If the log stops at `FAIL: ...`, the line above the failure says which
check failed (payload not found, RAM outside the high bank, ...).

## 5. What is not in the image yet

- no framebuffer console (the DECON handoff of the 9830 is not written,
  E4), the kernel output has to go to the log RAM once its address is known
- no keys and no touch controller (E5)
- the iOS root in USERDATA is not staged yet (E6), the UFS driver only reads
  the GPT
