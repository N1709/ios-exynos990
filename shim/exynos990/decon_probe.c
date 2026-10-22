/* SPDX-License-Identifier: GPL-2.0-only
 * Exynos 990 (universal9830) display probe, run by the loader before XNU.
 *
 * The Samsung bootloader programs the display before it hands over: the
 * Android DPU driver of the 9830 starts with
 *
 *   "DECON does not need to start, if DECON is already running
 *    (enabled in LCD_ON_UBOOT)"            (cal_9830/decon_reg.c)
 *
 * and only then falls back to the full init.  So on a phone the DECON
 * registers already describe the panel that is being scanned out: the
 * window geometry, the image size and the address of the buffer the
 * bootloader draws its logo into.  Reading them is the cheapest way to
 * learn the display facts of the model (panel size, buffer address,
 * video or command mode) before anything is flashed.
 *
 * The probe only reads; it never writes a display register.
 *
 * Register sources (Samsung Android kernel tree
 * android_kernel_samsung_exynos990):
 *   arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi
 *       decon_f@0x19050000, dsim@0x190C0000, dphy@0x190E0000,
 *       dpp_0..dpp_6 (0x19091000 + n*0x1000) and their IDMA blocks
 *       (0x19041000 + n*0x1000)
 *   drivers/video/fbdev/exynos/dpu30/cal_9830/regs-decon.h  (DECON)
 *   drivers/video/fbdev/exynos/dpu30/cal_9830/regs-dpp.h    (DPP/IDMA)
 *   drivers/video/fbdev/exynos/dpu30/cal_9830/regs-dsim.h   (DSIM)
 */
#include <stdint.h>
#include "decon_probe.h"

#define DECON		0x19050000UL	/* decon_f@0x19050000 */
#define DSIM		0x190C0000UL	/* dsim@0x190C0000 */
#define DPP(n)		(0x19091000UL + (n) * 0x1000UL)
#define IDMA(n)		(0x19041000UL + (n) * 0x1000UL)

/* DECON (regs-decon.h) */
#define GLOBAL_CONTROL		0x0000
#define DATA_PATH_CONTROL_0	0x0214
#define WIN_CONTROL_0(w)	(0x1000 + ((w) * 0x30))
#define WIN_START_POSITION(w)	(0x1008 + ((w) * 0x30))
#define WIN_END_POSITION(w)	(0x100C + ((w) * 0x30))
#define GLOBAL_RUN_STATUS	(1u << 4)	/* DECON is scanning out */
#define GLOBAL_OP_MODE		(1u << 8)	/* 1 = command mode */
#define WIN_EN_F(w)		(1u << (4 * (w)))
#define WIN_CHMAP_F(w)		(4 * (w))	/* channel of the window */

/* DPP / IDMA (regs-dpp.h) */
#define IDMA_IN_CON		0x0008
#define IDMA_IMG_FORMAT(_v)	((_v) << 8)
#define IDMA_IMG_FORMAT_MASK	(0x3f << 8)
#define IDMA_IMG_SIZE		0x0018
#define IDMA_IN_BASE_ADDR_Y8	0x0040
#define IDMA_IN_BASE_ADDR_C8	0x0044
#define IDMA_SRC_STRIDE_0	0x0050	/* chroma stride */
#define IDMA_SRC_STRIDE_1	0x0054	/* plane 0 / plane 1 stride */

/* DSIM (regs-dsim.h) */
#define DSIM_VERSION		0x00
#define DSIM_LINK_STATUS0	0x08
#define DSIM_LINK_STATUS1	0x0c
#define DSIM_LINK_STATUS3	0x14
#define DSIM_PLL_STABLE		(1u << 0)

#define NUM_WINDOWS	8
#define NUM_DPP		6

static uint32_t rd(uintptr_t a)
{
	return *(volatile uint32_t *)(uintptr_t)a;
}

/* Read the layer description of one data path unit. */
static void read_layer(unsigned n, struct decon_fb *fb)
{
	uint32_t size, con, stride0, stride1;

	size = rd(IDMA(n) + IDMA_IMG_SIZE);
	fb->width = size & 0x3fff;
	fb->height = size >> 16;
	fb->phys = rd(IDMA(n) + IDMA_IN_BASE_ADDR_Y8);
	con = rd(IDMA(n) + IDMA_IN_CON);
	fb->format = (con & IDMA_IMG_FORMAT_MASK) >> 8;
	stride0 = rd(IDMA(n) + IDMA_SRC_STRIDE_0);
	stride1 = rd(IDMA(n) + IDMA_SRC_STRIDE_1);
	/* the luma plane stride is the low half of SRC_STRIDE_1 */
	fb->stride = stride1 & 0xffff;
	if (!fb->stride)
		fb->stride = stride0 & 0xffff;
}

void decon_probe(void (*kv)(const char *, uint64_t), void (*out)(const char *),
		 struct decon_fb *fb)
{
	uint32_t gc, dpc;
	unsigned n, unit = NUM_DPP;

	fb->phys = 0;
	fb->width = fb->height = fb->stride = fb->format = 0;
	fb->running = 0;
	fb->found = 0;

	out("DECON probe (read-only)\n");

	gc = rd(DECON + GLOBAL_CONTROL);
	kv("GLOBAL", gc);
	fb->running = (gc & GLOBAL_RUN_STATUS) ? 1 : 0;
	kv("run", fb->running);
	kv("cmdmode", (gc & GLOBAL_OP_MODE) ? 1 : 0);

	dpc = rd(DECON + DATA_PATH_CONTROL_0);
	kv("DPC0", dpc);

	/* The enabled window tells which data path unit carries the image:
	 * DATA_PATH_CONTROL_0 holds, per window, the channel it is mapped
	 * to (WIN_CHMAP_F). */
	for (n = 0; n < NUM_WINDOWS; n++) {
		uint32_t start, end;

		if (!(dpc & WIN_EN_F(n)))
			continue;
		start = rd(DECON + WIN_START_POSITION(n));
		end = rd(DECON + WIN_END_POSITION(n));
		out("  window ");
		kv("id", n);
		kv("ch", (dpc >> WIN_CHMAP_F(n)) & 7);
		kv("ctrl", rd(DECON + WIN_CONTROL_0(n)));
		/* x in bits 15:0, y in bits 31:16 */
		kv("x0", start & 0xffff);
		kv("y0", start >> 16);
		kv("x1", end & 0xffff);
		kv("y1", end >> 16);
		if (unit == NUM_DPP)
			unit = (dpc >> WIN_CHMAP_F(n)) & 7;
	}

	if (unit < NUM_DPP) {
		read_layer(unit, fb);
		if (fb->phys)
			fb->found = 1;
	} else {
		/* No window: look for a layer that is programmed anyway. */
		for (n = 0; n < NUM_DPP && !fb->found; n++) {
			read_layer(n, fb);
			fb->found = fb->phys ? 1 : 0;
		}
	}

	for (n = 0; n < NUM_DPP; n++) {
		uint32_t size, by, bc, stride;

		size = rd(IDMA(n) + IDMA_IMG_SIZE);
		by = rd(IDMA(n) + IDMA_IN_BASE_ADDR_Y8);
		bc = rd(IDMA(n) + IDMA_IN_BASE_ADDR_C8);
		stride = rd(IDMA(n) + IDMA_SRC_STRIDE_1) & 0xffff;
		if (!size && !by && !bc && !stride)
			continue;
		out("  dpp ");
		kv("id", n);
		kv("w", size & 0x3fff);
		kv("h", size >> 16);
		kv("base_y", by);
		kv("base_c", bc);
		kv("stride", stride);
		kv("dpp_base", rd(DPP(n)));
	}

	if (fb->found) {
		out("  framebuffer ");
		kv("phys", fb->phys);
		kv("w", fb->width);
		kv("h", fb->height);
		kv("stride", fb->stride);
		kv("format", fb->format);
	}

	kv("DSIM_VER", rd(DSIM + DSIM_VERSION));
	kv("DSIM_L0", rd(DSIM + DSIM_LINK_STATUS0));
	kv("DSIM_L1", rd(DSIM + DSIM_LINK_STATUS1));
	kv("DSIM_PLL", (rd(DSIM + DSIM_LINK_STATUS3) & DSIM_PLL_STABLE) ? 1 : 0);
	out("\n");
}
