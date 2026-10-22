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

| code  | phone                 | panel (native) | touch |
|-------|-----------------------|----------------|-------|
| `x1s` | Galaxy S20 / S20 5G   | 1440x3200 `s6e3hab` | sec_ts `0x48` |
| `y2s` | Galaxy S20+           | 1440x3200 `s6e3hab` | sec_ts `0x48` |
| `z3s` | Galaxy S20 Ultra      | 1440x3200 `s6e3hab` | sec_ts `0x48` — **the phone of this port** |
| `r8s` | Galaxy S20 FE         | 1080x2400 `ea8076` | zinitix `0x20` / STM `0x49` |
| `c1s` | Galaxy Note 20        | 1080x2400 `s6e3hab_fhd` | sec_ts `0x48` |
| `c2s` | Galaxy Note 20 Ultra  | 1440x3088 `s6e3hac` | sec_ts `0x48` |
| `win2`| Galaxy S10 5G         | 1440x3040 `s6e3ha9` | sec_ts `0x48` |

The boot chain differs from the older Exynos 8895 (Galaxy S8) this repository
started with: these phones boot from the `boot` partition with an Android boot
image v2, so the port has its own loader in `shim/exynos990/`.

## Status

| Item | What it is | State | Tested |
| --- | --- | --- | --- |
| E1 | Loader: arm64 Image for S-Boot, UART, UFS probe, payload copy, jump to XNU | done | build + self test, **not on a phone** |
| E2 | XNU board config, patches `0125`–`0130` | done | static checker + `git apply`; first CI build stopped on a missing `SUPPORTED_*` entry (fixed in `0130`), **not compiled yet** |
| E3 | UFS: protector/descriptor type, plain 16-byte and FMP 128-byte PRDT, GPT | 90% | code, not on a phone |
| E4 | Display: DECON probe, `boot_args.Video` handoff, framebuffer console | 85% | code, not on a phone |
| E5 | Touch: bus/IRQ wiring per model, Y79A command set | 40% | wiring only |
| E6 | iOS root in USERDATA (NativeUfs) | 0% | — |

**Nothing has booted on a phone yet, and the XNU kernel has not been compiled
yet** (the macOS CI workflow has never run). Everything above that says
"tested" is offline testing: the loader is cross-built with
`aarch64-linux-gnu-gcc`, the whole boot chain is exercised by
`tools/exynos990-selftest.py` with a stand-in kernel for all seven models, and
the XNU side is checked statically by `tools/check-exynos990-config.py`
because the kernel only builds on macOS.

Hardware facts come from the Samsung Android kernel tree
`android_kernel_samsung_exynos990`, never from guesswork; every value is cited
with its source file and line in `docs/exynos990-hardware.md`.

## Layout

| Path | What it is |
| --- | --- |
| `shim/exynos990/loader.c` | the loader: payload scan, segment copy, `boot_args`, jump to XNU |
| `shim/exynos990/console.c` | USI debug UART console (`0x10540000`, 115200 8N1) |
| `shim/exynos990/ufs_probe.c` | read-only UFS probe (host state, GPT, descriptor type) |
| `shim/exynos990/decon_probe.c` | read-only display probe: the panel the bootloader leaves running |
| `shim/exynos990/board.h` | every board constant with its source |
| `tools/mkadt_exynos990.py` | Apple Device Tree per model |
| `tools/mkpayload.py` | the `SWEETXNU` container the loader reads |
| `tools/mkboot_exynos990.py` | Android boot image v2 packer |
| `tools/exynos990-make-boot.sh` | CI kernel → flashable `boot.img`, one command |
| `tools/exynos990-selftest.py` | self test of the chain, no phone needed (7 models) |
| `tools/check-exynos990-config.py` | static check of the XNU board configuration |
| `tools/exynos990-collect.sh` | reads the board facts off the phone (TWRP) |
| `xnu/patches/` | XNU patches, applied in order (`0125`–`0130`) |
| `docs/exynos990-hardware.md` | hardware facts, each with its Android kernel source |
| `docs/exynos990-flash.md` | flashing procedure and the UART log |
| `EXYNOS990_PROGRESS.md` | port log: milestones, evidence, open questions |

## Build and flash

```bash
# 1. the kernel comes from the macOS CI (artifact "xnu-kernel")
# 2. build the flashable boot image for one model (default z3s)
tools/exynos990-make-boot.sh kernel.development.arm64 z3s
# 3. flash the boot partition (back up first)
adb push out/exynos990/boot-z3s.img /tmp/boot.img
adb shell dd if=/tmp/boot.img of=/dev/block/by-name/boot bs=4M conv=fsync
# 4. watch the loader on the debug UART (0x10540000, 115200 8N1)
picocom -b 115200 /dev/ttyUSB0
```

The full procedure, the UART log and what to read from it are in
`docs/exynos990-flash.md`. The board facts the port still needs (DRAM banks,
timer PPI, log RAM, partitions) come from `tools/exynos990-collect.sh` in
TWRP.

## Known limits

- no framebuffer re-programming: the kernel draws into the buffer the
  bootloader already scans out (the panel format it uses is reported by the
  loader's DECON probe and has to be confirmed on the phone)
- touch: the Y79A power/reset and firmware check are not ported yet, the
  driver stays off
- no iOS root in USERDATA yet
- no phone has booted this port

## Origin

The repository started as the Galaxy S8 port (`dreamlte`, Exynos 8895) and the
Galaxy A5 2016 port (`a5xelte`, Exynos 7580). Those projects are kept in the
history: `DREAMLTE_PROGRESS.md` and `A5XELTE_PROGRESS.md`.

## Credits

- Apple: XNU (xnu-8796), the only source of the kernel code used here
- the Samsung Android kernel `android_kernel_samsung_exynos990`, the only
  source of the hardware facts
- the iOS 16.7.16 (20H392) userland of the iPhone X, unmodified
