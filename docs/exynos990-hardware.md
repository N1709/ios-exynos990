# Exynos 990 (universal9830) hardware fact sheet

Hardware facts for the Galaxy S20 family port, for use by `EXYNOS990_PROGRESS.md`,
`shim/exynos990/` and the XNU-side work.

**Single source of truth.** Every fact below comes from the Samsung Android
kernel tree at `/home/n1709/app/experimental/android_kernel_samsung_exynos990`
only. Nothing in this file is copied from another project in the workspace.
Each fact cites the file path *relative to that tree root* and the line
number(s).

**Reading the citations.** `arch/arm64/boot/dts/exynos/exynos9830.dts:1146-1158`
means "lines 1146 through 1158 of that file". Line numbers are for the tree as
checked out now; they move if the tree is updated.

**Verification status.** A fact is one of:

- **verified** — stated in the kernel tree; the citation proves it.
- **UNVERIFIED** — not decidable from the tree. Each UNVERIFIED item names
  the observation on the phone that would settle it.

**Scope limits.** This is a hardware fact sheet, not a bring-up plan. It does
not claim that any of it has been observed on a phone. The phone's exact
model code is still unknown (see §7 and §14).

---

## 1. SoC identity and CPU topology

### 1.1 Identity

| Fact | Value | Source |
| --- | --- | --- |
| Root `compatible` | `"samsung,armv8", "samsung,exynos990"` | `arch/arm64/boot/dts/exynos/exynos9830.dts:44` |
| Board `compatible` (universal9830 overlay) | `"samsung,exynos990", "samsung,UNIVERSAL990"` | `arch/arm64/boot/dts/exynos/exynos9830-universal9830.dts:23` |
| Board `model` | `"Samsung UNIVERSAL9830 board based on EXYNOS9830"` | `arch/arm64/boot/dts/exynos/exynos9830-universal9830.dts:33` |
| Chip ID node | `chipid@10000000`, compatible `"samsung,exynos9830-chipid"`, reg `0x10000000` size `0x100` | `arch/arm64/boot/dts/exynos/exynos9830.dts:78-81` |
| Build target | `TARGET_SOC=exynos9830`, `DEFCONFIG=universal9830_defconfig` | `build.config.universal9830:4,7` |
| Base DTB shipped by the build | `arch/arm64/boot/dts/exynos/exynos9830.dtb` | `build.config.universal9830:12` |

The SoC is the Samsung Exynos 990; the kernel tree names the SoC codename
`exynos9830` and the board `universal9830`.

### 1.2 CPU clusters

Eight cores in three clusters. `reg` in a `cpu` node is the MPIDR affinity
value (`Affinity` field), so `reg` values below are the MPIDRs.

| Cluster | CPU | `compatible` | MPIDR (`reg`) | Source |
| --- | --- | --- | --- | --- |
| 0 (little) | `cpu0` | `arm,ananke`, `arm,armv8` | `0x0000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:147-162` |
| 0 | `cpu1` | `arm,ananke`, `arm,armv8` | `0x0001` | `arch/arm64/boot/dts/exynos/exynos9830.dts:163-178` |
| 0 | `cpu2` | `arm,ananke`, `arm,armv8` | `0x0002` | `arch/arm64/boot/dts/exynos/exynos9830.dts:179-194` |
| 0 | `cpu3` | `arm,ananke`, `arm,armv8` | `0x0003` | `arch/arm64/boot/dts/exynos/exynos9830.dts:195-210` |
| 1 (middle) | `cpu4` | `arm,enyo`, `arm,armv8` | `0x0004` | `arch/arm64/boot/dts/exynos/exynos9830.dts:211-226` |
| 1 | `cpu5` | `arm,enyo`, `arm,armv8` | `0x0005` | `arch/arm64/boot/dts/exynos/exynos9830.dts:227-242` |
| 2 (prime) | `cpu6` | `arm,lion`, `arm,armv8` | `0x0100` | `arch/arm64/boot/dts/exynos/exynos9830.dts:243-258` |
| 2 | `cpu7` | `arm,lion`, `arm,armv8` | `0x0101` | `arch/arm64/boot/dts/exynos/exynos9830.dts:259-274` |

The `compatible` strings are Samsung implementation names. The tree ties two
of the three to a core type through the MIDR part numbers:

| `compatible` | Core | Evidence in the tree |
| --- | --- | --- |
| `arm,ananke` | Cortex-A55 | `ARM_CPU_PART_ANANKE` = `0xD05` = `ARM_CPU_PART_CORTEX_A55` (`arch/arm64/include/asm/cputype.h:83`, `:90`); the same name is the little-cluster list in `drivers/soc/samsung/exynos-reboot.c:34-37` |
| `arm,lion` | Samsung Exynos M5 | `ARM_CPU_PART_LION` = `0x004` = `ARM_CPU_PART_SAMSUNG_M5` (`arch/arm64/include/asm/cputype.h:88`, `:92`); `MIDR_SEC_M5` builds on `ARM_CPU_IMP_SEC` (`arch/arm64/include/asm/cputype.h:70`, `:169`) |
| `arm,enyo` | **not named in the tree** | — |

> **UNVERIFIED.** The tree has no `ARM_CPU_PART_ENYO` and no other statement
> of what `arm,enyo` is; its DT cache geometry (64 KiB L1 I/D, 256 KiB L2 —
> `arch/arm64/boot/dts/exynos/exynos9830.dts:218-223`, `:308-315`) does not
> identify the core either. Read the MIDR of a middle core on the phone (e.g.
> `MIDR_EL1` in a debugger) to name it. Note that
> `drivers/soc/samsung/exynos-reboot.c:39-44` lists only the 8895-era
> `arm,mongoose` / `arm,meerkat` / `arm,cortex-a73` as "big", so
> `soc_has_big()` returns 0 on this SoC — do not reuse that list.

Topology, from `cpu-map` in the same file:

- `cluster0` = `cpu0`..`cpu3` — `arch/arm64/boot/dts/exynos/exynos9830.dts:113-126`
- `cluster1` = `cpu4`, `cpu5` — `arch/arm64/boot/dts/exynos/exynos9830.dts:128-135`
- `cluster2` = `cpu6`, `cpu7` — `arch/arm64/boot/dts/exynos/exynos9830.dts:137-144`

All eight cores use `enable-method = "psci"` (same eight nodes above; e.g.
`arch/arm64/boot/dts/exynos/exynos9830.dts:151`).

Cache and clock-domain hints from the same nodes:

| Fact | Value | Source |
| --- | --- | --- |
| Little-cluster L1 I/D | 32 KiB each, 64-byte lines | `arch/arm64/boot/dts/exynos/exynos9830.dts:154-159` (cpu0) |
| Middle/prime L1 I/D | 64 KiB each; middle: 64-byte I and D lines; prime: 128-byte I-cache line, 64-byte D-cache line | `arch/arm64/boot/dts/exynos/exynos9830.dts:218-223` (cpu4), `:250-255` (cpu6) |
| `arm,ananke` L2 | 64 KiB, one per little core, all pointing at the shared `L3_DSU` | `arch/arm64/boot/dts/exynos/exynos9830.dts:276-307` |
| `arm,enyo` L2 | 256 KiB per middle core, pointing at the shared `L3_DSU` | `arch/arm64/boot/dts/exynos/exynos9830.dts:308-323` |
| `arm,lion` L2 / L3 | 2 MiB L2 + a **private** 3 MiB L3 (`L3_LION`), not the DSU | `arch/arm64/boot/dts/exynos/exynos9830.dts:324-345` |
| Shared L3 (DSU) | 1 MiB | `arch/arm64/boot/dts/exynos/exynos9830.dts:332-338` |
| Power-management siblings | cluster0 `"0-3"`, cluster1 `"4-5"`, cluster2 `"6-7"`; system idle `"0-7"` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1468-1510` |

`cpupm` in that file also gives the vendor power-state indices: cluster power
states use `psci-index = <128>`, system idle uses `psci-index = <256>`
(`arch/arm64/boot/dts/exynos/exynos9830.dts:1471`, `:1480`, `:1489`, `:1505`).
These are vendor extensions, not standard PSCI function IDs.

---

## 2. Interrupt controller

The node is GICv2 (GIC-400 class), **not** GICv3.

```
gic:interrupt-controller@10100000 {
        compatible = "arm,cortex-a15-gic", "arm,cortex-a9-gic";
        #interrupt-cells = <3>;
        #address-cells = <0>;
        interrupt-controller;
        reg =  <0x0 0x10101000 0x1000>,
               <0x0 0x10102000 0x1000>,
               <0x0 0x10104000 0x2000>,
               <0x0 0x10106000 0x2000>;
        interrupts = <GIC_PPI 9 (GIC_CPU_MASK_SIMPLE(4) | IRQ_TYPE_LEVEL_HIGH)>;
};
```

— `arch/arm64/boot/dts/exynos/exynos9830.dts:1134-1144`

### 2.1 Register frames

| reg index | Address | Size | Role | Source |
| --- | --- | --- | --- | --- |
| 0 | `0x10101000` | `0x1000` | GICD — distributor | `arch/arm64/boot/dts/exynos/exynos9830.dts:1139` |
| 1 | `0x10102000` | `0x1000` | GICC — CPU interface | `arch/arm64/boot/dts/exynos/exynos9830.dts:1140` |
| 2 | `0x10104000` | `0x2000` | GIC virtual interface control (VGIC) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1141` |
| 3 | `0x10106000` | `0x2000` | GIC virtual CPU interface (VCPU) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1142` |

The meaning of reg[2]/reg[3] is fixed by the binding: "The first additional
region is the GIC virtual interface control register base and size. The 2nd
additional region is the GIC virtual cpu interface register base and size." —
`Documentation/devicetree/bindings/interrupt-controller/arm,gic.txt:100-103`.

The parent node (the GIC itself, i.e. the virtual GIC seen by the guest) is
signalled by `interrupts = <GIC_PPI 9 ...>`: a virtual-GIC maintenance
interrupt, PPI 9, to CPUs 0-3 (`GIC_CPU_MASK_SIMPLE(4)`),
`arch/arm64/boot/dts/exynos/exynos9830.dts:1143`.

### 2.2 Why this is GICv2

- `compatible` is `"arm,cortex-a15-gic", "arm,cortex-a9-gic"`
  (`arch/arm64/boot/dts/exynos/exynos9830.dts:1135`).
- The GICv2 driver registers that string:
  `IRQCHIP_DECLARE(cortex_a15_gic, "arm,cortex-a15-gic", gic_of_init);` —
  `drivers/irqchip/irq-gic.c:1514`; the A9 variant is `drivers/irqchip/irq-gic.c:1515`.
- The GICv3 driver only matches `"arm,gic-v3"`:
  `IRQCHIP_DECLARE(gic_v3, "arm,gic-v3", gicv3_of_init);` —
  `drivers/irqchip/irq-gic-v3.c:1355`. There is no GICv3 match here.
- The GICv2 driver maps only two of the four `reg` entries: distributor at
  index 0 and CPU interface at index 1 —
  `gic->raw_dist_base = of_iomap(node, 0);` (`drivers/irqchip/irq-gic.c:1393`)
  and `gic->raw_cpu_base = of_iomap(node, 1);` (`drivers/irqchip/irq-gic.c:1397`).
  The two virtual frames are consumed by the virtual-GIC side, not by
  `irq-gic.c`.

### 2.3 INTID numbering

GICv2 has no INTID range register, so an INTID is simply the interrupt number:
SPIs are `32 + GIC_SPI_number`, PPIs are `32 + PPI_number` (PPI 0 is INTID 32).

| Timer interrupt | PPI | INTID | Source of the PPI |
| --- | --- | --- | --- |
| Virtual timer | 11 | **43** | `arch/arm64/boot/dts/exynos/exynos9830.dts:1152-1153` |
| Non-secure physical timer | 14 | **46** | `arch/arm64/boot/dts/exynos/exynos9830.dts:1150-1151` |
| Secure physical timer | 13 | 45 | `arch/arm64/boot/dts/exynos/exynos9830.dts:1148-1149` |
| Hypervisor timer | 10 | 42 | `arch/arm64/boot/dts/exynos/exynos9830.dts:1154-1155` |

SPI numbering used by the DT is the raw GIC SPI number, e.g.
`INTREQ__UFS_EMBD = 321` → INTID 353
(`include/dt-bindings/interrupt-controller/exynos9830.h:339`).

> **UNVERIFIED (important).** The INTID arithmetic above follows the GICv2
> rule. The A5XELTE and S8 ports in this repo used INTID 27 and 30 for a
> "PPI 11" virtual timer (`DREAMLTE_PROGRESS.md:18`), i.e. a *different*
> numbering scheme. On the phone, read the real INTID from the GICD (or let
> XNU print the unexpected INTID) before hard-coding 43.

---

## 3. Architected timer

```
timer {
        compatible = "arm,armv8-timer";
        interrupts = <GIC_PPI 13 (GIC_CPU_MASK_SIMPLE(8) | IRQ_TYPE_LEVEL_LOW)>,
                     <GIC_PPI 14 (GIC_CPU_MASK_SIMPLE(8) | IRQ_TYPE_LEVEL_LOW)>,
                     <GIC_PPI 11 (GIC_CPU_MASK_SIMPLE(8) | IRQ_TYPE_LEVEL_LOW)>,
                     <GIC_PPI 10 (GIC_CPU_MASK_SIMPLE(8) | IRQ_TYPE_LEVEL_LOW)>;
        clock-frequency = <26000000>;
        use-clocksource-only;
};
```

— `arch/arm64/boot/dts/exynos/exynos9830.dts:1146-1158`
(identical node in the EVT0 variant: `arch/arm64/boot/dts/exynos/exynos9830_evt0.dts:931-943`)

| Fact | Value | Source |
| --- | --- | --- |
| Compatible | `arm,armv8-timer` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1147` |
| Interrupt order | secure, non-secure, virtual, hypervisor | `Documentation/devicetree/bindings/timer/arm,arch_timer.txt:17-18` |
| PPI list | 13, 14, 11, 10 — all to CPUs 0-7 (`GIC_CPU_MASK_SIMPLE(8)`), level low | `arch/arm64/boot/dts/exynos/exynos9830.dts:1148-1155` |
| Main counter frequency | **26 000 000 Hz** | `arch/arm64/boot/dts/exynos/exynos9830.dts:1156` |
| Clocksource only | `use-clocksource-only` present | `arch/arm64/boot/dts/exynos/exynos9830.dts:1157` |
| Oscillator node (model overlay) | `oscclk` `clock-frequency = <26000000>` | `arch/arm64/boot/dts/samsung/exynos990-x1s_common.dtsi:32-37` |

The interrupt order is also confirmed by the driver: the DT index maps to
`enum arch_timer_ppi_nr { ARCH_TIMER_PHYS_SECURE_PPI, ARCH_TIMER_PHYS_NONSECURE_PPI,
ARCH_TIMER_VIRT_PPI, ARCH_TIMER_HYP_PPI }`
(`include/clocksource/arm_arch_timer.h:41-47`), and the node is parsed in that
order — `for (i = ARCH_TIMER_PHYS_SECURE_PPI; i < ARCH_TIMER_MAX_TIMER_PPI; i++)
arch_timer_ppi[i] = irq_of_parse_and_map(np, i);`
(`drivers/clocksource/arm_arch_timer.c:1309-1310`).

All interrupts in the DT are routed through the GIC: the root node sets
`interrupt-parent = <&gic>` (`arch/arm64/boot/dts/exynos/exynos9830.dts:47`).

The node is matched by `TIMER_OF_DECLARE(armv8_arch_timer, "arm,armv8-timer", arch_timer_of_init);`
— `drivers/clocksource/arm_arch_timer.c:1357`.

**Counters at EL0.** The architected timer is a per-CPU CP15 timer, so
`CNTPCT_EL0` (physical counter) and `CNTVCT_EL0` (virtual counter) are the
EL0-accessible registers; the driver reads them with `read_sysreg(cntpct_el0)`
/ `read_sysreg(cntvct_el0)` (`drivers/clocksource/arm_arch_timer.c:306-320`).
`CNTVCT_EL0` is the default clocksource (`arch_timer_read_counter =
arch_counter_get_cntvct`, `drivers/clocksource/arm_arch_timer.c:164`) and the
default virtual timer PPI (`static enum arch_timer_ppi_nr arch_timer_uses_ppi =
ARCH_TIMER_VIRT_PPI;`, `drivers/clocksource/arm_arch_timer.c:76`).

**MCT (Samsung's own timer) is a separate block** at `0x10040000`, size `0x800`,
with its own interrupt controller and 12 SPIs mapped to `INTREQ__MCT_G0..G7` /
`INTREQ__MCT_L0..L7` — `arch/arm64/boot/dts/exynos/exynos9830.dts:1178-1208`.
It also carries `use-clockevent-only` (`arch/arm64/boot/dts/exynos/exynos9830.dts:1189`),
i.e. it is not the system clocksource. XNU should use the architected timer.

> **UNVERIFIED.** `clock-frequency = <26000000>` is a DT override for broken
> firmware that does not program `CNTFRQ` uniformly. On the phone, read
> `CNTFRQ_EL0` (the S8 boot CPU reported `CNTFRQ_EL0 = 0` while its DT also
> said 26 MHz — `DREAMLTE_PROGRESS.md:13`). If `CNTFRQ_EL0` is 0 or wrong,
> the timebase must be taken from the DT value, not from the register.

---

## 4. Debug UART

Two nodes matter, and they are **not** the same thing.

### 4.1 The USI mux node (`usi_uart_dbg`)

```
/* USI_UART_DBG */
usi_uart_dbg: usi@10421000 {
        compatible = "samsung,exynos-usi-v2";
        reg = <0x0 0x10421000 0x4>;
        /*  usi_v2_mode = "i2c" */
        status = "disabled";
};
```

— `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:529-535`

| Fact | Value | Source |
| --- | --- | --- |
| Base | `0x10421000` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:532` |
| Block size | `0x4` (the USI channel register, not the UART FIFO) | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:532` |
| Compatible | `samsung,exynos-usi-v2` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:531` |
| Default status | `disabled` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:534` |

### 4.2 The actual debug UART node (`serial_0`)

```
/* USI_PERIC0_UART_DBG */
serial_0: uart@10540000 {
        compatible = "samsung,exynos-uart";
        samsung,separate-uart-clk;
        reg = <0x0 0x10540000 0x100>;
        samsung,fifo-size = <256>;
        interrupts = <GIC_SPI INTREQ__DBG_UART IRQ_TYPE_LEVEL_HIGH>;
        pinctrl-names = "default";
        pinctrl-0 = <&uart0_bus>;
        samsung,usi-serial-v2;
        clocks = <&clock GATE_PERIC0_TOP0_QCH_UART_DBG>, <&clock UART_DBG>;
        samsung,dbg-uart-ch;
        samsung,dbg-uart-baud = <115200>;
        samsung,dbg-word-len = <8>;
        clock-names = "gate_uart_clk0", "ipclk_uart0";
        status = "disabled";
};
```

— `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1917-1933`

| Fact | Value | Source |
| --- | --- | --- |
| UART register base | `0x10540000` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1921` |
| Block size | `0x100` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1921` |
| FIFO size | 256 bytes | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1922` |
| Interrupt | `INTREQ__DBG_UART` = SPI 391 → INTID 423 | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1923`; `include/dt-bindings/interrupt-controller/exynos9830.h:409` |
| Baud rate | 115200 | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1929` |
| Word length | 8 | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1930` |
| Pins | `uart0_bus` = `gpp4-6`, `gpp4-7`, function 3 | `arch/arm64/boot/dts/exynos/exynos9830-pinctrl.dtsi:912-917` |
| Pin controller | `pinctrl_0` @ `0x15850000` (ALIVE) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1211-1213` |
| Clocks | `GATE_PERIC0_TOP0_QCH_UART_DBG` (gate) + `UART_DBG` (ipclk) | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1927`; `include/dt-bindings/clock/exynos9830.h:66,92` |

> **Correction needed in this repo.** `shim/exynos990/board.h:38` sets
> `UART_BASE 0x10421000`, citing `usi_uart_dbg`. That is the *USI mux*
> address with a 4-byte `reg`; the UART registers are at `0x10540000`.
> Writing S3C24XX registers at `0x10421000` will not produce UART output.
> The `usi_uart_dbg` name in the DT is a Samsung naming convention for the
> USI channel that *may* become UART; the functional UART node is
> `serial_0`.
>
> **UNVERIFIED.** Which of the two addresses the board's debug pads are
> actually wired to, and whether `samsung,dbg-uart-ch` (the debug channel)
> is enabled at runtime, can only be settled on the phone: scope the TX pad
> while the Android bootloader prints, and read the live DT
> (`/sys/firmware/fdt`).

### 4.3 S3C24XX register layout

Register offsets used by the Samsung USI/UART driver
(`include/linux/serial_s3c.h`):

| Register | Offset | Source |
| --- | --- | --- |
| `S3C2410_ULCON` | `0x00` | `include/linux/serial_s3c.h:21` |
| `S3C2410_UCON` | `0x04` | `include/linux/serial_s3c.h:22` |
| `S3C2410_UFCON` | `0x08` | `include/linux/serial_s3c.h:23` |
| `S3C2410_UMCON` | `0x0C` | `include/linux/serial_s3c.h:24` |
| `S3C2410_UTRSTAT` | `0x10` | `include/linux/serial_s3c.h:26` |
| `S3C2410_UERSTAT` | `0x14` | `include/linux/serial_s3c.h:27` |
| `S3C2410_UFSTAT` | `0x18` | `include/linux/serial_s3c.h:28` |
| `S3C2410_UMSTAT` | `0x1C` | `include/linux/serial_s3c.h:29` |
| `S3C2410_UTXH` | `0x20` | `include/linux/serial_s3c.h:20` |
| `S3C2410_URXH` | `0x24` | `include/linux/serial_s3c.h:19` |
| `S3C2410_UBRDIV` | `0x28` | `include/linux/serial_s3c.h:25` |
| `S3C2410_UINTP` | `0x30` | `include/linux/serial_s3c.h:30` |
| `S3C2410_UINTS` | `0x34` | `include/linux/serial_s3c.h:31` |
| `S3C2410_UINTM` | `0x38` | `include/linux/serial_s3c.h:32` |
| `USI_CON` | `0xC4` | `include/linux/serial_s3c.h:34` |
| `USI_OPTION` | `0xC8` | `include/linux/serial_s3c.h:35` |

`UTRSTAT` bits: `RXDR` = bit 0, `TXFE` = bit 1, `TXE` = bit 2, `TIMEOUT` = bit 3
— `include/linux/serial_s3c.h:181-184`.

`USI_CON` / `USI_OPTION` bits: `USI_SET_RESET` = bit 0,
`USI_HWACG_CLKREQ_ON` = bit 1, `USI_HWACG_CLKSTOP_ON` = bit 2
— `include/linux/serial_s3c.h:37-40`.

Driver behaviour worth copying into the loader console:

- The USI-v2 mode is selected by the presence of the `samsung,usi-serial-v2`
  property (`drivers/tty/serial/samsung.c:2224-2227`); the node has it
  (`arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1926`). In USI-v2 mode the
  driver skips the legacy IR-mode bits in `ULCON`
  (`drivers/tty/serial/samsung.c:1311-1312`).
- Reset sequence: `USI_CON = USI_SET_RESET`, then `USI_CON = USI_RESET`
  (`drivers/tty/serial/samsung.c:1645-1649`).
- Clock gating handshake: `USI_OPTION = USI_HWACG_CLKREQ_ON` before touching the
  FIFO and `USI_OPTION = USI_HWACG_CLKSTOP_ON` when done
  (`drivers/tty/serial/samsung.c:1653-1664`).
- Baud divisor: `UBRDIV` from the clock rate, with a 4-bit `udivslot` table
  (`drivers/tty/serial/samsung.c:1239-1270`, written at
  `drivers/tty/serial/samsung.c:1331-1335`).
- The debug channel forces its own baud/word length
  (`drivers/tty/serial/samsung.c:2313-2327`) — 115200/8 from the DT.

---

## 5. UFS

```
ufs: ufs@0x13100000 {
        compatible ="samsung,exynos-ufs";
        ...
        reg =
            <0x0 0x13100000 0x200>,  /* 0: HCI standard */
            <0x0 0x13101100 0x200>,  /* 1: Vendor specificed */
            <0x0 0x13180000 0x8000>, /* 2: UNIPRO */
            <0x0 0x132A0000 0x100>;  /* 3: UFS protector */
        interrupts = <GIC_SPI INTREQ__UFS_EMBD IRQ_TYPE_LEVEL_HIGH>;
        pinctrl-names = "default";
        pinctrl-0 = <&ufs_rst_n &ufs_refclk_out &xboot_ldo>;
        clocks =
            <&clock GATE_UFS_EMBD>,   /* aclk clock */
            <&clock UFS_EMBD>;        /* unipro clocks */
        clock-names = "GATE_UFS_EMBD", "UFS_EMBD";
```

— `arch/arm64/boot/dts/exynos/exynos9830.dts:1520-1547`

### 5.1 Register ranges

| reg index | Address | Size | Role | Source |
| --- | --- | --- | --- | --- |
| 0 | `0x13100000` | `0x200` | UFS HCI standard registers | `arch/arm64/boot/dts/exynos/exynos9830.dts:1530` |
| 1 | `0x13101100` | `0x200` | Exynos vendor-specific block | `arch/arm64/boot/dts/exynos/exynos9830.dts:1531` |
| 2 | `0x13180000` | `0x8000` | UNIPRO (Samsung UFS controller IP) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1532` |
| 3 | `0x132A0000` | `0x100` | UFS protector | `arch/arm64/boot/dts/exynos/exynos9830.dts:1533` |

> Note: the "UFS protector" is a **register range of the UFS hardware**, not a
> reserved-memory carve-out. It is not DRAM and must not be excluded from the
> memory map like §9.

Child nodes of the same controller:

| Node | Address | Size | Source |
| --- | --- | --- | --- |
| `ufs-phy` | `0x13104000` | `0x3000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1584-1590` |
| `ufs-io-coherency` (SYSREG) | `0x13020700` | `0x4` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1593-1602` |
| `ufs-cport` | `0x13108000` | `0x804` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1604-1607` |

### 5.2 Interrupt

| Fact | Value | Source |
| --- | --- | --- |
| Interrupt | `INTREQ__UFS_EMBD` = SPI 321 → INTID 353 | `arch/arm64/boot/dts/exynos/exynos9830.dts:1534`; `include/dt-bindings/interrupt-controller/exynos9830.h:339` |
| Trigger | level high | `arch/arm64/boot/dts/exynos/exynos9830.dts:1534` |

### 5.3 Clock gates

| Gate | ID | CMU block | Gate register | Source |
| --- | --- | --- | --- | --- |
| `GATE_UFS_EMBD` (aclk) | `CLK_HSI1_BASE + 23` | CMU_HSI1 `0x13000000` | `GATE_CLKCMU_HSI1_UFS_EMBD` @ `0x209c` in CMU_TOP | `include/dt-bindings/clock/exynos9830.h:531`; `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:834` |
| `UFS_EMBD` (unipro ipclk) | `CLK_HSI1_BASE + 26` | CMU_HSI1 | `CLK_CON_GAT_GOUT_BLK_HSI1_*_ACLK_UFS_EMBD` family @ `0x2080` | `include/dt-bindings/clock/exynos9830.h:534`; `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1361-1363` |

Additional UFS clock-tree SFRs in CMU_HSI1: `ACLK_UFS_UNIPRO` @ `0x2084`,
`ACLK_UFS_FMP` @ `0x2088` (`drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1361-1363`).

### 5.4 Pinctrl

| Signal | Pin | Function | Source |
| --- | --- | --- | --- |
| `ufs_rst_n` | `gpf2-1` | 2 | `arch/arm64/boot/dts/exynos/exynos9830-pinctrl.dtsi:711-717` |
| `ufs_refclk_out` | `gpf2-0` | 2 | `arch/arm64/boot/dts/exynos/exynos9830-pinctrl.dtsi:719-725` |
| `xboot_ldo` | `gpg1-0` | — | `arch/arm64/boot/dts/exynos/exynos9830-pinctrl.dtsi:2223-2227` |

The controller's VCC is a fixed regulator switched by `gpg1-0`
(`arch/arm64/boot/dts/exynos/exynos9830.dts:1611-1615`).

> **UNVERIFIED.** UFS capacity, LUN geometry, block size and the descriptor
> layout are properties of the flash, not of the tree. Read the UFS
> descriptor from the phone (the `ufs` sysfs descriptors, or the TWRP
> `UFS` log) before writing the GPT reader.

---

## 6. Display (DECON / DSIM / DPHY / DPP / DisplayPort)

All display nodes are in `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi`.

### 6.1 DECON

| Fact | Value | Source |
| --- | --- | --- |
| `decon_f` base | `0x19050000`, size `0x10000` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:306-309` |
| `decon_s` base | `0x19060000`, size `0x10000` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:360-363` |
| `decon_t` base | `0x19070000`, size `0x10000` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:399-402` |
| Compatible | `samsung,exynos9-decon` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:307` |
| Chip version | `chip_ver = <9830>` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:340` |
| `decon_f` PSR mode | `2` = **MIPI command mode** | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:326` |
| `decon_f` trigger mode | `0` = **hardware trigger** | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:327` |
| `decon_f` DSI mode | `0` = single DSI | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:328` |
| `decon_f` output type / index | `out_type = <0>` (DSI), `out_idx = <0>` (DSI0) | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:331-333` |
| `decon_f` ppc / line mem / cycle | `ppc = <2>`, `line_mem_cnt = <4>`, `cycle_per_line = <8>` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:336-338` |
| `decon_f` windows | `max_win = <6>`, `default_win = <5>`, `default_ch = <0>` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:323-325` |
| `decon_f` dpp/dsim/decon counts | `dpp_cnt = <7>`, `dsim_cnt = <2>`, `decon_cnt = <3>` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:342-344` |
| `decon_f` interrupts | `INTREQ__DPU_DECON0_FRAME_START` (254), `..._FRAME_DONE` (253), `..._EXTRA` (252), `INTREQ__GPIO_PERIC1` (417) | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:311-315`; `include/dt-bindings/interrupt-controller/exynos9830.h:270-272,435` |
| `decon_f` TE interrupt | GPIO `gpc0-4`, flag `0xf` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:351-352` |
| `decon_f` sw-TE pending register | `0x10730a14` size `0x4` (`NWEINT_GPC0_PEND`) | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:354-357` |
| `decon_f` TE pinctrl | `decon_f_te_on` / `decon_f_te_off` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:319-321` |

`decon_s` is `out_type = <2>` (DisplayPort) and `decon_t` likewise
(`arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:378,417`), i.e. the internal
panel hangs off `decon_f`.

### 6.2 DSIM

| Fact | Value | Source |
| --- | --- | --- |
| `dsim_0` node | `dsim@0x190C0000`, compatible `samsung,exynos9-dsim` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:269-270` |
| reg[0] | `0x190C0000` size `0x300` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:272` |
| reg[1] | `0x190E0100` size `0x700` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:272` |
| reg[2] | `0x190E0000` size `0x100` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:272` |
| Interrupt | `INTREQ__DPU_DSIM0` = SPI 274 → INTID 306 | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:273`; `include/dt-bindings/interrupt-controller/exynos9830.h:292` |
| PHY | `mipi_phy_dsim0_m4m4` = `dphy_m4m4_dsim0@0x190E0000`, compatible `samsung,mipi-phy-m4m4-top` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:261-267` |
| Clock | `GATE_DPU_DPU`, clock name `"aclk"` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:277-278` |

### 6.3 DPP, DisplayPort, other DPU blocks

| Node | reg[0] | reg[1] | Notes | Source |
| --- | --- | --- | --- | --- |
| `dpp_0` | `0x19091000` | `0x19041000` (+ `0x19090000`) | GF0_L0 | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:48-52` |
| `dpp_1` | `0x19092000` | `0x19042000` | GF1_L1 | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:95-98` |
| `dpp_2` | `0x19093000` | `0x19043000` | VG_L2 | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:112-115` |
| `dpp_3` | `0x19094000` | `0x19044000` | VGS_L3 | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:138-141` |
| `dpp_4` | `0x19095000` | `0x19045000` | VGF_L4 | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:167-170` |
| `dpp_5` | `0x19096000` | `0x19046000` | VGRFS_L5 | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:193-196` |
| `dpp_6` | `0x19097000` | `0x19080000` | **used for writeback**; "dpp_6 becomes output device of DECON" | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:223-254` |
| `disp_ss` | `0x19021000` size `0x10` | — | `samsung,exynos9-disp_ss` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:256-259` |
| `displayport` | `0x10AB0000` size `0xFFFF` | `0x10AE0000` size `0x20CC` | `INTREQ__DP_LINK` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:294-298` |

### 6.4 Display clock gates

| Gate | ID | CMU block | Register | Source |
| --- | --- | --- | --- | --- |
| `GATE_DPU_DPU` | `CLK_DPU_BASE + 1` | CMU_TOP `0x1a330000` | `GATE_CLKCMU_DPU` @ `0x2060` | `include/dt-bindings/clock/exynos9830.h:414`; `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:866` |
| `GATE_CLKCMU_DPU_BUS` | — | CMU_TOP | `0x2064` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:837` |
| `ACLK_DECON` | — | CMU_DPU `0x19000000` | `0x200c` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1235` |
| `ACLK_DMA` | — | CMU_DPU | `0x2010` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1236` |
| `ACLK_DPP` | — | CMU_DPU | `0x2014` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1237` |
| `ACLK_DPU_WB_MUX` | — | CMU_DPU | `0x2018` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1233` |
| `APB_DECON0_PCLKM` | — | CMU_DPU | `0x2008` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1220` |

In the DT the **same** gate `GATE_DPU_DPU` is referenced by the DSIM, the
DPP host and the DisplayPort controller, all with clock name `"aclk"`
(`arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:252-253`, `:277-278`,
`:302-303`).

Gate register bitfields (all CMU gate registers): `CG_VAL` = bit 21,
`MANUAL` = bit 20, `ENABLE_AUTOMATIC_CLKGATING` = bit 28 — e.g.
`drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:5132-5134` (DPU),
`:5036-5038` (HSI1 UFS), `:6239-6241` (ACLK_DECON).

### 6.5 Difference from the 8895 DECON used by the S8 port in this repo

The S8 (Exynos 8895) loader in this repo writes the DECON trigger register:

```
#define DECON_TRIG       0x12860070UL  /* DECON_F HW_SW_TRIG_CONTROL */
#define DECON_TRIG_UNMASK 0x1281U      /* uniLoader dreamlte */
...
*(volatile uint32_t *)DECON_TRIG = DECON_TRIG_UNMASK;
```

— `shim/dreamlte/loader.c:31-32,298`

That address is in the 8895's DECON block. On the 9830:

- The DECON block is `0x19050000`..`0x1905ffff` (`decon_f`,
  `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:306-309`), so
  `0x12860070` is not a DECON register at all on this SoC.
- The 9830 DT says the panel is driven in **hardware trigger** mode
  (`trig_mode = <0>`, `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:327`) and
  in **MIPI command mode** (`psr_mode = <2>`,
  `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:326`).
- The equivalent "unmask" concept on the 9830 is not described in the tree:
  the DT only exposes the software TE pending register `0x10730a14`
  (`arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:354-357`).

> **UNVERIFIED.** The 9830 DECON "HW_SW_TRIG_CONTROL" offset, the window /
> block register layout and the correct framebuffer address are **not** in the
> kernel tree — the tree has no DECON register documentation and the DECON
> driver for `exynos9-decon` in this tree is the Exynos-DRM one. Settle these
> on the phone by dumping the DECON registers before and after the Android
> bootloader hands over, and by matching the S8 approach of "unmask the
> trigger" against the 9830's own register names.

### 6.6 Panel (x1s family, for reference)

| Fact | Value | Source |
| --- | --- | --- |
| Panel DDI `s6e3hab` | id `0x001081`, resolution `1440x3200`, `dsi-hs-clk = <1157>` MHz, 4 data lanes, DSC 2 slices of `720x40` | `arch/arm64/boot/dts/exynos/exynos9830-display-lcd.dtsi:277-308` |
| Native display mode | `1440x3200` @ 60 Hz, `dsc_en = <1>`, `dsc_cnt = <2>`, slice `720x40` | `arch/arm64/boot/dts/samsung/exynos990-hubble_panel_display_modes.dtsi:20,37-56` |
| Other modes | `1440x3200` @48, `1080x2400` @120/96/60/48, `720x1600` @120/96/60/48 | `arch/arm64/boot/dts/samsung/exynos990-hubble_panel_display_modes.dtsi:22-33` |

---

## 7. Touchscreen and input, per model

The exact model code of the phone is still unknown, so the per-model facts are
kept in one place here.

### 7.1 Which models use which input file

| Model code | Phone | Input common file | Touch node |
| --- | --- | --- | --- |
| x1s | Galaxy S20 / S20 5G | `exynos9830-hubblex1_input_common.dtsi` (includes `exynos9830-hubble_input_common.dtsi`) | `touchscreen@48` on `hsi2c_15` |
| y2s | Galaxy S20+ | `exynos9830-hubbley2_input_common.dtsi` | `touchscreen@48` on `hsi2c_15` |
| z3s | Galaxy S20 Ultra | `exynos9830-hubblez3_input_common.dtsi` | `touchscreen@48` on `hsi2c_15` |
| r8s | Galaxy S20 FE | `exynos990-r8s_input_common.dtsi` | `touchscreen@20` (zinitix) **and** `tsp_stm touchscreen@49` (STM) on `hsi2c_15` |
| c1s | Galaxy Note 20 | `exynos9830-c1_input_common.dtsi` (includes `exynos9830-canvas_input_common.dtsi`) | `tsp_lsi touchscreen@48` on `hsi2c_15` + Wacom on `hsi2c_44` |
| c2s | Galaxy Note 20 Ultra | `exynos9830-c2_input_common.dtsi` (includes the canvas file) | `tsp_lsi touchscreen@48` on `hsi2c_15` + Wacom on `hsi2c_44` |
| win2 | Galaxy S10 5G | `exynos9830-win2_input_common.dtsi` | `touchscreen@49` (STM) on `hsi2c_15` **and** `touchscreen@48` (sec_ts) on `hsi2c_13` |

Model → input file mapping comes from the model overlays, e.g.
`arch/arm64/boot/dts/samsung/exynos990-x1s_common.dtsi:18`
(`#include "exynos9830-hubblex1_input_common.dtsi"`).

### 7.2 I2C bus

| Bus | Node | Base | Size | Interrupt | SCL / SDA pins | Source |
| --- | --- | --- | --- | --- | --- | --- |
| `hsi2c_15` (USI07) | `hsi2c@108D0000` | `0x108D0000` | `0x1000` | `INTREQ__USI07_I2C` = SPI 420 → INTID 452 | `gpp5-6` / `gpp5-7` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:845-863` |
| `hsi2c_13` (USI06) | `hsi2c@108B0000` | `0x108B0000` | `0x1000` | `INTREQ__USI06_I2C` | `gpp5-2` / `gpp5-3` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:806-823` |

`hsi2c_15` details: compatible `samsung,exynos5-hsi2c`,
`default-clk = <200000000>`, `samsung,usi-i2c-v2`,
clocks `DOUT_CLK_PERIC1_USI_I2C` + `GATE_PERIC1_TOP0_QCH_USI07_I2C`
(`arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:846-862`).

The touch I2C bus pin group `tspi2c_bus` (`gpp5-6`, `gpp5-7`, function 3) is in
`pinctrl_5` @ `0x10730000` (PERIC1) —
`arch/arm64/boot/dts/samsung/exynos9830-hubble_input_common.dtsi:80-89`.

### 7.3 Touch controllers

| Model | Node | Address | Compatible | IRQ GPIO | Firmware | Source |
| --- | --- | --- | --- | --- | --- | --- |
| x1s (base) | `touchscreen@48` | `0x48` | `sec,sec_ts` | `sec,irq_gpio = <&gpa1 0 0>` | `tsp_sec/y79a_x1.bin` | `arch/arm64/boot/dts/samsung/exynos9830-hubble_input_common.dtsi:99-119`; `arch/arm64/boot/dts/samsung/exynos9830-hubblex1_input_common.dtsi:15-19` |
| x1s (05/16/17/19/21) | `touchscreen@48` | `0x48` | `sec,sec_ts` | `&gpa1 0` **+** `sec,tsp-id_gpio = <&gpm21 0 0>` | `tsp_sec/y771_y2_ub.bin`, `tsp_sec/y79a_x1_ub.bin` | `arch/arm64/boot/dts/samsung/exynos990-x1s_eur_open_05.dts:111-127` |
| y2s (base) | `touchscreen@48` | `0x48` | `sec,sec_ts` | `&gpa1 0` | `tsp_sec/y79a_y2.bin` | `arch/arm64/boot/dts/samsung/exynos9830-hubbley2_input_common.dtsi:15-19` |
| y2s (05) | `touchscreen@48` | `0x48` | `sec,sec_ts` | `&gpa1 0` | `tsp_sec/y771_y2_ub.bin` | `arch/arm64/boot/dts/samsung/exynos990-y2s_eur_open_05.dts:145` |
| y2s (06/16) | `touchscreen@48` | `0x48` | `sec,sec_ts` | `&gpa1 0` + `sec,tsp-id_gpio = <&gpm21 0 0>` | `tsp_sec/y771_y2_ub.bin`, `tsp_sec/y79a_y2_ub.bin` | `arch/arm64/boot/dts/samsung/exynos990-y2s_eur_open_06.dts:126-139` |
| z3s (base) | `touchscreen@48` | `0x48` | `sec,sec_ts` | `&gpa1 0` | `tsp_sec/y79a_z3.bin` | `arch/arm64/boot/dts/samsung/exynos9830-hubblez3_input_common.dtsi:15-19` |
| z3s (16/17) | `touchscreen@48` | `0x48` | `sec,sec_ts` | `&gpa1 0` + `tsp_id` (`gpm21-0`) pinctrl | base firmware | `arch/arm64/boot/dts/samsung/exynos990-z3s_eur_open_16.dts:240-251` |
| r8s | `touchscreen@20` | `0x20` | `zinitix,zt_ts_device` | `zinitix,irq_gpio = <&gpa1 0 0>` | `tsp_zinitix/zt7650_r8.bin` | `arch/arm64/boot/dts/samsung/exynos990-r8s_input_common.dtsi:107-139` |
| r8s | `tsp_stm touchscreen@49` | `0x49` | `stm,fts_touch` | `stm,irq_gpio = <&gpa1 0 0>` | `tsp_stm/fts5cu56a_r8.bin` | `arch/arm64/boot/dts/samsung/exynos990-r8s_input_common.dtsi:141-166` |
| c1s (base) | `tsp_lsi touchscreen@48` | `0x48` | `sec,sec_ts` | `sec,irq_gpio = <&gpa1 0 0>` | `tsp_sec/y771_c1.bin` | `arch/arm64/boot/dts/samsung/exynos9830-canvas_input_common.dtsi:118-139`; `arch/arm64/boot/dts/samsung/exynos9830-c1_input_common.dtsi:14-18` |
| c1s (21) | `tsp_lsi touchscreen@48` | `0x48` | `sec,sec_ts` | as above | `tsp_sec/y771_c1_pv1.bin` | `arch/arm64/boot/dts/samsung/exynos990-c1s_eur_open_21.dts:354` |
| c1s (openx 19) | `tsp_lsi touchscreen@48` | `0x48` | `sec,sec_ts` | as above | `tsp_sec/y79a_c1_old.bin`, `""` | `arch/arm64/boot/dts/samsung/exynos990-c1s_eur_openx_19.dts:280` |
| c2s (base) | `tsp_lsi touchscreen@48` | `0x48` | `sec,sec_ts` | `sec,irq_gpio = <&gpa1 0 0>` | **none** (the firmware line is commented out) | `arch/arm64/boot/dts/samsung/exynos9830-c2_input_common.dtsi:14-21` |
| c2s (21-24) | `tsp_lsi touchscreen@48` | `0x48` | `sec,sec_ts` | as above | `tsp_sec/y79a_c2_4layer.bin` | `arch/arm64/boot/dts/samsung/exynos990-c2s_eur_open_21.dts:214` (also 22/23/24) |
| c2s (openx 16-19) | `tsp_lsi touchscreen@48` | `0x48` | `sec,sec_ts` | as above | `tsp_sec/y79a_c2_6layer.bin` ×2, `tsp_sec/y79a_c2_4layer.bin` ×2 | `arch/arm64/boot/dts/samsung/exynos990-c2s_eur_openx_16.dts:353` (also 17/18/19) |
| win2 | `touchscreen@49` | `0x49` | `stm,fts_touch` | `stm,irq_gpio = <&gpa1 0 0>` | **none** (commented out: `tsp_stm/fts9cu80f_winner.fw`) | `arch/arm64/boot/dts/samsung/exynos9830-win2_input_common.dtsi:106-125` |
| win2 (sub-touch) | `touchscreen@48` on `hsi2c_13` | `0x48` | `sec,sec_ts` | `sec,irq_gpio = <&gpa3 7 0>` | **none** (commented out: `tsp_sec/y761_winner.bin`) | `arch/arm64/boot/dts/samsung/exynos9830-win2_input_common.dtsi:139-161` |

`sec,tsp-id_gpio` appears only on the hubble family (x1s, y2s, z3s), always as
`&gpm21 0 0` — for example `arch/arm64/boot/dts/samsung/exynos990-x1s_eur_open_05.dts:124`.
The `tsp_id` pin group is `gpm21-0`, function 0, in `pinctrl_1` @ `0x15C30000`
(CMGP) — `arch/arm64/boot/dts/samsung/exynos990-x1s_eur_open_05.dts:111-118`.

Common `sec_ts` node properties (hubble family):
`sec,irq_type = <8200>`, `sec,max_coords = <4096 4096>`,
`sec,area-size = <133 266 341>`, regulators `tsp_io`/`tsp_avdd`,
`support_fod`, `support_ear_detect_mode` —
`arch/arm64/boot/dts/samsung/exynos9830-hubble_input_common.dtsi:99-119`.

The r8s zinitix node additionally gives the panel geometry as
`zinitix,x_resolution = <1080>`, `zinitix,y_resolution = <2400>` and the IC name
`zinitix,chip_name = "ZT7650"` —
`arch/arm64/boot/dts/samsung/exynos990-r8s_input_common.dtsi:117-118,123`.

Wacom (c1s/c2s) is on `hsi2c_44`, address `0x56`, compatible `wacom,w90xx`,
IRQ `gpa2-2`, PDCT `gpa3-2`, FWE `gpm8-0`, `wacom,ic_type = <9020>` —
`arch/arm64/boot/dts/samsung/exynos9830-canvas_input_common.dtsi:165-199`.

> **UNVERIFIED.** The touch IC actually fitted to the phone (the DT lists
> alternatives per firmware revision), its I2C address and its IRQ line must
> be confirmed on the phone: read the live DT
> (`/sys/firmware/fdt`), or probe `hsi2c_15` at `0x48`/`0x49`/`0x20` and
> watch for an ACK.

---

## 8. Keys

The same three keys appear in every input-common file:

| Key | `linux,code` | GPIO | Wakeup | Source |
| --- | --- | --- | --- | --- |
| `KEY_VOLUMEDOWN` | 114 | `&gpa0 4 0xf` | no | `arch/arm64/boot/dts/samsung/exynos9830-hubble_input_common.dtsi:23-27` |
| `KEY_VOLUMEUP` | 115 | `&gpa0 3 0xf` | no | `arch/arm64/boot/dts/samsung/exynos9830-hubble_input_common.dtsi:28-32` |
| `KEY_POWER` | 116 | `&gpa2 4 0xf` | `gpio-key,wakeup = <1>` | `arch/arm64/boot/dts/samsung/exynos9830-hubble_input_common.dtsi:33-38` |

Node: `gpio_keys`, compatible `gpio-keys`, `status = "okay"`, pinctrl
`key_voldown` / `key_volup` / `key_power` —
`arch/arm64/boot/dts/samsung/exynos9830-hubble_input_common.dtsi:17-39`.

Pin groups (all function `0xf`, no pull, no drive) live in `pinctrl_0`
@ `0x15850000` — `arch/arm64/boot/dts/samsung/exynos9830-hubble_input_common.dtsi:44-64`.

The same three-key set is repeated verbatim in the r8s, canvas (c1s/c2s) and
win2 input files:

- `arch/arm64/boot/dts/samsung/exynos990-r8s_input_common.dtsi:17-39`
- `arch/arm64/boot/dts/samsung/exynos9830-canvas_input_common.dtsi:21-43`
- `arch/arm64/boot/dts/samsung/exynos9830-win2_input_common.dtsi:17-39`

`gpa0`/`gpa1`/`gpa2` belong to `pinctrl_0` (ALIVE, `0x15850000`) —
`arch/arm64/boot/dts/exynos/exynos9830.dts:1211-1213`.

---

## 9. Memory map and reserved-memory carve-outs

### 9.1 Peripheral address map (summary)

The blocks relevant to the port, in address order. Sizes are the `reg` sizes
where the DT gives one; the CMU blocks are 0x8000 each
(`drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:5-48`).

| Block | Base | Size | Source |
| --- | --- | --- | --- |
| Chip ID | `0x10000000` | `0x100` | `arch/arm64/boot/dts/exynos/exynos9830.dts:78-81` |
| MCT (Samsung timer) | `0x10040000` | `0x800` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1178-1180` |
| GICD (GIC distributor) | `0x10101000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1139` |
| GICC (GIC CPU interface) | `0x10102000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1140` |
| VGIC control | `0x10104000` | `0x2000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1141` |
| GIC VCPU interface | `0x10106000` | `0x2000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1142` |
| CMU_PERIC0 | `0x10400000` | `0x8000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:42` |
| `usi_uart_dbg` (USI mux) | `0x10421000` | `0x4` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:532` |
| GPIO controller PERIC0 | `0x10430000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1277-1279` |
| **Debug UART** (`serial_0`) | `0x10540000` | `0x100` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1921` |
| CMU_PERIC1 | `0x10700000` | `0x8000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:43` |
| GPIO controller PERIC1 | `0x10730000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1284-1286` |
| `hsi2c_13` (USI06) | `0x108B0000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:810` |
| `hsi2c_15` (USI07, touch bus) | `0x108D0000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:849` |
| DisplayPort controller | `0x10AB0000` | `0xFFFF` (+ `0x10AE0000`, `0x20CC`) | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:295-296` |
| CMU_HSI1 | `0x13000000` | `0x8000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:32` |
| GPIO controller HSI1 | `0x13040000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1263-1265` |
| UFS HCI | `0x13100000` | `0x200` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1530` |
| UFS vendor block | `0x13101100` | `0x200` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1531` |
| UFS PHY | `0x13104000` | `0x3000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1588` |
| UFS cport | `0x13108000` | `0x804` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1605-1606` |
| UFS UNIPRO | `0x13180000` | `0x8000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1532` |
| UFS protector | `0x132A0000` | `0x100` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1533` |
| GPIO controller HSI2 | `0x13C30000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1270-1272` |
| GPIO controller VTS | `0x15580000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1291-1293` |
| GPIO controller ALIVE | `0x15850000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1211-1213` |
| GPIO controller CMGP | `0x15C30000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1238-1240` |
| CMU_AUD | `0x18c00000` | `0x8000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:6` |
| CMU_DPU | `0x19000000` | `0x8000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:26` |
| DISP_SS | `0x19021000` | `0x10` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:256-259` |
| DECON_F (panel) | `0x19050000` | `0x10000` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:306-309` |
| DECON_S (DisplayPort) | `0x19060000` | `0x10000` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:360-363` |
| DECON_T (DisplayPort) | `0x19070000` | `0x10000` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:399-402` |
| DPP host 0 (`GF0_L0`) | `0x19091000` | `0x1000` (+ `0x19041000`, `0x19090000`) | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:48-52` |
| DSIM0 | `0x190C0000` | `0x300` (+ `0x190E0100`, `0x190E0000`) | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:272` |
| MIPI PHY (DSIM0) | `0x190E0000` | `0x100` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:261-267` |
| CMU_TOP (clock controller) | `0x1a330000` | `0x8000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1162` |
| CMU_CPUCL0 | `0x1d020000` | `0x8000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:8` |

This is a summary of blocks already cited in detail above; it is not an
exhaustive address map of the SoC. DRAM is not in it — see §9.2.

### 9.2 There is no `memory` node for this SoC

The 9830 tree contains **no** `memory`/`memory@` node. (The older Exynos 9820
tree does: `arch/arm64/boot/dts/exynos/exynos9820.dts:73-86`.) Consequently the
DRAM layout is supplied by the bootloader, not by the DT.

> **MEASURED (2026-10-22).** `/proc/iomem` under TWRP shows the high bank
> starting at `0x880000000`, with `880000000-88acfffff : reserved` at its
> start. That reservation is `sec_rdx_bootdev`, the RAM-dump carveout, whose
> DT node is `<0x8 0x80000000 0x0AD00000>`
> (`arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:65-70`); the next reserved
> range is `0x9f0c00000`, so `0x890000000..0x8afffffff` is free RAM.
> XNU's window therefore runs from `0x890000000`, not from the start of the
> bank: placing it at `0x880000000` put the loader's 512 MiB zeroing on top
> of the RAM-dump carveout on every boot.
>
> Note that the 9820 tree's memory node is a *different SoC*; it is cited
> here only to show what the 9830 tree does **not** contain.

### 9.3 Carve-outs from the 9830 reserved-memory tree

`arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:19-237`

| Region | Base | Size | Source |
| --- | --- | --- | --- |
| `el3mon_reserve` | `0x0BAB00000` | `0x5500000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:47-50` |
| `sec_debug_magic` | `0x080000000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:60-63` |
| `sec_rdx_bootdev` | `0x80000000` | `0x0AD00000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:65-69` |
| `debug_snapshot` (all sub-regions) | derived from `DSS_START_ADDR` | see below | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:71-131` |
| `seclog_mem` | `0x0C3000000` | `8 × 0x10000` = `0x80000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:133-136`; `include/dt-bindings/soc/samsung/exynos9830-seclog.h:23-25` |
| `s2mpu_table` | `0x0C1800000` | `0x300000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:138-141`; `include/dt-bindings/soc/samsung/exynos9830-s2mpu.h:14-15` |
| `abox_dbg` | allocated in `0x0C0000000` + `0x20000000` | `0x5600000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:143-147` |
| `abox_slog` | allocated in `0x0C0000000` + `0x20000000` | `0x2800000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:149-153` |
| `abox_rmem` | `0x0F8000000` | `0x2A00000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:155-162` |
| `abox_rdma_compr` | `0x0F7FB0000` | `0x50000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:164-167` |
| `vts_rmem` | allocated in `0x0C5000000` + `0x15000000` | `0x200000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:169-173` |
| `abox_pci_rmem` | `0x0F4200000` | `0x800000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:175-178` |
| `dp_ado_rmem` | allocated in `0x0C0000000` + `0x20000000` | `0x200000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:180-184` |
| `camera_rmem` | `CAMERA_LOG_BUF_BASE` = `0x0ED210000` | `0x100000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:186-189`; `include/dt-bindings/camera/exynos_is_dt.h:342` |
| `secure_camera` (ion heap) | `0x096000000` | `0x3200000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:191-198` |
| `camera` (ion heap) | allocated `0xA00000000`/`0x900000000` | `0x22F00000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:200-208` |
| `teegris_rmem` | `0x0ED310000` | `0x2000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:210-213` |
| `cp_rmem` (modem) | `0x0F0000000` | `0x800000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:215-219` |
| `cp_msi_rmem` (modem MSI) | `0x0F6200000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:221-225` |
| `cp_rmem_2` (modem) | `0x0D0000000` | `0x4000000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:227-231` |
| `wifi_msi_rmem` | `0x0F7200000` | `0x1000` | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:233-236` |

Two ION heaps also exist without a fixed base: `ion_video_stream`
(`0x4800000`, alloc-ranges `0x40000000` + `0xB0000000`,
`arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:24-34`) and `ion_crypto`
(`0x1800000`, `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:36-45`).

### 9.4 debug-snapshot layout

`include/dt-bindings/soc/samsung/debug-snapshot-def.h`

| Region | Address | Size | Source |
| --- | --- | --- | --- |
| Start address | `0xFD900000` | — | `include/dt-bindings/soc/samsung/debug-snapshot-def.h:54` |
| `header` | `0xFD900000` | `64 KiB` | `:55,84` |
| `log_kernel` | `0xFD910000` | 2 MiB | `:56,85` |
| `log_platform` | `0xFDB10000` | 4 MiB | `:57,86` |
| `log_sfr` | `0xFDF10000` | 1 MiB | `:58,87` |
| `log_s2d` | `0xEB310000` | 21 MiB | `:59,96` |
| `log_pstore` | `0xEB308000` | 32 KiB | `:66,97` |
| `log_arrdumpreset` | `0xFE010000` | 10 MiB | `:60,88` |
| `log_arrdumppanic` | `0xEC810000` | 10 MiB | `:61,98` |
| `log_etm` | `0xFEA10000` | 1 MiB | `:62,90` |
| `log_bcm` | `0xFEB10000` | 4 MiB | `:63,91` |
| `log_llc` | `0xFEF10000` | 4 MiB | `:64,92` |
| `log_dbgc` | `0xFF310000` | 1 MiB − 64 KiB | `:65,93` |
| `log_kevents` | `0xFF400000` | 8 MiB | `:67,94` |
| `log_fatal` | `0xFFC00000` | 4 MiB | `:68,95` |

The addresses above are **derived** from `DSS_START_ADDR` plus the offset
macros in the same header (`:70-82`); the header defines the sizes and
offsets, and `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:71-131` only
wires the resulting base/size pairs into the `debug_snapshot` node.

### 9.5 Other fixed memory regions

| Region | Address / size | Source |
| --- | --- | --- |
| `ect` parameter area | `parameter_address = <0x90000000>`, `parameter_size = <0x4B000>` | `arch/arm64/boot/dts/exynos/exynos9830.dts:65-68` |
| Initrd in `/chosen` | `linux,initrd-start = <0x84000000>`, `linux,initrd-end = <0x841FFFFF>` | `arch/arm64/boot/dts/exynos/exynos9830.dts:73-74` |

> **UNVERIFIED.** `ramoops`. There is **no** `ramoops` node anywhere in the
> 9830 DT tree (the S8 port read one at `0x92000000`, 2 MiB, console at
> `0x92080000` — `DREAMLTE_PROGRESS.md:26-28`). On the 9830 the nearest
> equivalent is the `debug_snapshot` `log_pstore` region at `0xEB308000`
> (32 KiB), which is a Samsung-specific mechanism, not Linux `ramoops`.
> Confirm on the phone by looking for a `ramoops` node in the live DT and
> checking `/sys/fs/pstore`.

---

## 10. Clock controller

```
clock: clock-controller@0x1a330000 {
        compatible = "samsung,exynos9830-clock";
        reg = <0x0 0x1a330000 0x8000>,
              <0x0 0x1d02083c 0x100>,
              <0x0 0x1840083c 0x100>,
              <0x0 0x1d130000 0x100>,
              <0x0 0x1d200000 0x100>;
        #clock-cells = <1>;
        acpm-ipc-channel = <0>;
};
```

— `arch/arm64/boot/dts/exynos/exynos9830.dts:1160-1169`

| Fact | Value | Source |
| --- | --- | --- |
| Base | `0x1a330000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1162` |
| Size | `0x8000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1162` |
| Extra ranges | `0x1d02083c`, `0x1840083c`, `0x1d130000`, `0x1d200000` (0x100 each) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1163-1166` |
| Compatible | `samsung,exynos9830-clock` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1161` |

There is also a CMU "EWF" node at the same base with size `0x1000`
(`arch/arm64/boot/dts/exynos/exynos9830.dts:1171-1176`).

### 10.1 CMU SFR blocks

All CMU blocks are 0x8000 bytes
(`drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:5-48`):

| Block | Base | Source |
| --- | --- | --- |
| `CMU_TOP` | `0x1a330000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:7` |
| `CMU_PERIC0` | `0x10400000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:42` |
| `CMU_PERIC1` | `0x10700000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:43` |
| `CMU_HSI1` | `0x13000000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:32` |
| `CMU_DPU` | `0x19000000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:26` |
| `CMU_CPUCL0` | `0x1d020000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:8` |
| `CMU_AUD` | `0x18c00000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:6` |

### 10.2 Gate register layout

All CMU gate registers use the same bit layout:

| Field | Bit | Source |
| --- | --- | --- |
| `CG_VAL` (gate state) | 21 | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:5132` |
| `MANUAL` (manual vs automatic gating) | 20 | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:5133` |
| `ENABLE_AUTOMATIC_CLKGATING` | 28 | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:5134` |

### 10.3 Registers needed for the port

| Purpose | Register | Offset | Block | Source |
| --- | --- | --- | --- | --- |
| DPU bus gate | `CLK_CON_GAT_GATE_CLKCMU_DPU_BUS` | `0x2064` | CMU_TOP | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:837` |
| DPU gate | `CLK_CON_GAT_GATE_CLKCMU_DPU` | `0x2060` | CMU_TOP | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:866` |
| UFS gate | `CLK_CON_GAT_GATE_CLKCMU_HSI1_UFS_EMBD` | `0x209c` | CMU_TOP | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:834` |
| DECON aclk | `..._ACLK_DECON` | `0x200c` | CMU_DPU | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1235` |
| DPU DMA aclk | `..._ACLK_DMA` | `0x2010` | CMU_DPU | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1236` |
| DPP aclk | `..._ACLK_DPP` | `0x2014` | CMU_DPU | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1237` |
| DPU write-back mux aclk | `..._ACLK_DPU_WB_MUX` | `0x2018` | CMU_DPU | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1233` |
| UART DBG divider | `CLK_CON_DIV_DIV_CLK_PERIC0_UART_DBG` | `0x1800` | CMU_PERIC0 | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:519` |
| UART DBG clock mux | `PLL_CON0_MUX_CLKCMU_PERIC0_UART_DBG` / `PLL_CON1_...` | `0x610` / `0x614` | CMU_PERIC0 | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:329-330` |
| UFS aclk | `..._ACLK_UFS_EMBD` | `0x2080` | CMU_HSI1 | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:1361` |

The clock ID space is `include/dt-bindings/clock/exynos9830.h`; e.g.
`GATE_DPU_DPU` = `CLK_DPU_BASE + 1` (`:414`), `GATE_UFS_EMBD` = `CLK_HSI1_BASE + 23`
(`:531`), `UFS_EMBD` = `CLK_HSI1_BASE + 26` (`:534`),
`GATE_PERIC0_TOP0_QCH_UART_DBG` = `CLK_PERIC0_BASE + 4` (`:66`),
`UART_DBG` = `CLK_PERIC0_BASE + 32` (`:92`).

---

## 11. PSCI

```
psci {
        compatible = "arm,psci-1.0";
        method = "smc";
};
```

— `arch/arm64/boot/dts/exynos/exynos9830.dts:1459-1462`

| Fact | Value | Source |
| --- | --- | --- |
| Compatible | `arm,psci-1.0` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1460` |
| Conduit method | `smc` (not `hvc`) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1461` |
| Driver reads `method` | `of_property_read_string(np, "method", &method)` | `drivers/firmware/psci.c:253` |
| `smc` selected | `!strcmp("smc", method)` | `drivers/firmware/psci.c:260` |
| SMC invocation | `arm_smccc_smc(function_id, arg0, arg1, arg2, ...)` | `drivers/firmware/psci.c:147` |

Because the DT declares `arm,psci-1.0`, the standard function IDs apply
(`PSCI_0_2_FN_*`), i.e. `cpu_on` = `0xC4000003`. The vendor `cpupm` node adds
its own power-state indices on top (`psci-index = <128>` for cluster states,
`<256>` for system idle) — `arch/arm64/boot/dts/exynos/exynos9830.dts:1471,1505`.

All CPUs use `enable-method = "psci"` (§1.2), so secondary cores are started
by PSCI `cpu_on` with the MPIDR as the target.

> **UNVERIFIED.** That the bootloader/EL3 actually implements PSCI 1.0 with
> the standard IDs on this device (the S8 boot CPU answered `cpu_on` but no
> secondary core ran — `DREAMLTE_PROGRESS.md:19-20`). Start with one CPU
> and test `cpu_on` on the phone.

---

## 12. Boot flow

### 12.1 What the kernel expects

The arm64 kernel entry contract is documented in the tree:

> The requirements are:
>   MMU = off, D-cache = off, I-cache = on or off,
>   x0 = physical address to the FDT blob.

— `arch/arm64/kernel/head.S:62-64`

`x0` is moved to `x21` by `preserve_boot_args` (`mov x21, x0  // x21=FDT`,
`arch/arm64/kernel/head.S:140`) and put back into `x0` when the FDT is handed
to the kernel (`mov x0, x21  // pass FDT address in x0`,
`arch/arm64/kernel/head.S:453`).

This is the **generic** arm64 contract. What the Samsung bootloader actually
does is not described in this tree.

### 12.2 `samsung,boot` / DTBH

Searching the whole tree for a `samsung,boot` property or for `DTBH` returns
**nothing** — there is no such property, no DTBH parsing and no DTBH magic in
the 9830 kernel tree. The tree also contains no code that reads a Samsung
bootloader header.

> **UNVERIFIED.** Whether sboot on this phone passes a DTB in `x0`, passes a
> DTBH pointer instead, or passes nothing at all. The S8 measurement was
> "x0 = DTB (0x8A000000)" (`DREAMLTE_PROGRESS.md:12`), and
> `shim/exynos990/board.h:12-20` assumes no DTB. Settle it on the phone by
> reading `x0` at the loader's entry point and dumping the pointed-to
> memory.

### 12.3 `/chosen`

```
chosen: chosen {
        bootargs = "androidboot.hardware=exynos990 bcm_setup=0xffffff80f8e00000 firmware_class.path=/vendor/firmware reserve-fimc=0xffffff90f9fe0000 nohugeiomap epx_activate=true fpsimd_check_context=y rcupdate.rcu_expedited=1 corememsize=8G cgroup.memory=nokmem loop.max_part=7";
        linux,initrd-start = <0x84000000>;
        linux,initrd-end = <0x841FFFFF>;
};
```

— `arch/arm64/boot/dts/exynos/exynos9830.dts:70-75`

| Fact | Value | Source |
| --- | --- | --- |
| `bootargs` | see above; note `androidboot.hardware=exynos990` and `corememsize=8G` | `arch/arm64/boot/dts/exynos/exynos9830.dts:72` |
| `linux,initrd-start` | `0x84000000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:73` |
| `linux,initrd-end` | `0x841FFFFF` | `arch/arm64/boot/dts/exynos/exynos9830.dts:74` |

### 12.4 `bootargs_ext` (model overlays)

`bootargs_ext` is a Samsung/Exynos bootloader extension that appends to
`bootargs`. It appears only in the per-model overlays:

| Value | Models | Source |
| --- | --- | --- |
| `root=/dev/ram0 androidboot.boot_devices=13100000.ufs` | x1s, y2s, z3s, x1slte(00 only, with `sec_debug.cp=0`), win2… | e.g. `arch/arm64/boot/dts/samsung/exynos990-x1s_eur_open_05.dts:92-94` |
| `root=/dev/ram0 androidboot.boot_devices=13100000.ufs sec_debug.cp=0` | c1s, c2s, r8s, win2, y2slte, x1slte | e.g. `arch/arm64/boot/dts/samsung/exynos990-c1s_eur_open_21.dts:153` |

`androidboot.boot_devices=13100000.ufs` confirms the UFS HCI base as
`0x13100000` (§5).

### 12.5 Board id / revision

| Fact | Value | Source |
| --- | --- | --- |
| `acpm_ipc` board id / rev | `BOARD_ID = 0x10`, `BOARD_REV = 0x0` | `arch/arm64/boot/dts/exynos/exynos9830-universal9830.dts:19-20,41-43` |
| `dtb-hw_rev` / `_end` | `<0>` / `<255>` | `arch/arm64/boot/dts/exynos/exynos9830.dts:45-46` |
| Model overlay `compatible` | `"samsung,X1S EUR OPEN 05", "samsung,EXYNOS990"` | `arch/arm64/boot/dts/samsung/exynos990-x1s_eur_open_05.dts:27` |

> **UNVERIFIED.** The phone's exact model code and therefore its model
> overlay (`dtbo-hw_rev`), its panel and its touch firmware.

---

## 13. Differences from Exynos 8895 (the S8 port in this repo)

The 8895 column is taken from the S8 port in this repository
(`DREAMLTE_PROGRESS.md`, `shim/dreamlte/`), i.e. it is the port's current
assumption, not an independent 8895 hardware survey.

| Topic | Exynos 8895 (S8 port in this repo) | Exynos 990 / 9830 (this document) | Source (9830) |
| --- | --- | --- | --- |
| GIC distributor | `0x10201000` (`DREAMLTE_PROGRESS.md:18`) | `0x10101000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1139` |
| GIC CPU interface | `0x10202000` (`DREAMLTE_PROGRESS.md:18`) | `0x10102000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1140` |
| GIC virtual frames | not used by the port | `0x10104000` (VGIC ctrl) / `0x10106000` (VCPU) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1141-1142` |
| GIC type | GIC-400, GICv2 | GIC-400 class, GICv2 (`arm,cortex-a15-gic`) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1135`; `drivers/irqchip/irq-gic.c:1514` |
| Virtual timer INTID | 27 (`DREAMLTE_PROGRESS.md:18`) | 43 by the GICv2 rule (`32 + PPI 11`) — **UNVERIFIED on hardware** | `arch/arm64/boot/dts/exynos/exynos9830.dts:1152-1153` |
| Timer frequency | 26 MHz in DT, `CNTFRQ_EL0 = 0` on the boot CPU (`DREAMLTE_PROGRESS.md:13`) | 26 MHz in DT | `arch/arm64/boot/dts/exynos/exynos9830.dts:1156` |
| CPU topology | 4× Cortex-A53, boot CPU MPIDR `0x100` (`DREAMLTE_PROGRESS.md:14`) | 4× A55 (`0x000`-`0x003`) + 2× `arm,enyo` (`0x004`,`0x005`) + 2× M5 (`0x100`,`0x101`); **boot CPU is MPIDR `0x000`** | `arch/arm64/boot/dts/exynos/exynos9830.dts:147-274` |
| PSCI | 0.1 IDs, `cpu_on 0xc4000003` (`DREAMLTE_PROGRESS.md:19`) | `arm,psci-1.0`, `method = "smc"` — same standard IDs | `arch/arm64/boot/dts/exynos/exynos9830.dts:1459-1462` |
| Debug UART | (S8 console; the A5XELTE base was `0x10540000` for the same family) | S3C24XX at `0x10540000`; the `usi_uart_dbg` node at `0x10421000` is only the 4-byte USI mux | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:530-535,1918-1933` |
| Storage | UFS (`DREAMLTE_PROGRESS.md:29`) | UFS HCI `0x13100000`, vendor block `0x13101100`, UNIPRO `0x13180000`, protector `0x132A0000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1529-1533` |
| DECON base / trigger | trigger write `0x1281` → `0x12860070` (`shim/dreamlte/loader.c:31-32,298`) | DECON block `0x19050000`; `trig_mode = <0>` (hw trigger), `psr_mode = <2>` (MIPI command). **`0x12860070` does not exist here** | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:306-358` |
| Framebuffer | `0xcc000000`, 1440×2960 (`DREAMLTE_PROGRESS.md:21-23`) | no framebuffer address in the tree; panel 1440×3200 via `s6e3hab` | `arch/arm64/boot/dts/exynos/exynos9830-display-lcd.dtsi:277-308` |
| RAM layout | `80000000-bc7fffff`, `c0000000-ffffffff`, `880000000-8ffffffff` (`DREAMLTE_PROGRESS.md:24`) | **no `memory` node in the 9830 tree**; bootloader-supplied | §9.2 |
| Carve-outs | ramoops `0x92000000` 2 MiB, console `0x92080000` (`DREAMLTE_PROGRESS.md:26-28`) | no `ramoops` node; `debug_snapshot` from `0xFD900000`, `log_pstore` `0xEB308000` | §9.4 |
| Clock controller | (S8: not listed) | `0x1a330000`, size `0x8000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1160-1162` |
| Boot CPU | MPIDR `0x100` (`DREAMLTE_PROGRESS.md:14`) | MPIDR `0x000` — CPU0 is a little core | `arch/arm64/boot/dts/exynos/exynos9830.dts:147-162` |

---

## 14. Open questions for the phone

Each item is something the kernel tree cannot answer. The first three block
the next milestone.

1. **Exact model code.** Which of x1s / y2s / z3s / r8s / c1s / c2s / win2 is
   the phone? It decides the panel, the touch controller, the touch firmware
   and the keys. Read `ro.boot.hardware` and the DT `compatible` string, or
   look at the About screen / IMEI model.
2. **Debug UART address.** `0x10421000` (USI mux, 4 bytes) or `0x10540000`
   (UART, 0x100 bytes)? Which pads are the debug UART on this board? Scope
   the TX pad while the bootloader prints.
3. **Does sboot pass a DTB in `x0`?** If yes, dump it and compare it with
   the DT in this tree; if not, the loader must supply its own device tree.
4. **DRAM layout.** `/proc/iomem` (or the DT the bootloader passes): where
   does the low bank end, where does the high bank start, and which carve-outs
   are live? The 9830 tree has no `memory` node.
5. **Timer.** What is `CNTFRQ_EL0` on the boot CPU, and which INTID does the
   virtual timer actually use? (43 by the GICv2 rule, but the A5XELTE/S8
   ports saw something else.)
6. **DECON.** The 9830 DECON register map: window/block registers, the
   framebuffer address, and whether an "unmask trigger" write is needed at
   all (`trig_mode = <0>` says hardware trigger). Dump DECON before and after
   the bootloader hands over.
7. **Panel.** Which DDI is fitted (`s6e3hab` 1440×3200, `s6e3ha8` 1440×2960,
   `s6e3ha9` 1440×3040, …) and does the live DT agree with the model
   overlay's native mode?
8. **Touch IC.** Which controller is on the board and at which address —
   `sec_ts` at `0x48`, STM at `0x49`, zinitix at `0x20`? Does it need the
   `sec,tsp-id_gpio` (`gpm21-0`) handshake?
9. **UFS.** Capacity, block size, LUN layout and the descriptor; and whether
   the Exynos vendor block at `0x13101100` differs from the 8895's.
10. **PSCI.** Does `cpu_on` actually start a secondary core on this SoC? (It
    did not on the S8.)
11. **Reserved memory on the live device.** Which of the §9.3 carve-outs are
    really reserved, and where does the kernel's `reserved-memory` view sit
    relative to the EL3 monitor at `0x0BAB00000`?
12. **Middle-core type.** What is `arm,enyo`? The tree only names `arm,ananke`
    (A55) and `arm,lion` (M5) — read `MIDR_EL1` on a middle core (§1.2).

---

## 15. Fact → value → source

| Fact | Value | Source (path relative to the kernel tree root) |
| --- | --- | --- |
| SoC compatible | `samsung,armv8`, `samsung,exynos990` | `arch/arm64/boot/dts/exynos/exynos9830.dts:44` |
| SoC codename | exynos9830 / universal9830 | `build.config.universal9830:7`; `arch/arm64/boot/dts/exynos/exynos9830-universal9830.dts:33` |
| Chip ID base | `0x10000000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:78-81` |
| Little cores | 4× `arm,ananke` (Cortex-A55), MPIDR `0x0000`-`0x0003` | `arch/arm64/boot/dts/exynos/exynos9830.dts:147-210`; `arch/arm64/include/asm/cputype.h:83,90` |
| Middle cores | 2× `arm,enyo` (core type not named in the tree), MPIDR `0x0004`, `0x0005` | `arch/arm64/boot/dts/exynos/exynos9830.dts:211-242` |
| Prime cores | 2× `arm,lion` (Samsung M5), MPIDR `0x0100`, `0x0101` | `arch/arm64/boot/dts/exynos/exynos9830.dts:243-274`; `arch/arm64/include/asm/cputype.h:88,92` |
| Cluster map | 0-3 / 4-5 / 6-7 | `arch/arm64/boot/dts/exynos/exynos9830.dts:112-145` |
| CPU enable method | `psci` | `arch/arm64/boot/dts/exynos/exynos9830.dts:151` |
| GIC compatible | `arm,cortex-a15-gic`, `arm,cortex-a9-gic` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1135` |
| GIC driver match | `IRQCHIP_DECLARE(cortex_a15_gic, ...)` | `drivers/irqchip/irq-gic.c:1514` |
| GICD | `0x10101000` (0x1000) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1139` |
| GICC | `0x10102000` (0x1000) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1140` |
| VGIC control | `0x10104000` (0x2000) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1141` |
| VCPU | `0x10106000` (0x2000) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1142` |
| VGIC maintenance IRQ | PPI 9 to CPUs 0-3 | `arch/arm64/boot/dts/exynos/exynos9830.dts:1143` |
| Timer compatible | `arm,armv8-timer` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1147` |
| Timer PPIs | 13 (secure), 14 (non-secure), 11 (virtual), 10 (hypervisor) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1148-1155` |
| Timer PPI order rule | secure, non-secure, virtual, hypervisor | `Documentation/devicetree/bindings/timer/arm,arch_timer.txt:17-18` |
| Virtual timer INTID | 43 (32 + PPI 11) — UNVERIFIED on hardware | derived; `arch/arm64/boot/dts/exynos/exynos9830.dts:1152-1153` |
| Physical (non-secure) timer INTID | 46 (32 + PPI 14) | derived; `arch/arm64/boot/dts/exynos/exynos9830.dts:1150-1151` |
| Timer frequency | 26 000 000 Hz | `arch/arm64/boot/dts/exynos/exynos9830.dts:1156` |
| Clocksource-only timer | `use-clocksource-only` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1157` |
| EL0 counters | `CNTPCT_EL0`, `CNTVCT_EL0` | `drivers/clocksource/arm_arch_timer.c:306-320` |
| MCT block | `0x10040000` (0x800) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1178-1180` |
| USI mux node | `usi@10421000`, size `0x4`, `samsung,exynos-usi-v2` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:530-535` |
| Debug UART node | `uart@10540000`, size `0x100`, `samsung,exynos-uart` | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1918-1933` |
| Debug UART IRQ | `INTREQ__DBG_UART` = 391 | `include/dt-bindings/interrupt-controller/exynos9830.h:409` |
| Debug UART baud / word | 115200 / 8 | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:1929-1930` |
| Debug UART pins | `gpp4-6`, `gpp4-7`, func 3 | `arch/arm64/boot/dts/exynos/exynos9830-pinctrl.dtsi:912-917` |
| UART register layout | ULCON `0x00` … USI_OPTION `0xC8` | `include/linux/serial_s3c.h:19-35` |
| `UTRSTAT` bits | RXDR 0, TXFE 1, TXE 2, TIMEOUT 3 | `include/linux/serial_s3c.h:181-184` |
| USI reset / clock handshake | `USI_CON` bit 0, `USI_OPTION` bits 1-2 | `include/linux/serial_s3c.h:37-40`; `drivers/tty/serial/samsung.c:1645-1664` |
| UFS HCI | `0x13100000` (0x200) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1530` |
| UFS vendor block | `0x13101100` (0x200) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1531` |
| UFS UNIPRO | `0x13180000` (0x8000) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1532` |
| UFS protector | `0x132A0000` (0x100) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1533` |
| UFS IRQ | `INTREQ__UFS_EMBD` = 321 | `arch/arm64/boot/dts/exynos/exynos9830.dts:1534` |
| UFS clocks | `GATE_UFS_EMBD`, `UFS_EMBD` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1537-1547` |
| UFS pinctrl | `ufs_rst_n` gpf2-1, `ufs_refclk_out` gpf2-0, `xboot_ldo` gpg1-0 | `arch/arm64/boot/dts/exynos/exynos9830-pinctrl.dtsi:711-725,2223-2227` |
| UFS PHY | `0x13104000` (0x3000) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1588` |
| UFS SYSREG | `0x13020700` (0x4) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1598` |
| UFS cport | `0x13108000` (0x804) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1605-1606` |
| DECON_F | `0x19050000` (0x10000) | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:306-309` |
| DECON_S | `0x19060000` (0x10000) | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:360-363` |
| DECON_T | `0x19070000` (0x10000) | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:399-402` |
| DECON chip version | 9830 | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:340` |
| DECON mode | psr 2 (MIPI cmd), trig 0 (hw), dsi 0 (single) | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:326-328` |
| DECON TE | gpio `gpc0-4`; sw-TE reg `0x10730a14` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:352,354-357` |
| DSIM | `0x190C0000`, `0x190E0100`, `0x190E0000` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:272` |
| DSIM IRQ | `INTREQ__DPU_DSIM0` = 274 | `include/dt-bindings/interrupt-controller/exynos9830.h:292` |
| MIPI PHY | `dphy_m4m4_dsim0@0x190E0000` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:261-267` |
| DisplayPort | `0x10AB0000` | `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi:294-298` |
| Display clock gate | `GATE_DPU_DPU` (`CLK_DPU_BASE + 1`) | `include/dt-bindings/clock/exynos9830.h:414` |
| DPU gate register | CMU_TOP `0x2060` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:866` |
| Gate bitfields | CG_VAL 21, MANUAL 20, AUTO_GATING 28 | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:5132-5134` |
| Touch I2C bus | `hsi2c_15` @ `0x108D0000`, IRQ `INTREQ__USI07_I2C` = 420 | `arch/arm64/boot/dts/exynos/exynos9830-usi.dtsi:846-862` |
| Touch IRQ pin | `gpa1-0` (`attn_irq`) | `arch/arm64/boot/dts/samsung/exynos9830-hubble_input_common.dtsi:66-71` |
| Touch ID pin | `gpm21-0` (`tsp_id`) | `arch/arm64/boot/dts/samsung/exynos990-x1s_eur_open_05.dts:112-117` |
| Keys | 114 `gpa0-4`, 115 `gpa0-3`, 116 `gpa2-4` (wakeup) | `arch/arm64/boot/dts/samsung/exynos9830-hubble_input_common.dtsi:23-38` |
| Memory node | none in the 9830 tree | §9.2 (absence; contrast `arch/arm64/boot/dts/exynos/exynos9820.dts:73-86`) |
| EL3 monitor carve-out | `0x0BAB00000` (0x5500000) | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:47-50` |
| `sec_debug_magic` | `0x080000000` (0x1000) | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:60-63` |
| `sec_rdx_bootdev` | `0x80000000` (0x0AD00000) | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:65-69` |
| Debug snapshot base | `0xFD900000` | `include/dt-bindings/soc/samsung/debug-snapshot-def.h:54` |
| Debug snapshot pstore | `0xEB308000` (32 KiB) | `include/dt-bindings/soc/samsung/debug-snapshot-def.h:66,97` |
| Seclog | `0x0C3000000`, 8 × 64 KiB | `include/dt-bindings/soc/samsung/exynos9830-seclog.h:23-25` |
| S2MPU table | `0x0C1800000` (0x300000) | `include/dt-bindings/soc/samsung/exynos9830-s2mpu.h:14-15` |
| abox | `0x0F8000000` (0x2A00000) | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:155-162` |
| abox_dbg / abox_slog | 0x5600000 / 0x2800000 in `0x0C0000000`+0x20000000 | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:143-153` |
| Modem `cp_rmem` | `0x0F0000000` (0x800000) | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:215-219` |
| Modem `cp_rmem_2` | `0x0D0000000` (0x4000000) | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:227-231` |
| Modem MSI | `0x0F6200000` (0x1000) | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:221-225` |
| Wi-Fi MSI | `0x0F7200000` (0x1000) | `arch/arm64/boot/dts/exynos/exynos9830-rmem.dtsi:233-236` |
| Clock controller | `0x1a330000` (0x8000) | `arch/arm64/boot/dts/exynos/exynos9830.dts:1160-1162` |
| CMU_TOP / PERIC0 / HSI1 / DPU | `0x1a330000` / `0x10400000` / `0x13000000` / `0x19000000` | `drivers/soc/samsung/cal-if/exynos9830/cmucal-sfr.c:7,42,32,26` |
| PSCI | `arm,psci-1.0`, `method = "smc"` | `arch/arm64/boot/dts/exynos/exynos9830.dts:1459-1462` |
| PSCI conduit code | `arm_smccc_smc(...)` | `drivers/firmware/psci.c:147` |
| Kernel entry contract | MMU off, `x0` = FDT address | `arch/arm64/kernel/head.S:62-64` |
| FDT register | `x21` = FDT (`mov x21, x0`) | `arch/arm64/kernel/head.S:140` |
| `bootargs` | `androidboot.hardware=exynos990 … corememsize=8G …` | `arch/arm64/boot/dts/exynos/exynos9830.dts:72` |
| Initrd range | `0x84000000`-`0x841FFFFF` | `arch/arm64/boot/dts/exynos/exynos9830.dts:73-74` |
| `bootargs_ext` | `root=/dev/ram0 androidboot.boot_devices=13100000.ufs` | `arch/arm64/boot/dts/samsung/exynos990-x1s_eur_open_05.dts:92-94` |
| `ect` parameter area | `0x90000000`, `0x4B000` | `arch/arm64/boot/dts/exynos/exynos9830.dts:65-68` |
| `samsung,boot` / DTBH | not present in the tree | absence (whole-tree search) |
| Panel `s6e3hab` | 1440×3200, DSI 1157 MHz, 4 lanes, DSC 2×(720×40) | `arch/arm64/boot/dts/exynos/exynos9830-display-lcd.dtsi:277-308` |
| Native display mode | 1440×3200 @ 60 | `arch/arm64/boot/dts/samsung/exynos990-hubble_panel_display_modes.dtsi:20,37-56` |

## Display (DECON / DSIM / DPHY)

The 9830 DPU is the "dpu30" generation: DECON is the display controller, the
six DPP/IDMA units are the layer readers, DQE is the quality block and DSIM
the MIPI DSI host.  Sources: `arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi`
(device tree) and `drivers/video/fbdev/exynos/dpu30/cal_9830/regs-{decon,dpp,dsim}.h`
(register map of the 9830, the file the Android driver itself uses).

### Bases

| Block | Address | Size | Source |
|---|---|---|---|
| DECON_F | `0x19050000` | 0x10000 | `exynos9830-dpu.dtsi:306-309` |
| DECON_S | `0x19060000` | 0x10000 | `exynos9830-dpu.dtsi:360-363` |
| DECON_T | `0x19070000` | 0x10000 | `exynos9830-dpu.dtsi:399-402` |
| DSIM | `0x190C0000` | 0x300 | `exynos9830-dpu.dtsi:269-272` |
| DPHY (DSIM) | `0x190E0000` 0x100 + `0x190E0100` 0x700 | | `exynos9830-dpu.dtsi:261-272` |
| DPP n (n = 0..5) | `0x19091000 + n*0x1000` | 0x1000 | `exynos9830-dpu.dtsi:48-96` |
| IDMA n | `0x19041000 + n*0x1000` | 0x1000 | `exynos9830-dpu.dtsi:52,98` |
| disp_ss | `0x19021000` | 0x10 | `exynos9830-dpu.dtsi:256-258` |

### Registers used by the port

| Register | Offset | Bits | Source |
|---|---|---|---|
| `GLOBAL_CONTROL` | `0x0000` | `RUN_STATUS` bit 4, `OPERATION_MODE` bit 8 (1 = command), `DECON_EN` bit 0 | `regs-decon.h:92,99-105` |
| `DATA_PATH_CONTROL_0` | `0x0214` | window `w` enabled = bit `4w`; its channel = bits `4w+0..2` | `regs-decon.h:335-341` |
| `WIN_CONTROL_0(w)` | `0x1000 + 0x30w` | window control | `regs-decon.h:407` |
| `WIN_START_POSITION(w)` | `0x1008 + 0x30w` | x = bits 15:0, y = bits 31:16 | `regs-decon.h:430-432` |
| `WIN_END_POSITION(w)` | `0x100C + 0x30w` | x = bits 15:0, y = bits 31:16 | `regs-decon.h:434-436` |
| `SHADOW_REG_UPDATE_REQ` | `0x0060` | global / per-window / DQE shadow update | `regs-decon.h:154` |
| `IDMA_IN_CON` | `0x0008` | image format bits 13:8 | `regs-dpp.h:115,120` |
| `IDMA_IMG_SIZE` | `0x0018` | width bits 13:0, height bits 29:16 | `regs-dpp.h:184-188` |
| `IDMA_IN_BASE_ADDR_Y8` | `0x0040` | luma / single plane base | `regs-dpp.h:202` |
| `IDMA_IN_BASE_ADDR_C8` | `0x0044` | chroma base | `regs-dpp.h:203` |
| `IDMA_SRC_STRIDE_0` | `0x0050` | chroma stride bits 15:0 | `regs-dpp.h:207-216` |
| `IDMA_SRC_STRIDE_1` | `0x0054` | plane 0 stride bits 15:0, plane 1 bits 31:16 | `regs-dpp.h:218-221` |
| `DSIM_VERSION` / `LINK_STATUS0/1` / `LINK_STATUS3` | `0x0` / `0x8`,`0xc` / `0x14` | `LINK_STATUS3` bit 0 = PLL stable | `regs-dsim.h:18-40` |

Image formats (`IDMA_IMG_FORMAT`): ARGB8888 = 0, XRGB8888 = 4, RGB565 = 8,
YUV420 2-plane = 24 (`regs-dpp.h:122-144`).

### The bootloader leaves the display running

The Android driver does not initialize DECON unconditionally.  It first asks
whether the block already runs and, if so, only re-programs the probe state:

    if (decon_reg_get_run_status(id)) {
            decon_info("decon_reg_init already called by BOOTLOADER\n");
            decon_reg_init_probe(id, dsi_idx, p);
            ...
            return -EBUSY;
    }

(`drivers/video/fbdev/exynos/dpu30/cal_9830/decon_reg.c:1634-1651`; the
comment above it says "DECON does not need to start, if DECON is already
running (enabled in LCD_ON_UBOOT)".)

So on a phone the DECON registers already describe the panel being scanned
out, and the layer base address registers hold the buffer the bootloader
draws its logo into.  There is no `s3cfb.bootloaderfb`-style device tree
property on the 9830 (`exynos9830-dpu.dtsi` has none; the 8895 S8 port relied
on that property), but the same information is in the registers, which is what
`shim/exynos990/decon_probe.c` reads.

### Handoff to XNU

`boot_args.Video` (`pexpert/pexpert/arm64/boot.h:32-41`, `struct Boot_Video`,
six `unsigned long` fields) carries `v_baseAddr`, `v_display`, `v_rowBytes`,
`v_width`, `v_height`, `v_depth`.  `PE_init_platform` copies it into
`PE_state.video` (`pexpert/arm/pe_init.c:406-413`) and `PE_current_console`
returns that to IOKit.  The loader fills it from the DECON probe, so the
kernel console and the framebuffer driver do not need a build-time address.

## UFS descriptor types (plain PRDT and FMP)

The host reads its command descriptors through the UFS protector block
(`0x132A0000`).  `UFSPRSECURITY` (`0x10`) bits 20:19 are `DESCTYPE`:

| DESCTYPE | PRDT entry | Where the table comes from |
|---|---|---|
| 0-2 | 16 bytes (`base`, `upper`, `reserved`, `size`) | the driver builds it |
| 3 | 128 bytes: the 16-byte entry followed by the 112-byte FMP crypto table | `struct fmp_table_setting`, `include/crypto/fmp.h` |

With DESCTYPE 3 the FMP library receives `prdt_off = sizeof(struct
ufshcd_sg_entry)` (16) and `prdt_cnt = number of segments`
(`drivers/scsi/ufs/ufs-exynos-fmp.c:104-112`), i.e. it fills only the
112-byte tail of each 128-byte descriptor and leaves the standard entry at
offset 0 untouched.  A bypassed transfer (no bio crypt context) calls
`exynos_fmp_bypass()`, which leaves the table in its reset state, so a
zeroed tail plus the normal 16-byte entry is a valid bypassed descriptor.
The Android side selects the type through `exynos_smc(SMC_CMD_FMP_SECURITY,
0, fmp_id, CFG_DESCTYPE_3)` (`drivers/crypto/fmp/fmp.c:697-710`).

`bsd/dev/sweet_ufs.c` therefore reads `DESCTYPE` and uses a 128-byte stride
when it is 3 (patch `0128`), capping one request at
`PRDT_AREA / stride * 4 KiB` (128 KiB with the FMP table, 1 MiB plain).

## Touch (sec_ts on hsi2c_15)

| Fact | Value | Source |
|---|---|---|
| Controller | `hsi2c_15` at `0x108D0000` | `exynos/exynos9830-usi.dtsi:846` |
| Touch node | `touchscreen@48`, `compatible = "sec,sec_ts"` | `samsung/exynos9830-hubble_input_common.dtsi:91-106` |
| I2C address | `0x48` | same |
| Interrupt GPIO | `gpa1` pin 0, `sec,irq_type = <8200>` (falling edge) | same |
| GPIO block | pinctrl_0 at `0x15850000`; gpa1 bank at `0x020`, DAT at `+0x04` | `exynos/exynos9830.dts:1211`, `drivers/pinctrl/samsung/pinctrl-exynos-arm64.c` (EXYNOS9 bank table) |
| Firmware | `tsp_sec/y79a_x1.bin` (x1s), `y79a_z3.bin` (z3s), `y771_c1.bin` (c1s) | `samsung/exynos9830-{hubblex1,hubblez3}_input_common.dtsi`, `samsung/exynos9830-c1_input_common.dtsi` |

Model coverage of that one node: `x1s`/`y2s`/`z3s` include
`exynos9830-hubble_input_common.dtsi`; `c1s`/`c2s`/`win2` include
`exynos9830-canvas_input_common.dtsi`, whose `tsp_lsi: touchscreen@48` is the
same `sec,sec_ts` node on the same bus.  The **Galaxy S20 FE (`r8s`) is the
exception**: `exynos990-r8s_input_common.dtsi:101-144` puts a
`zinitix,zt_ts_device` at `0x20` and an `stm,fts_touch` at `0x49` on
`hsi2c_15`, a different protocol.

The XNU side (`bsd/dev/samsung_touch.c`, patch `0129`) now takes the bus,
address and GPIO block from the 990 board header; the register protocol is
still the one of the S8's S6SY761, which is why `SWEET_TOUCH_DEFAULT` stays 0
until the Y79A protocol is ported.

### The Y79A register protocol (for the port)

The 990's sec_ts is the Y79A family (`drivers/input/touchscreen/sec_ts/y79a/`),
not the S6SY761 of the S8.  The command set the XNU driver has to speak:

| Command | Value | Source |
|---|---|---|
| `SEC_TS_CMD_SENSE_ON` | `0x10` | `sec_ts/y79a/sec_ts.h:150` |
| `SEC_TS_READ_ONE_EVENT` | `0x60` | `sec_ts/y79a/sec_ts.h:181` |
| `SEC_TS_READ_ALL_EVENT` | `0x61` | `sec_ts/y79a/sec_ts.h:182` |

The event packet is read from `0x60` after the interrupt line falls; the
report id is the first byte of the packet (the S8 driver already parses the
same sec_ts event layout, so the parser in `bsd/dev/samsung_touch.c` can be
reused once the command constants and the power/reset sequence are the
Y79A ones).  What is still missing for E5 is exactly that: the Y79A power
and reset sequence (`sec_ts_fn.c`), the firmware check (`sec_ts_fw.c`) and a
run on the phone to confirm the panel coordinates.
