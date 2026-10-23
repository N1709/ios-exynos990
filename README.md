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

The boot chain differs from the older Exynos 8895 (Galaxy S8) this repository
started with: these phones boot from the `boot` partition with an Android boot
image v2, so the port has its own loader in `shim/exynos990/`.

## Status

| Item | What it is | State | Tested |
| --- | --- | --- | --- |
| E1 | Loader: arm64 Image for S-Boot, UART, UFS probe, payload copy, jump to XNU | done | flashed: S-Boot accepts the image, the loader reads its DTB, finds the payload, copies all ten segments into the XNU window, builds ADT + `boot_args` and jumps; log ends at `JUMP TO XNU` |
| E2 | XNU board config, patches `0125`–`0141` | in progress | CI builds it and XNU now runs: stage bytes 1–7 and 20–37 are written on the phone (all of `arm_init`, `arm_vm_init` up to and including `arm_vm_physmap_init`). It stops between mark 37 and mark 10, i.e. in `set_mmu_ttb_alternate()` / `ml_enable_monitor()` / `set_mmu_ttb(invalid_ttep)`; `0141` marks those three |
| E3 | UFS: protector/descriptor type, plain 16-byte and FMP 128-byte PRDT, GPT | 90% | code, not on a phone |
| E4 | Display: DECON/DSIM bring-up, framebuffer console | 60% | on the phone: DSIM0 PLL locks (`pll=0x1`), all four lanes report `PHY_READY`, the DECON shadow commit now completes (`upd_spin=0x1`); no pixels yet, because the panel expects a DSC-compressed frame and the DPP/DSIM frame path is not programmed |
| E5 | Touch: bus/IRQ wiring per model, Y79A command set | 40% | wiring only; the z3s bus/IRQ is not confirmed |
| E6 | iOS root in USERDATA (NativeUfs) | 0% | out of scope until E3–E5 run on the phone |

**State 2026-10-23:** XNU runs much further than the first logs suggested.
The old "stops at mark 20" reading was a cache artefact: the marks are now one
atomic byte per stage, written to a page that survives TWRP, and the phone
reports `mkbits: 1 2 3 4 5 6 7 20 … 30` plus `31 33 34 35 36 37` once patch
`0140` is in. So `arm_init` completes (timer, `/chosen`, page size, CPU
topology), and inside `arm_vm_init` everything through
`arm_vm_physmap_init()` completes. The kernel stops before mark 10, the point
where the bootstrap TTBR0 is dropped, so the remaining suspects are
`set_mmu_ttb_alternate()`, `ml_enable_monitor()`, the console relocation and
`set_mmu_ttb(invalid_ttep)`; patch `0141` puts a mark on each. Reads lag one
boot: the loader prints the marks of the previous boot before zeroing the
page, so every mark patch costs two cycles.

Display work is the other track. The bootloader leaves DECON configured for
the panel (`GLOBAL=0x133`, command mode, window 5 → DPP0 at 1440x3200) but
shuts the DSI link down; the panel keeps its last frame in self refresh. The
loader now reopens that link itself (`shim/exynos990/dsim_init.c`), in the
vendor order of `dsim_reg.c`: select DPHY reset control → OSC link clock →
DSIM reset → lanes/escape/word clock → PHY defaults and the D-PHY timings for
the 1157 Mb/s row → assert DPHY reset → PLL (`p=2, m=178, s=2`) → lane power →
release reset → word clock from PLL → link config → HS clock request. On the
phone the PLL locks and all four lanes come up. DECON's shadow commit, which
used to time out, now completes as soon as the update request is followed by
the trigger instead of being polled first. What is still missing is the frame
path: the DPU's SYSMMU is bypassed (CTRL `0x4`, enable is `0x5`) while every
framebuffer address is 36-bit (DRAM starts at `0x880000000`), the IDMA base
and stride registers are shadowed (`DMA_SHD_OFFSET 0x800`), and the panel is
initialised for DSC, so the DPP encoder and the PPS packet have to be
programmed before a frame is understood.

Two facts worth remembering for any register work: the PMU isolation bit at
`0x15860710` cannot be touched from the loader (the access hangs the bus; the
power domains are behind an SMC), and the PHY module base is `0x190E0100`, so
the MC block is at `0x190E0300` and the lanes at `0x190E0400+0x100n` — the
first attempt wrote one page too low and the PLL still locked, which made the
mistake look like a reset problem.

**State 2026-10-22 (late):** the boot chain to XNU is complete on the phone.
S-Boot accepts the v2 image, the loader reads the S-Boot device tree, finds
the payload, copies all ten segments into the XNU window at `0x890000000`,
builds the ADT and `boot_args`, and jumps: `JUMP TO XNU` is the last line of
the loader log. The loader prints through a RAM sink in the ramoops *dump*
zone (`0xeb308000`), which TWRP does not overwrite, so every boot is
diagnosable without UART or framebuffer. What is still unproven is XNU
itself: it has produced no output yet. Patch `0136` moved XNU's own klog
into the same dump zone (run `39809245853`, artifact `xnu-kernel`), so the
next boot either shows its early text or nothing — which separates "XNU dies
before the console exists" from "XNU runs silently".

Offline testing: the loader is cross-built with `aarch64-linux-gnu-gcc`, the
whole boot chain is exercised by `tools/exynos990-selftest.py` with a stand-in
kernel for all seven models, and the XNU side is checked statically by
`tools/check-exynos990-config.py` because the kernel only builds on macOS.

Hardware facts come from the Samsung Android kernel tree
`android_kernel_samsung_exynos990`, never from guesswork; every value is cited
with its source file and line in `docs/exynos990-hardware.md`.

## Layout

| Path | What it is |
| --- | --- |
| `shim/exynos990/loader.c` | the loader: payload scan, segment copy, `boot_args`, jump to XNU |
| `shim/exynos990/console.c` | USI debug UART console (`0x10540000`, 115200 8N1) |
| `shim/exynos990/ufs_probe.c` | read-only UFS probe (host state, GPT, descriptor type) |
| `shim/exynos990/decon_probe.c` | display probe, plus the temporary DPP0/DECON test frame |
| `shim/exynos990/dsim_init.c` | DSIM0 + M4M4 PHY bring-up: reopens the DSI link the bootloader shuts down |
| `shim/exynos990/board.h` | every board constant with its source |
| `tools/mkadt_exynos990.py` | Apple Device Tree per model |
| `tools/mkpayload.py` | the `SWEETXNU` container the loader reads |
| `tools/mkboot_exynos990.py` | Android boot image v2 packer |
| `tools/exynos990-make-boot.sh` | CI kernel → flashable `boot.img`, one command |
| `tools/exynos990-selftest.py` | self test of the chain, no phone needed (7 models) |
| `tools/check-exynos990-config.py` | static check of the XNU board configuration |
| `tools/exynos990-collect.sh` | reads the board facts off the phone (TWRP) |
| `tools/exynos990-readlog.sh` | dumps the RAM log the instant TWRP's adbd appears |
| `xnu/patches/` | XNU patches, applied in order (`0125`–`0141`) |
| `docs/exynos990-hardware.md` | hardware facts, each with its Android kernel source |
| `docs/exynos990-flash.md` | flashing procedure and the UART log |
| `docs/exynos990-boot-audit.md` | every handoff assumption of the boot chain, with its status |
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

## Boot attempts and the RAM log

S-Boot accepts the image, but every boot so far ends in a reset loop: the
Samsung splash screen, the screen goes dark, the splash screen again. The
loader prints to the USI debug UART only (`0x10540000`, 115200 8N1), so with
no UART adapter the failure is invisible - which is why the port now also
writes to a RAM log that survives the reset.

The sink is Samsung's `log_pstore` zone, the first 32 KiB of the
debug-snapshot area (`/proc/iomem` on the phone: `eb308000-ed311fff`).
`lib/debug-snapshot-pstore.c` hands it to ramoops as a 16 KiB dump zone
followed by a 16 KiB pmsg zone, so the ring is at **`0xeb30c000`**. The loader
and XNU both append to it in `persistent_ram` format (sig `DBGC`, start, size,
data), and TWRP exposes it as `/sys/fs/pstore/pmsg-ramoops-0`:

TWRP writes its own `recoverycharge_status` line into that same ring every
second, so it erases a boot's log in about three minutes. Read it the moment
adbd appears, not after TWRP is idle:

```bash
# start this first (it blocks), then boot TWRP with Vol Up + Power
tools/exynos990-readlog.sh
```

or by hand, immediately after adbd comes up:

```bash
adb exec-out 'cat /sys/fs/pstore/pmsg-ramoops-0' > /tmp/pmsg.txt
tr -d '\000' < /tmp/pmsg.txt | grep -a -A40 EXYNOS990
```

TWRP fills that ring in about three minutes, so the loader also mirrors its
own log into Samsung's 2 MiB `log_kernel` buffer (`0xFD900000 + 0x10000`,
`exynos9830-rmem.dtsi:79-81`), one megabyte in - the buffer `/proc/last_kmsg`
shows, where neither S-Boot nor TWRP writes:

```bash
adb shell 'cat /proc/last_kmsg' | tr -d '\000' | grep -a -A40 EXYNOS990
```

That copy only holds the loader's text (XNU writes to the ring), but it is
enough to tell whether the jump was reached, and it survives as long as the
phone is not power-cycled. `tools/exynos990-readlog.sh` dumps both sinks as
soon as adbd appears.

What the log already said in the first attempt (before the sink existed) and
what it is expected to say is in `EXYNOS990_PROGRESS.md`.

## Known limits

- no framebuffer re-programming: the kernel draws into the buffer the
  bootloader already scans out (the panel format it uses is reported by the
  loader's DECON probe and has to be confirmed on the phone)
- touch: the Y79A power/reset and firmware check are not ported yet, the
  driver stays off
- no iOS root in USERDATA yet
- no phone has booted this port: the kernel has not printed anything yet

## Origin

The repository started as the Galaxy S8 port (`dreamlte`, Exynos 8895) and the
Galaxy A5 2016 port (`a5xelte`, Exynos 7580). Those projects are kept in the
history: `DREAMLTE_PROGRESS.md` and `A5XELTE_PROGRESS.md`.

## Credits

- Apple: XNU (xnu-8796), the only source of the kernel code used here
- the Samsung Android kernel `android_kernel_samsung_exynos990`, the only
  source of the hardware facts
- the iOS 16.7.16 (20H392) userland of the iPhone X, unmodified
