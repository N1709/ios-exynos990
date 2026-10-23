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

/* DECON shadow/trigger (regs-decon.h) */
#define DECON_SHADOW_REG_UPDATE_REQ	0x0060
#define DECON_SW_UPDATE_FOR_DECON	(0x3fu | (1u << 31))
#define DECON_HW_SW_TRIG_CONTROL	0x0070
#define DECON_SW_TRIG_EN		(1u << 8)

/* SYSMMU of the DPU (exynos9830-sysmmu.dtsi): reg 0 is SYSMMU_CTRL. */
#define SYSMMU_DPU0		0x19100000UL
#define SYSMMU_DPU1		0x19110000UL

/* DSIM (regs-dsim.h) */
#define DSIM_VERSION		0x00
#define DSIM_LINK_STATUS0	0x08
#define DSIM_LINK_STATUS1	0x0c
#define DSIM_LINK_STATUS2	0x10
#define DSIM_LINK_STATUS3	0x14
#define DSIM_CONFIG		0x4c
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
	kv("DSIM_L2", rd(DSIM + DSIM_LINK_STATUS2));
	kv("DSIM_L3", rd(DSIM + DSIM_LINK_STATUS3));
	kv("DSIM_CFG", rd(DSIM + DSIM_CONFIG));
	kv("DSIM_PLL", (rd(DSIM + DSIM_LINK_STATUS3) & DSIM_PLL_STABLE) ? 1 : 0);
	/* Is the DPU's IOMMU translating, or bypassed?  The bootloader has to
	 * leave it in some state for the logo buffer, and the answer decides
	 * whether a new framebuffer address has to be mapped first. */
	kv("SYSMMU_DPU0", rd(SYSMMU_DPU0 + 0x0000));
	kv("SYSMMU_DPU1", rd(SYSMMU_DPU1 + 0x0000));
	kv("SWTRIG", rd(DECON + DECON_HW_SW_TRIG_CONTROL));
	kv("UPDREQ", rd(DECON + DECON_SHADOW_REG_UPDATE_REQ));
	out("\n");
}

/* ---- TEMPORARY: first attempt at putting pixels on the panel -------------
 * The bootloader leaves DECON running with window 5 mapped to DPP0 and the
 * panel scanning out its last frame in self refresh, but DPP0 has no buffer
 * (base_y = base_c = stride = 0) and the DSIM link is down.  Fill a memory
 * area with colour bars, point DPP0 at it, commit the shadow registers and
 * ask DECON for one frame.  Without the DSIM bring-up the frame will not
 * reach the panel; the point of this step is that the DECON side can be
 * checked on its own (GLOBAL run status, DSI FIFO, IOMMU faults) instead of
 * debugging everything at once.
 */
static void wr(uintptr_t a, uint32_t v)
{
	*(volatile uint32_t *)(uintptr_t)a = v;
}

int display_test(uintptr_t fb_pa, unsigned w, unsigned h,
		 void (*kv)(const char *, uint64_t), void (*out)(const char *))
{
	static const uint32_t bars[8] = {
		0xffffffff, 0xff0000ff, 0xff00ff00, 0xffffff00,
		0xffff0000, 0xffff00ff, 0xff00ffff, 0xff202020,
	};
	volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)fb_pa;
	uint32_t con;
	unsigned x, y;
	int spin;

	if (!w || !h || w > 1440 || h > 3200)
		return -1;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++)
			p[(uintptr_t)y * w + x] = bars[(x * 8) / w];
	}
	/* A white block in the top left corner: if the panel ever shows the
	 * bars, this says the origin and the stride are right. */
	for (y = 0; y < 64; y++) {
		for (x = 0; x < 64; x++)
			p[(uintptr_t)y * w + x] = 0xffffffff;
	}

	wr(IDMA(0) + IDMA_IMG_SIZE, (h << 16) | w);
	wr(IDMA(0) + IDMA_IN_BASE_ADDR_Y8, (uint32_t)fb_pa);
	wr(IDMA(0) + IDMA_IN_BASE_ADDR_C8, (uint32_t)fb_pa);
	wr(IDMA(0) + IDMA_SRC_STRIDE_0, w * 4);
	wr(IDMA(0) + IDMA_SRC_STRIDE_1, w * 4);
	con = rd(IDMA(0) + IDMA_IN_CON);
	con = (con & ~IDMA_IMG_FORMAT_MASK) | IDMA_IMG_FORMAT(0);	/* ARGB8888 */
	wr(IDMA(0) + IDMA_IN_CON, con);

	/* Commit and then trigger, in that order: in command mode the
	 * shadow update only completes while a frame is being transferred,
	 * so polling before the trigger always times out
	 * (decon_reg_update_req_and_unmask() calls set_trigger right after
	 * the update request). */
	wr(DECON + DECON_SHADOW_REG_UPDATE_REQ, DECON_SW_UPDATE_FOR_DECON);
	wr(DECON + DECON_HW_SW_TRIG_CONTROL, DECON_SW_TRIG_EN);
	for (spin = 0; spin < 2000; spin++) {
		if (!(rd(DECON + DECON_SHADOW_REG_UPDATE_REQ) & 0x3f))
			break;
	}
	kv("upd_spin", (uint64_t)spin);
	kv("trig", rd(DECON + DECON_HW_SW_TRIG_CONTROL));
	kv("global", rd(DECON + GLOBAL_CONTROL));
	kv("dma0_y", rd(IDMA(0) + IDMA_IN_BASE_ADDR_Y8));
	kv("dma0_sz", rd(IDMA(0) + IDMA_IMG_SIZE));
	kv("dma0_stride", rd(IDMA(0) + IDMA_SRC_STRIDE_1));
	out("\n");
	return 0;
}
/* SYSMMU accessibility probe (step 2a of the display plan).  The DPU's DMA
 * address registers are 32-bit while DRAM starts at 0x880000000, so the
 * display path must go through the SYSMMU; but an earlier read of
 * 0x1910000C raised a data abort (FAR=0x1910000c, ESR=0x96000010) while
 * 0x19100000 read back 0x4.  Read the block one register at a time, each
 * preceded by its own flushed marker, so the next log says exactly which
 * registers are reachable and which are not.
 * Offsets: drivers/iommu/exynos-iommu.h (CTRL 0x000, CFG 0x004,
 * STATUS 0x008, PT_BASE_PPN 0x00C, VERSION 0x034).
 */
void smmu_probe(void (*kv)(const char *, uint64_t), void (*out)(const char *))
{
	out("SYSMMU probe\n");
	out("s0\n"); kv("dpu0_ctrl", rd(SYSMMU_DPU0 + 0x000));
	out("s1\n"); kv("dpu0_cfg", rd(SYSMMU_DPU0 + 0x004));
	out("s2\n"); kv("dpu0_status", rd(SYSMMU_DPU0 + 0x008));
	out("s3\n"); kv("dpu0_pt", rd(SYSMMU_DPU0 + 0x00c));
	out("s4\n"); kv("dpu0_ver", rd(SYSMMU_DPU0 + 0x034));
	out("s5\n"); kv("dpu1_ctrl", rd(SYSMMU_DPU1 + 0x000));
	out("s6\n"); kv("dpu1_ver", rd(SYSMMU_DPU1 + 0x034));
	out("\n");
}
