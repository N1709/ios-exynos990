# Galaxy S20 family (Samsung Exynos 990 / universal9830) port — progress

Branch `exynos990` ports the A5XELTE/S8 work (iOS 16.7 on Samsung hardware:
Samsung boot chain, Exynos UFS, DECON display handoff, HSI2C touch, GPT
root) from the Galaxy S8 (Exynos 8895) to the **Exynos 990** family.

The port is built *in this repo* from the `ios-exynos990` base; no code is
taken from other projects. Hardware facts come from the Samsung Android
kernel tree `android_kernel_samsung_exynos990` (DTS/DTSI + drivers), each
fact cited in `docs/exynos990-hardware.md`.

Observations on the phone, build results and plans are kept apart.

## Target models

Exynos 990 (universal9830) boards in the Android kernel tree:

| code  | phone                | panel (native) | notes |
|-------|----------------------|----------------|-------|
| x1s   | Galaxy S20 / S20 5G  | 1440x3200      | `s6e3hab` |
| y2s   | Galaxy S20+          | 1440x3200      | `s6e3hab` |
| z3s   | Galaxy S20 Ultra     | 1440x3200      | `s6e3hab` — **the phone of this port** |
| r8s   | Galaxy S20 FE        | 1080x2400      | `ea8076/ea8079/s6e3fc3` |
| c1s   | Galaxy Note 20       | 1080x2400      | `s6e3hab_fhd` |
| c2s   | Galaxy Note 20 Ultra | 1440x3088      | `s6e3hac` |
| win2  | Galaxy S10 5G        | 1440x3040      | `s6e3ha9` |

**The port covers the whole Exynos 990 family**; the tooling takes
`--model` and defaults to `z3s`, the device being brought up here:
**Galaxy S20 Ultra with the Exynos 990** (`z3s`, SM-G988B). The exact
SM-number of the phone (it may be `SM-G988B`, `SM-G988U`, `SM-G988N`,
`SM-G9880`…) is still to be read from the About-phone screen; the board
code that matters for the DTS files is `z3s` either way.

Per-model facts (panel, touch controller, keys) live in one table
(`tools/mkadt_exynos990.py` MODELS) so the rest of the family follows the
same build.

## Hardware facts (from the Android kernel tree)

Full list with sources: `docs/exynos990-hardware.md`.

- SoC: Exynos 990 = universal9830. 8 cores: 4x Cortex-A55, 2x Cortex-A76,
  2x Exynos M5 (`exynos9830.dts` cpu nodes, MPIDR 0x000-0x003 / 0x004-0x005
  / 0x100-0x101).
- GICv2 (GIC-400, `arm,cortex-a15-gic`): GICD `0x10101000`, GICC
  `0x10102000`, VCPU `0x10104000`, CPU `0x10106000`
  (`exynos9830.dts`, `gic:interrupt-controller@10100000`).
- Architected timer: `arm,armv8-timer`, PPI 13/14/11/10, 26 MHz
  (`exynos9830.dts` timer node, `use-clocksource-only`).
  **The Exynos GIC maps PPI n to INTID 16 + n**, not 32 + n: it implements
  only the 16 local SGIs, so its PPI window starts at INTID 16. The A5XELTE
  port (Exynos 7580) runs its virtual timer (PPI 11) on INTID 27 and the
  S8 port (Exynos 8895) its physical timer (PPI 14) on INTID 30 — both on
  hardware — and XNU itself rejects a timer INTID outside 16..31 as "not a
  PPI" (`pexpert/arm/pe_fiq.c`). The tooling therefore defaults to the
  **physical timer, INTID 30**, like the S8 (on Exynos the EL1 virtual
  timer does not reach the secondary cores). `--timer-intid` overrides it;
  **UNVERIFIED on the phone**.
- Debug UART: `usi_uart_dbg: usi@10421000` (`exynos9830-usi.dtsi`),
  S3C24XX register layout.
- UFS: `ufs: ufs@0x13100000` — HCI standard `0x200` @ `0x13100000`, Exynos
  vendor block `0x200` @ `0x13101100`, UNIPRO `0x8000` @ `0x13180000`,
  protector @ `0x132A0000`.
- Display: DECON `decon_f@0x19050000`, DSIM `dsim@0x190C0000`, DPHY
  `0x190E0000` (`exynos9830-dpu.dtsi`).
- Clock controller `0x1a330000` (0x8000).
- DRAM: low bank `0x80000000`, high bank from `0x880000000` (same layout as
  the 8895: the S8 measured `80000000-bc7fffff`, `c0000000-ffffffff`,
  `880000000-8ffffffff`). **To be read from /proc/iomem on the phone.**
- Carve-outs in the low bank: EL3 monitor `0xBAB00000`+0x5500000,
  `sec_debug_magic` `0x80000000`, debug-snapshot areas, `abox`
  `0xf8000000`, modem `0xf0000000` (`exynos9830-rmem.dtsi`).

## Build pieces (this branch)

- `shim/exynos990/` — the loader (this is the new part; the S8's
  `shim/dreamlte/` is the template):
  - `start.S`: arm64 Image header, entry at EL1 with MMU/caches off, no
    DTB expected in x0 (sboot on Exynos passes none), vectors, jump to
    XNU with the MMU off.
  - `console.c`: USI debug UART console (`usi_uart_dbg` @ 0x10421000),
    puts the USI into UART mode like `drivers/tty/serial/samsung.c`.
  - `loader.c`: finds the payload container ("SWEETXNU" v1) in the ramdisk,
    copies the Mach-O segments into the kernel-managed RAM window, places
    ADT + boot_args, jumps to XNU's physical entry.
  - `board.h`: every board constant with its kernel source.
- `tools/mkboot_exynos990.py` — boot image v2 packer (AOSP `ANDROID!`
  header) for the phone's `boot` partition: kernel = loader Image at
  0x10008000, ramdisk = the XNU payload at 0x11000000.
- `shim/exynos990/build.sh` — cross build + packaging.
- `tools/mkadt_exynos990.py` — ADT generator (per model): GICv2, 8 CPUs,
  26 MHz timebase, DRAM 0x880000000 + 512 MiB, UFS node, display node.
- `tools/exynos990-make-boot.sh` — the whole chain from the CI kernel to a
  flashable `boot.img` (ADT → payload → loader → boot image).
- `tools/exynos990-selftest.py` + `tools/mkfakekernel.py` — self test of
  that chain without a phone: every field the loader reads at run time is
  checked (Image header, ADT nodes, payload layout, RAM window against
  `board.h`, boot image slots).  Green for all seven models.
- `tools/check-exynos990-config.py` — static check of the XNU board
  configuration (macro visibility, board constants, donor names, renamed
  drivers, device tree node names).  The CI runs it next to the build,
  because the kernel itself only builds on macOS.

Local cross build of the loader: `bash shim/exynos990/build.sh` — builds
clean with `aarch64-linux-gnu-gcc` (`-Wall -Wextra -Werror`), no absolute
relocations, image ~44 KiB.

Flashing procedure: `docs/exynos990-flash.md`.

## Phone results

_(none yet — milestone E1 has not been flashed)_

## Plan / milestones

- **E1 — loader boots on the phone.** Flash the boot image v2 to the
  `boot` partition (TWRP, keep a backup), read the UART log: the loader
  banner, the payload copy, the jump to XNU. Needs: the exact model code,
  a USB-TTL adapter on the debug UART pins (or a different log sink).
- **E2 — XNU runs.** The XNU side reuses the A5XELTE machine config (4 KiB
  pages, GICv2 support) like the S8 port does; the ADT identity lists
  `samsung,a5xelte` so the platform expert matches. Build on the macOS CI
  (`gh` is not available on this host, so kernel builds cannot run here).
- **E3 — storage.** `bsd/dev/sweet_ufs.c` reads the UFS host from the ADT
  node; the Exynos 9830 vendor SFR block (`0x13101100`) may differ from
  the 8895's — the S8 port's `sweet_ufs.c` is the starting point.
- **E4 — display.** DECON/DSIM for the 9830 is new code: the 8895 DECON
  trigger register (`0x12860070`) does not exist on the 9830
  (`0x19050000` block, `exynos9830-dpu.dtsi`). Until then there is no
  screen console and no main display.
- **E5 — touch.** Per-model I2C controller + GPIO IRQ (x1s: `hsi2c_15`,
  address 0x48, IRQ `gpm21`).
- **E6 — iOS root.** Flash the iPhone X root into USERDATA (like the S8
  port did) and boot with `rd=md0` once storage works.

## Next action

1. **Run the XNU build on the macOS CI.** The repository has no Actions run
   yet (`total_count 0` in the API), so the workflow has to be started by
   hand: GitHub → *Actions* → *xnu* → *Run workflow* (machine config
   `EXYNOS990`).  Until that build is green, the XNU side of the port is
   only verified statically (`tools/check-exynos990-config.py`).
2. **Flash the boot image** (TWRP, backup the `boot` partition first) and
   read the UART log: `tools/exynos990-make-boot.sh <kernel> z3s` builds
   the image, `docs/exynos990-flash.md` has the procedure.  The log gives
   the boot MPIDR, the GIC type, the timer frequency and the UFS
   `prdt_stride` (16 = plain PRDT, 128 = FMP `DESCTYPE 3`).
3. **Collect the board facts** with `tools/exynos990-collect.sh` in TWRP:
   the DRAM banks (`/proc/iomem`), the timer PPI (`/proc/interrupts`),
   the pstore/ramoops address and the partition list.  They fill
   `SWEET_BOOT_MPIDR`, `SWEET_KLOG_PHYS` and the `dram-size` of the ADT.
4. Read the exact SM-number off the About-phone screen and note it here.

After that: E3 (UFS: the FMP descriptor path if `prdt_stride` is 128, and
the read of USERDATA), E4 (DECON), E5 (touch), E6 (iOS root in USERDATA
with the NativeUfs driver).
