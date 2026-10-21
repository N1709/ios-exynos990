# iOS 16 on the Samsung Galaxy S20 family (Exynos 990)

**iOS 16.7 on the Samsung Galaxy S20 family (Exynos 990 / `universal9830`).**

This project ports Apple's open-source XNU kernel (xnu-8796, the iOS 16
generation) to the Exynos 990 phones. On top of it runs the **unmodified iOS
16.7.16 (20H392) userland of the iPhone X**: launchd, the system daemons,
backboardd and SpringBoard.

> ⚠️ Experimental bring-up, not a usable phone OS. Installing it **erases the
> USERDATA partition** (all Android data). Exynos **SM-G988B** (`z3s`, to be
> confirmed from the phone) and the other Exynos 990 models only.

## Target models

| code  | phone                | panel (native) | notes |
|-------|----------------------|----------------|-------|
| `x1s` | Galaxy S20 / S20 5G  | 1440x3200      | `s6e3hab` |
| `y2s` | Galaxy S20+         | 1440x3200      | `s6e3hab` |
| `z3s` | Galaxy S20 Ultra    | 1440x3200      | `s6e3hab` — **the phone of this port** |
| `r8s` | Galaxy S20 FE       | 1080x2400      | `ea8076` |
| `c1s` | Galaxy Note 20      | 1080x2400      | `s6e3hab_fhd` |
| `c2s` | Galaxy Note 20 Ultra| 1440x3088      | `s6e3hac` |
| `win2`| Galaxy S10 5G       | 1440x3040      | `s6e3ha9` |

The boot chain of these phones differs from the older Exynos 8895 (Galaxy S8)
this repository started with: the phone boots from the `boot` partition with
an Android boot image v2, so the port has its own loader in
`shim/exynos990/`.

## Status

| Item | What it is | State |
| --- | --- | --- |
| E1 | Loader (`shim/exynos990/`): arm64 Image for S-Boot, UFS probe, payload copy, jump to XNU | code done, not flashed yet |
| E2 | XNU board configuration (`xnu/patches/0125`, `0126`): machine config `ARM64_BOARD_CONFIG_EXYNOS990`, board constants, renamed drivers | verified statically, not compiled yet |
| E3 | UFS: ADT node, descriptor type (plain PRDT vs FMP), GPT read | partial |
| E4 | Display: DECON/DSIM/DPHY of the 9830 | in progress |
| E5 | Touch: I2C controller + GPIO IRQ per model | not started |
| E6 | iOS root in USERDATA | not started |

**Nothing has been flashed to a phone yet.** The loader is built and packaged
locally, the XNU side is verified with a static checker, and the macOS CI
build has not run yet.

Hardware facts come from the Samsung Android kernel tree
`android_kernel_samsung_exynos990`, never from guesswork; every value is
cited with its source file and line in `docs/exynos990-hardware.md`.

## Layout

| Path | What it is |
| --- | --- |
| `shim/exynos990/` | Loader for the Exynos 990: `loader.c`, `console.c` (USI debug UART), `ufs_probe.c`, `board.h` (every board constant with its source) |
| `tools/mkadt_exynos990.py` | Apple Device Tree for the Exynos 990 (per model) |
| `tools/mkpayload.py` | the `SWEETXNU` container the loader reads (segments, ADT, command line) |
| `tools/mkboot_exynos990.py` | Boot image v2 packer (AOSP `ANDROID!` header) |
| `tools/exynos990-make-boot.sh` | CI kernel → flashable `boot.img`, one command |
| `tools/exynos990-selftest.py` | self test of that chain, without a phone (all seven models) |
| `tools/check-exynos990-config.py` | static check of the XNU board configuration |
| `tools/exynos990-collect.sh` | reads the board facts off the phone (TWRP) |
| `xnu/patches/` | XNU patches, applied in order |
| `docs/exynos990-hardware.md` | hardware facts with the Android kernel source of every value |
| `docs/exynos990-flash.md` | flashing procedure and the UART log |
| `EXYNOS990_PROGRESS.md` | port log: milestones, evidence, open questions |

## Build and flash

```bash
# 1. the XNU kernel comes from the macOS CI (the "xnu-kernel" artifact)
# 2. build the flashable boot image for one model
tools/exynos990-make-boot.sh kernel.development.arm64 z3s
# 3. flash it to the boot partition (backup first)
adb push out/exynos990/boot-z3s.img /tmp/boot.img
adb shell dd if=/tmp/boot.img of=/dev/block/by-name/boot bs=4M conv=fsync
# 4. read the loader log on the debug UART (0x10540000, 115200 8N1)
picocom -b 115200 /dev/ttyUSB0
```

The full procedure, including the board facts to collect, is in
`docs/exynos990-flash.md`.

## Origin

The repository started as the Galaxy S8 port (`dreamlte`, Exynos 8895) and
the Galaxy A5 2016 port (`a5xelte`, Exynos 7580). Those projects are kept
in the history: `DREAMLTE_PROGRESS.md` and `A5XELTE_PROGRESS.md`.

## Credits

- Apple: XNU (xnu-8796), the only source of the kernel code used here
- The Samsung Android kernel `android_kernel_samsung_exynos990`, the only
  source of the hardware facts
- The iOS 16.7.16 (20H392) userland of the iPhone X, unmodified
