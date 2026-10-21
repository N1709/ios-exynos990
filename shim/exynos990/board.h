/* SPDX-License-Identifier: GPL-2.0-only
 * Galaxy S20 family (Samsung Exynos 990, SoC id universal9830) board facts.
 *
 * Every address in here comes from the Samsung Android kernel tree
 * (android_kernel_samsung_exynos990) and is listed with its source in
 * docs/exynos990-hardware.md.  The ones marked "verify" can only be
 * confirmed on the phone (live DT / /proc/iomem) and must be re-checked
 * before the port is trusted.
 */
#pragma once

/* ---- boot image v2 layout (Samsung sboot) -------------------------------
 *
 * The phone boots from the "boot" partition: sboot reads the boot image
 * header we build with tools/mkboot_exynos990.py and loads
 *   - the "kernel"  (our loader Image)  at base + kernel_offset
 *   - the "ramdisk" (the XNU payload)  at base + ramdisk_offset
 * with base = 0x10000000.  Nothing else is guaranteed: sboot on Exynos
 * passes no DTB to the kernel, so the loader must not need one.
 */
#define BOOT_BASE			0x10000000UL
#define KERNEL_OFFSET			0x00008000UL
#define RAMDISK_OFFSET			0x01000000UL
#define KERNEL_LOAD_ADDR		(BOOT_BASE + KERNEL_OFFSET)
#define RAMDISK_LOAD_ADDR		(BOOT_BASE + RAMDISK_OFFSET)

/* Fallback search range for the payload container, in case sboot ignores
 * the ramdisk address from the header (some Samsung loaders relocate the
 * ramdisk).  The whole boot region is scanned in 16-byte steps. */
#define BOOT_SCAN_START			RAMDISK_LOAD_ADDR
#define BOOT_SCAN_END			0x14000000UL

/* ---- USI debug UART ------------------------------------------------------
 * exynos9830-usi.dtsi: "serial_0: uart@10540000", compatible
 * "samsung,exynos-uart", reg 0x100 bytes, 115200 8N1.  This is
 * USI_PERIC0_UART_DBG, the UART the debug pads are wired to.  The other
 * node in that file, "usi_uart_dbg: usi@10421000", is the USI block mux
 * with a 4-byte reg, not the UART registers.
 *
 * Register layout is the S3C24XX one (include/linux/serial_s3c.h in the
 * Android kernel).  The bootloader owns the pin mux and the clock; the
 * loader only writes ULCON/UTRSTAT/UTXH.
 */
#define UART_BASE			0x10540000UL
#define UART_ULCON			0x00
#define UART_UTRSTAT			0x10
#define UART_UFSTAT			0x18
#define UART_UTXH			0x20
#define UART_URXH			0x24
#define UART_UBRDIV			0x28
#define UART_USI_CON			0xc4
#define UART_USI_OPTION			0xc8
#define UART_UTRSTAT_TXE		(1u << 2)	/* transmit buffer empty */
#define UART_UTRSTAT_TXFE		(1u << 1)	/* FIFO empty */
#define UART_ULCON_UART		(1u << 0)	/* 8-bit UART mode */
#define UART_ULCON_TX		(1u << 1)	/* transmitter enable */
#define UART_USI_CON_RESET		(1u << 0)
#define UART_USI_OPTION_CLKREQ_ON	(1u << 1)

/* ---- GICv2 ---------------------------------------------------------------
 * exynos9830.dts: "gic:interrupt-controller@10100000", compatible
 * "arm,cortex-a15-gic" (GIC-400).  reg: 0x10101000 (distributor),
 * 0x10102000 (CPU interface), 0x10104000 (VCPU), 0x10106000 (CPU).
 */
#define GICD_BASE			0x10101000UL
#define GICC_BASE			0x10102000UL
#define GICD_VCPU_BASE			0x10104000UL
#define GICD_TYPER			0x04

/* ---- UFS -----------------------------------------------------------------
 * exynos9830.dts: "ufs: ufs@0x13100000": HCI standard block 0x200 bytes at
 * 0x13100000, the Exynos vendor block 0x200 bytes at 0x13101100, UNIPRO at
 * 0x13180000 (0x8000) and the protector at 0x132A0000.
 */
#define UFS_HCI_BASE			0x13100000UL
#define UFS_HCI_VENDOR_BASE		0x13101100UL
#define UFS_PROTECTOR_BASE		0x132A0000UL

/* Scratch area for the read-only UFS probe: inside the assumed high DRAM
 * bank, above the 512 MiB XNU window's first 384 MiB, so the probe never
 * touches the boot image (0x10000000) and is wiped by the loader's zeroing
 * of the XNU window afterwards. */
#define UFS_DMA_BASE			0x8f0000000UL

/* ---- clock controller ----------------------------------------------------
 * exynos9830.dts: "clock: clock-controller@0x1a330000" (0x8000 bytes).
 */
#define CLK_BASE			0x1a330000UL

/* ---- DRAM ----------------------------------------------------------------
 * UNVERIFIED on the phone.  exynos9830.dts has no memory node at all (the
 * S20 board trees in the Android kernel are "open" trees: no DRAM
 * description), and the bootloader passes its own map to the kernel.
 * The Exynos 9820 tree in the same kernel describes memory@80000000
 * (2 GiB), memory@880000000 (2 GiB) and memory@900000000 (0x80000000),
 * which is where the 0x880000000 high bank comes from.
 *
 * Settle it on the phone: TWRP "cat /proc/iomem" and read the System RAM
 * ranges.  The loader prints the MIDR/MPIDR only, it cannot read the map.
 */
#define LOW_RAM_BASE			0x80000000UL
#define LOW_RAM_END			0x100000000UL
#define HIGH_RAM_BASE			0x880000000UL
#define HIGH_RAM_END			0x900000000UL

/* XNU's RAM window: the high bank, like the S8 (Exynos 8895) port.  The
 * low banks hold the EL3 monitor (0xBAB00000), abox, the modem and the
 * framebuffer, so they are not usable for the kernel. */
#define XNU_RAM_BASE			HIGH_RAM_BASE
#define XNU_RAM_END			HIGH_RAM_END

/* XNU runs with 4 KiB pages, like the A5XELTE and S8 ports (all Exynos
 * cores support 4 KiB; the 16 KiB granule is optional and would need a
 * separate XNU page-size configuration). */
#define PAGE_SIZE			4096UL
#define BOOT_LINE_LENGTH		608
