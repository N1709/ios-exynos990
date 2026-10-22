# Exynos 990 boot chain: every assumption, and how it is checked

The port fails silently: S-Boot loads our image, the splash screen comes back
in a reset loop and nothing is printed. This file lists, in boot order, every
thing the chain assumes, where the claim comes from, and whether it is
verified. "Measured" means it was read off the phone; "static" means it comes
from the Samsung Android kernel tree only.

| # | Assumption | Source | Status |
| --- | --- | --- | --- |
| 1 | S-Boot reads the `boot` partition as an Android boot image v2, loads the kernel slot at `0x10008000` and the ramdisk slot at `0x11000000` | `tools/mkboot_exynos990.py`, `shim/exynos990/board.h:19-30` | **measured**: the phone accepts the image and resets instead of refusing it |
| 2 | S-Boot jumps to the kernel slot with `x0 = 0` (no DTB) | `shim/exynos990/start.S:1-12`, `loader.c` "no DTB in x0" | static; the loader tolerates it |
| 3 | Entry is at EL1 with the MMU and the caches off | `start.S:14-16`, loader reads `CurrentEL`, `SCTLR_EL1` | **not measured**; if EL2, `ex_main` returns `FAIL: not entered at EL1` (the log will say so) |
| 4 | DRAM: low bank at `0x80000000`, high bank `0x880000000..0xaffffffff` (10 GiB); XNU gets `0x890000000..0x8b0000000` | phone `/proc/iomem`: `880000000-88acfffff : reserved`, next reserved `0x9f0c00000` | **measured** |
| 5 | The first `0xAD00000` of the high bank is `sec_rdx_bootdev`, the RAM-dump carveout, and must not be used | `exynos9830-rmem.dtsi:65-70` + the same `/proc/iomem` line | **measured** (this was the bug that put the loader's 512 MiB zeroing on top of it) |
| 6 | Samsung's watchdog (`watchdog_cl0@10050000`, `watchdog_cl2@10060000`, `timeout-sec = <30>`) may be armed by S-Boot | `exynos9830.dts:1990-2012`; layout `drivers/watchdog/s3c2410_wdt.c:46-56` | **unknown** - the loader now prints `wtcon_cl0/cl2` and disables an armed timer, because nothing in the port pets it and a reset after 30 s looks exactly like the observed loop |
| 7 | The payload container (`SWEETXNU`) is found by scanning the ramdisk region | `tools/mkpayload.py`, `loader.c:find_payload` | static + selftest |
| 8 | Segments are copied by `vmaddr - virt_base`; `virt_base = 0xfffffe0006000000` | `tools/mkpayload.py:151-158` | static; the built payload reports `phys_base = 0x890000000`, `mem_size = 0x20000000` |
| 9 | XNU is entered with `x0` = physical `boot_args`, MMU and caches off | `start.s` in `osfmk/arm64`, `shim/exynos990/start.S:60-74` | static |
| 10 | `boot_args.Revision = 2`, `Version = 2`, ADT VA in `deviceTreeP`, `topOfKernelData` above the ADT | `loader.c:320-340` | static |
| 11 | A framebuffer console exists only if the DECON probe found a scanout buffer and `boot_args.Video` is valid | `loader.c:341-360`, `pexpert/arm/pe_serial.c:fbcon_probe` | **unknown**; if it is, XNU draws on the panel and the splash screen would change |
| 12 | GICv2 at `0x10101000`/`0x10102000`, architected timer at 26 MHz, physical timer INTID 30 | `exynos9830.dts`, `tools/mkadt_exynos990.py:80-90` | static; a wrong INTID shows up as an interrupt storm after the log starts |
| 13 | No usable UART for XNU: the console has to be the framebuffer or the RAM log | `docs/exynos990-hardware.md` §4 | measured (no adapter attached) |
| 14 | The RAM log ring is Samsung's pmsg zone: `log_pstore` (`0xeb308000`, 32 KiB) split into 16 KiB dump + 16 KiB pmsg, so `0xeb30c000` | `debug-snapshot-def.h:66,96-97`, `debug-snapshot-pstore.c:244-247`, `fs/pstore/ram.c:793-816`; phone: `pmsg-ramoops-0` is 16372 bytes = `0x4000 - 12` | **measured** |
| 15 | The ring format is `persistent_ram`: `sig = "DBGC" (0x43474244)`, `start`, `size`, then the data | `fs/pstore/ram_core.c:32-39` | static, matches the A5XELTE sink that TWRP already reads |
| 16 | XNU's `kprintf` reaches the ring even with no serial device registered | `pexpert/arm/pe_serial.c:uart_putc_options_` (patch `0136`) | static; before the patch only the framebuffer console fed the ring |

## What the next boot should show

`/sys/fs/pstore/pmsg-ramoops-0` in TWRP after one failed boot, in order:

```
--- XNU console ---            <- klog_init() ran, PE started
... early kprintf ...
```

or, if XNU never gets that far, the loader's own text ends at

```
JUMP TO XNU
```

and nothing follows - which means the failure is between the jump and
`serial_init()` (page tables, ADT mapping, `PE_init`).

If the file still holds only TWRP's `recoverycharge_status` lines, the loader
never ran: the reset happens before our code, and the next thing to try is
the UART (`0x10540000`, 115200 8N1).

## Assumptions deliberately not made

- no driver for the Exynos watchdog in XNU yet (the loader turns it off; a
  bring-up kernel should not be reset by it)
- no display re-programming: the kernel draws into whatever the bootloader
  left scanning out
- no UFS, touch or iOS-root work is implied by a successful jump
