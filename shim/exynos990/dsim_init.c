/* SPDX-License-Identifier: GPL-2.0-only
 * Exynos 990 DSIM0 + M4M4 PHY bring-up (loader side).
 *
 * The bootloader programs the display, shows the boot logo and hands over
 * with the DSI link shut down, yet the panel still holds its last frame in
 * self refresh.  DECON stays configured for that panel, but its shadow
 * commit never completes while the link is down, so no new frame can be
 * pushed.  This reopens the link with the values the vendor driver uses for
 * the S6E3HAB panel (1440x3200, 4 lanes, 1157 Mb/s, DSC 2 slices), in the
 * order of dsim_reg_disable()'s counterpart in
 * drivers/video/fbdev/exynos/dpu30/cal_9830/dsim_reg.c:2256-2295:
 *
 *   select DPHY reset control -> OSC link clock -> DSIM sw reset -> lanes
 *   -> escape clock -> word clock -> assert DPHY reset -> PHY defaults and
 *   P-D-PHY timings -> PLL -> lane power -> release DPHY reset -> word
 *   clock from PLL -> link config -> HS clock request
 *
 * Register offsets: cal_9830/regs-dsim.h.  PHY module base is 0x190E0100,
 * so its MC block is at 0x190E0300 and the MD lanes at 0x190E0400+0x100n;
 * the PLL at 0x190E0100 is inside the top wrapper and keeps working even
 * while the module is held in reset (which is why an early attempt could
 * lock the PLL but never see PHY_READY).
 */
#include <stdint.h>
#include "dsim_init.h"

#define DSIM		0x190C0000UL
#define PHY_TOP		0x190E0000UL	/* BIAS at +0x00 */
#define PHY_MOD		0x190E0100UL	/* PLL/MC/MD */
#define DISP_SYSREG	0x19021000UL

/* DSIM registers (regs-dsim.h) */
#define R_SWRST		0x04
#define R_LINK3		0x14
#define R_DPHY_STATUS	0x1C
#define R_CLK_CTRL	0x20
#define R_DESKEW	0x24
#define R_TIMEOUT	0x28
#define R_ESCMODE	0x2C
#define R_NUM_XFER	0x30
#define R_UNDERRUN	0x34
#define R_THRESHOLD	0x38
#define R_RESOL		0x3C
#define R_CONFIG	0x4C
#define R_SFR_CTRL	0x64
#define R_CPRS_CTRL	0x74
#define R_SLICE01	0x78
#define R_CMD_CONFIG	0x80
#define R_CMD_TE0	0x84
#define R_CMD_TE1	0x88

/* PHY module registers, absolute */
#define P_PLL_CON(n)	(PHY_MOD + 4 * (n))
#define P_PLL_STAT	(PHY_MOD + 0x40)
#define P_MC_GNR0	(PHY_MOD + 0x200)
#define P_MC_GNR1	(PHY_MOD + 0x204)
#define P_MC_ANA0	(PHY_MOD + 0x208)
#define P_MC_ANA1	(PHY_MOD + 0x20C)
#define P_MC_TIME(n)	(PHY_MOD + 0x230 + 4 * (n))
#define P_MC_DESKEW	(PHY_MOD + 0x250)
#define P_MD(n, reg)	(PHY_MOD + 0x300 + 0x100 * (n) + (reg))
#define P_MD_TIME(n, k)	P_MD(n, 0x30 + 4 * (k))

#define PHY_READY	(1u << 1)

static void wr(uintptr_t a, uint32_t v)
{
	*(volatile uint32_t *)a = v;
}

static uint32_t rd(uintptr_t a)
{
	return *(volatile uint32_t *)a;
}

/* Bounded wait so a dead link cannot hang the loader; the ring is written
 * before this runs, so a timeout leaves a readable log. */
static int poll_set(uintptr_t a, uint32_t mask, unsigned spin)
{
	while (spin--) {
		if ((rd(a) & mask) == mask)
			return 0;
	}
	return -1;
}

int dsim_bringup(void (*kv)(const char *, uint64_t), void (*out)(const char *))
{
	uint32_t clk;
	unsigned n;

	out("DSIM bring-up\n");

	/* 1. Route the DPHY reset control to DSIM0 (bit 4 clear, as the
	 *    vendor's dpu_sysreg_select_dphy_rst_control(..., 0) does).  The
	 *    top reset itself is asserted later: doing it here makes the
	 *    SWRST access below hang, which is exactly where the first
	 *    attempt stopped. */
	out("d0 selrsten\n");
	kv("sysreg", rd(DISP_SYSREG + 0x8));
	wr(DISP_SYSREG + 0x8, rd(DISP_SYSREG + 0x8) & ~(1u << 4));

	/* 2. OSC link clock, then a full DSIM reset. */
	out("d1 reset\n");
	wr(DSIM + R_CLK_CTRL, rd(DSIM + R_CLK_CTRL) & ~(1u << 26));
	wr(DSIM + R_SWRST, 1);
	if (rd(DSIM + R_SWRST) & 1) {
		out("dsim: reset stuck\n");
		return -1;
	}

	/* 3. All lanes, escape clock for all lanes, word clock. */
	out("d2 lanes\n");
	wr(DSIM + R_CONFIG, (rd(DSIM + R_CONFIG) & ~0x1fu) | 0x1fu);
	clk = rd(DSIM + R_CLK_CTRL);
	clk = (clk & ~0x1fu) | 0x1fu;		/* LANE_ESCCLK */
	clk |= 1u << 17;			/* WORDCLK_EN */
	wr(DSIM + R_CLK_CTRL, clk);

	/* 4. PHY analog defaults (dsim_reg.c:44-86) and the 1157 Mb/s row of
	 *    dphy_timing[] for the common block and the four data lanes. */
	out("d3 phy\n");
	wr(PHY_TOP + 0x00, 0x10);
	wr(PHY_TOP + 0x04, 0x110);
	wr(PHY_TOP + 0x08, 0x3223);
	wr(PHY_TOP + 0x0c, 0x0);
	wr(PHY_TOP + 0x10, 0x0);
	for (n = 0; n < 5; n++)
		wr(P_PLL_CON(n), 0);
	wr(P_PLL_CON(5), 0x500);
	wr(P_PLL_CON(6), 0x8e);
	wr(P_PLL_CON(7), 0x1450);
	wr(P_PLL_CON(8), 0xa28);

	wr(P_MC_TIME(4), 0x1db);
	wr(P_MC_TIME(0), 0x1220);
	wr(P_MC_TIME(1), 0x122e);
	wr(P_MC_TIME(2), 0x0741);
	wr(P_MC_TIME(3), 0x0004);
	wr(P_MC_DESKEW, 0);
	for (n = 0; n < 4; n++) {
		wr(P_MD_TIME(n, 4), 0x1db);
		wr(P_MD_TIME(n, 0), 0x1220);
		wr(P_MD_TIME(n, 1), 0x072e);
		wr(P_MD_TIME(n, 2), 0x073b);
		wr(P_MD_TIME(n, 3), 0x0030);
	}
	wr(P_MC_GNR0, 0x0);
	wr(P_MC_GNR1, 0x1450);
	wr(P_MC_ANA0, 0x7033);
	wr(P_MC_ANA1, 0x0004);
	for (n = 0; n < 4; n++) {
		wr(P_MD(n, 0x00), 0x0);
		wr(P_MD(n, 0x04), 0x1450);
		wr(P_MD(n, 0x08), 0x7033);
		wr(P_MD(n, 0x0c), 0x0004);
		wr(P_MD(n, 0x10), 0x0);
		wr(P_MD(n, 0x14), 0x0);
	}

	/* 5. Hold the DPHY in reset while the PLL starts (vendor order: after
	 *    the DSIM reset, before dsim_reg_set_clocks). */
	out("d4 assert\n");
	wr(DISP_SYSREG + 0x8, rd(DISP_SYSREG + 0x8) & ~1u);
	kv("sysreg_rst", rd(DISP_SYSREG + 0x8));

	/* 6. PLL: p=2, m=178, s=2, k=0 - timing,pmsk of the S6E3HAB node -
	 *    lock counter 500 * p, then enable and wait for lock. */
	out("d5 pll\n");
	wr(P_PLL_CON(1), 0x0);
	wr(P_PLL_CON(0), 0x1202);
	wr(P_PLL_CON(2), 0xb2);
	wr(P_PLL_CON(6), 0x38e);
	wr(P_PLL_CON(7), 0x3e8);
	wr(P_PLL_CON(0), rd(P_PLL_CON(0)) | (1u << 12));
	if (poll_set(P_PLL_STAT, 1, 20000)) {
		kv("pll_con0", rd(P_PLL_CON(0)));
		kv("pll_stat", rd(P_PLL_STAT));
		out("dsim: PLL did not lock\n");
		return -1;
	}
	kv("pll", rd(P_PLL_STAT));

	/* 7. Power the lanes. */
	out("d6 lanes\n");
	wr(P_MC_GNR0, 0xffffffffu);
	for (n = 0; n < 4; n++)
		wr(P_MD(n, 0x00), 0xffffffffu);
	kv("mc_gnr_pre", rd(P_MC_GNR0));
	kv("md0_gnr_pre", rd(P_MD(0, 0x00)));

	/* 8. Release the DPHY reset, then wait for PHY_READY everywhere. */
	out("d7 release\n");
	wr(DISP_SYSREG + 0x8, rd(DISP_SYSREG + 0x8) | 1u);
	kv("sysreg_rel", rd(DISP_SYSREG + 0x8));

	if (poll_set(P_MC_GNR0, PHY_READY, 20000)) {
		kv("mc_gnr", rd(P_MC_GNR0));
		out("dsim: common lane not ready\n");
		return -1;
	}
	for (n = 0; n < 4; n++) {
		if (poll_set(P_MD(n, 0x00), PHY_READY, 20000)) {
			kv("md_gnr", rd(P_MD(n, 0x00)));
			out("dsim: data lane not ready\n");
			return -1;
		}
	}
	kv("lanes", 0xf);

	/* 9. Word clock from the PLL, then the fixed link configuration. */
	out("d8 config\n");
	clk = rd(DSIM + R_CLK_CTRL) | (1u << 26);
	wr(DSIM + R_CLK_CTRL, clk);
	wr(DSIM + R_SFR_CTRL, 0x2);
	clk = rd(DSIM + R_CLK_CTRL) | (1u << 25);
	clk = (clk & ~0x1fu) | 0x1fu;
	wr(DSIM + R_CLK_CTRL, clk);
	wr(DSIM + R_DESKEW, 0);
	wr(DSIM + R_TIMEOUT, 0x00ffffff);
	wr(DSIM + R_ESCMODE, 0x01400000);
	wr(DSIM + R_UNDERRUN, 0x1e6e);
	wr(DSIM + R_THRESHOLD, 0x1e0);
	wr(DSIM + R_RESOL, 0x0c8001e0);	/* 3200 lines, 480 compressed bytes */
	wr(DSIM + R_NUM_XFER, 0xc80);
	wr(DSIM + R_CONFIG, 0x00087cff);
	wr(DSIM + R_CPRS_CTRL, 0x0a);	/* DSC: 2 slices per packet */
	wr(DSIM + R_SLICE01, 0x02d002d0);
	wr(DSIM + R_CMD_CONFIG, 0);
	wr(DSIM + R_CMD_TE0, 28);
	wr(DSIM + R_CMD_TE1, 0x2cb933c9);

	/* 10. Escape clock and the HS clock request. */
	clk = rd(DSIM + R_CLK_CTRL);
	clk = (clk & ~0xffu) | 0x04;		/* ESC_PRESCALER(4) */
	clk |= 1u << 16;			/* ESCCLK_EN */
	wr(DSIM + R_CLK_CTRL, clk);
	wr(DSIM + R_CLK_CTRL, rd(DSIM + R_CLK_CTRL) | (1u << 20));

	kv("clk_ctrl", rd(DSIM + R_CLK_CTRL));
	kv("config", rd(DSIM + R_CONFIG));
	kv("link3", rd(DSIM + R_LINK3));
	kv("dphy", rd(DSIM + R_DPHY_STATUS));
	out("\n");
	return 0;
}
