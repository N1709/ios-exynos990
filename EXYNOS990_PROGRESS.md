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
- **E3 — storage.** The driver reads the host from the ADT node
  (`/exynos990-ufs`: HCI `0x13100000`, vendor block `0x13101100`,
  protector `0x132A0000`) and handles both descriptor types the protector
  can report: 16-byte entries (DESCTYPE 0-2) and the 128-byte FMP
  descriptor (DESCTYPE 3: the standard entry plus the zeroed 112-byte
  crypto table, valid for a bypassed transfer).  9830 specifics that are
  already in: byte granular UTRD offsets, 4 KiB per PRDT entry
  (`PRDT_SET_SIZE(12)`), inverted UTRLCLR and no `USE_OF_HCE`.  Remaining:
  read USERDATA on the phone once XNU runs, and the GPT/partition read
  that E6 needs.
- **E4 — display.** Done differently from the S8: the 9830 has no
  `bootloaderfb` device tree property, but the Samsung bootloader **leaves
  DECON running** (`LCD_ON_UBOOT`, see
  `cal_9830/decon_reg.c:1634-1651`), so the panel is already scanned out at
  hand-off.  The loader probes the DECON registers read-only
  (`shim/exynos990/decon_probe.c`): window geometry, image size, stride,
  format and the address of the bootloader's buffer.  That buffer goes to
  XNU in `boot_args.Video`, so the console (`pe_serial.c`), the framebuffer
  driver (`SweetFramebuffer.cpp`) and the early console (`sweet_early.c`,
  no-op on the 990 because its bootstrap TTBR0 maps no framebuffer) use it.
  The probe's output is the display evidence of the phone: run status,
  video/command mode, window, layer, format, DSIM PLL.  Remaining: confirm
  on the phone which format the bootloader used (the console needs a
  32-bit format) and, if the display must be re-programmed, write the
  window + IDMA configuration from `regs-decon.h`/`regs-dpp.h`.
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

## First CI build (run 39706196610, 2026-10-22)

The `xnu` workflow ran on `macos-14` with `MACHINE_CONFIG=EXYNOS990`; it is
the first real build of the port.  Patches `0125`–`0129` applied, the
`dtrace` tools and `libsyscall` built, then the kernel stopped at once:

```
makedefs/MakeInc.kernel:22: *** Unsupported CURRENT_MACHINE_CONFIG EXYNOS990.  Stop.
make[1]: *** [build_setup_bootstrap_DEVELOPMENT^ARM64^EXYNOS990] Error 2
```

`MakeInc.kernel` validates the machine config against
`SUPPORTED_ARM64_MACHINE_CONFIGS` in `makedefs/MakeInc.def`, and `0125` never
added `EXYNOS990` to that list (`MakeInc.def` had only the
`MACHINE_FLAGS_ARM64_EXYNOS990` line).  Patch `0130` adds it and also adds
the board to the "plain arm64, not arm64e" filter a few lines below, so the
kernel is built without Apple's pointer authentication, which the loader
does not set up.  The build failed before the compile step, so the
`xnu-kernel` artifact of that run holds no kernel and no compile errors:
the next run tells whether the sources themselves build.

`tools/check-exynos990-config.py` now checks both lists, and it fails on a
tree without `0130` — this class of error is caught offline from now on.

## Second CI build (run 39738921422, 2026-10-22)

With `0130` in place the kernel build starts; it failed after ~7 minutes with
ten errors in two groups:

```
libkern/crypto/sweet_aes.c:12:10: fatal error: 'sweet_bearssl/inc/bearssl_block.h' file not found
bsd/pthread/sweet_pthread_synch.c:4:10: fatal error: 'sweet_libpthread/kern/kern_synch.c' file not found
bsd/vm/vm_unix.c:1774:20: error: incomplete definition of type 'struct pid_hibernate_args'
```

1. The stage scripts (`0019-stage-libpthread.sh`, `0020-stage-bearssl.sh`,
   `0026-stage-hfs.sh`) fetched their sources only for `SWEET` and `A5XELTE`
   (`case "${MACHINE_CONFIG:-}" in SWEET|A5XELTE)`) and exited 0 for any
   other name, so the 990 build had no BearSSL and no libpthread sources at
   all.  They now accept `EXYNOS990`.
2. `pid_hibernate` needs `struct pid_hibernate_args`, which the build
   generates from `bsd/kern/syscalls.master` — and there the syscall was
   still guarded by `CONFIG_EMBEDDED || SWEET || A5XELTE`, so the 990 got
   `nosys` and no argument struct.  Patch `0131` adds `EXYNOS990`.

The config checker now also verifies the three stage scripts and that
`pid_hibernate` survives the preprocessor for this board.

## Third CI build (run 39758989801, 2026-10-22)

First run that reached the `bsd` compiles with the include paths fixed (and
the first on `macos-15-intel`; the job started in 3 seconds, while
`macos-14` had been queueing for minutes).  `make -k` hid the error in
thousands of warning lines, so the whole log is now published to the
`ci-logs` branch.  The actual failure:

```
make[7]: *** No rule to make target `.../xnu/bsd/dev/a5xelte_emmc.c', needed by `a5xelte_emmc.o'.
make[7]: *** No rule to make target `.../xnu/bsd/dev/a5xelte_keys.c', needed by `a5xelte_keys.o'.
make[7]: *** No rule to make target `.../xnu/bsd/dev/a5xelte_touch.c', needed by `a5xelte_touch.o'.
```

Patch `0125` renames those three sources to `samsung_emmc.c`,
`samsung_keys.c` and `samsung_touch.c` (the 990 has its own board files),
but it did not update `bsd/conf/files`, which still listed the old names.
Patch `0133` renames the three lines.

`tools/check-exynos990-config.py` now also compares `bsd/conf/files` with
the rename table, so the check fails offline when a source is renamed
without its build-list line — verified on the applied tree with and without
`0133`.

## Fourth and fifth CI builds (39761003089, 39765091076, 2026-10-22)

Both reached the final link.  The fourth one failed with

```
Undefined symbols for architecture arm64:
"_CleanPoC_Dcache", "_FlushPoC_Dcache", "_CleanPoU_Dcache", "_Idle_context",
"_Shutdown_context", "_ResetHandlerData", "_bcopyin", "_bcopyout", ...
```

`osfmk/conf/files.arm64` lists those assembly sources as `optional
nos_arm_asm`, and `doconf` selects `config/MASTER.arm64.<soc>`; there was no
`MASTER.arm64.exynos990`, so it fell back to the generic arm64 master and the
assembly never entered the build.  Patch `0134` adds the master config (copy
of the A5XELTE one, which already carries nos_arm_asm/nos_arm_pmap).

That fixed all of them.  The fifth build then failed with a single symbol:

```
"_dreamlte_battery_read", referenced from:
    SweetBattery::poll() in SweetBattery.cpo
```

`SweetBattery.cpp` grouped EXYNOS990 with A5XELTE and called the Galaxy S8
MAX77865 reader, but that code sits in `bsd/dev/samsung_touch.c` behind
`#if defined(DREAMLTE)`.  Patch `0135` gives the 990 its own branch: no
sample (returns -1), so `poll()` publishes its documented 100% default until
an Exynos 990 fuel-gauge driver exists.

The config checker now also requires `config/MASTER.arm64.exynos990`.

## First green build (run 39770435460, 2026-10-22)

`eb55547`, `macos-15-intel`, job 11:25:16 -> 11:38:56 (13m40s):

```
Build                 success
Collect the kernel    success
upload-artifact       success   xnu-kernel  66,649,065 bytes
```

This is a build claim only.  The kernel has not been run: no boot image has
been made from it yet, nothing has been flashed, and no UART or screen output
has been observed.  Next step: take `out/kernel.development.exynos990` out of
the `xnu-kernel` artifact, run
`tools/exynos990-make-boot.sh <kernel> z3s`, flash the resulting
`boot-z3s.img` to the boot partition and capture the UART log.

## Boot attempt 1: image accepted, nothing on screen (2026-10-22)

`out/exynos990/boot-z3s.img` (sha256 `4c85fe97...788c392`) was flashed to the
boot partition.  The phone shows the Samsung logo, the screen goes off, the
logo comes back: a reset loop.  S-Boot accepts the image and jumps to the
loader (the logo is S-Boot's), but the loader only prints to the USI debug
UART (`0x10540000`) and only *reads* DECON, it never draws, so a failure
after the jump is invisible.  The boot partition has been restored.

Log sinks found on the phone (TWRP/Android, `adb shell`):

```
/dev/pmsg0                 present (misc 247,0)
/proc/last_kmsg            present, 2 MiB  (Samsung sec_log, S-Boot text)
/sys/fs/pstore/dmesg-ramoops-0  present
/sys/fs/pstore/pmsg-ramoops-0   present, 16372 bytes
/dev/mem, /proc/kcore      absent
```

`/proc/iomem` has `eb308000-ed311fff : reserved`: the debug-snapshot area.
Its first 32 KiB is `log_pstore` (`include/dt-bindings/soc/samsung/
debug-snapshot-def.h:66,96-97`), and `lib/debug-snapshot-pstore.c:244-247`
hands it to ramoops as 16 KiB dump + 16 KiB pmsg (`fs/pstore/ram.c:793-816`
puts the pmsg zone last).  `pmsg-ramoops-0` is 16372 bytes = `0x4000 - 12`,
the persistent_ram header, which confirms the ring size.

Patch `0136` therefore sets:

```c
#define SWEET_KLOG_PHYS   0xeb30c000u   /* log_pstore + 16 KiB (pmsg) */
#define SWEET_KLOG_SIZE   0x4000
```

and the loader now writes its own text into the same ring in
`persistent_ram` format (sig `DBGC`, start, size, data), so after the reset
`cat /sys/fs/pstore/pmsg-ramoops-0` shows loader + XNU text without UART.
`pe_serial.c` no longer indexes past the data area when it continues a full
ring (the old check assumed a 512 KiB sink).

Not tested: whether XNU reaches `klog_init()` at all; whether S-Boot leaves
`log_pstore` alone across the reset.

## XNU's RAM window was on top of a bootloader carveout

Asked for work that does not need the phone, the board facts already
collected answer the DRAM question.  `/proc/iomem` from the phone:

```
880000000-88acfffff : reserved
```

and the bootloader DT names that range: `sec_rdx_bootdev`, the RAM-dump
carveout, `<0x8 0x80000000 0x0AD00000>`
(`arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:65-70`).  The next
reserved range is `0x9f0c00000`, so `0x890000000..0x8afffffff` is free RAM.

`shim/exynos990/board.h` and `tools/mkadt_exynos990.py` both put XNU at
`0x880000000`, i.e. every boot the loader zeroed 512 MiB starting inside
that carveout before jumping.  The window now starts at `0x890000000`
(size unchanged, 512 MiB); the payload header rebuilds with
`phys_base = 0x890000000`.

This is a static finding, not a proven boot fix: the phone still has to be
booted once with the new image, and the pstore log read back, before the
JUMP TO XNU / panic point is known.
