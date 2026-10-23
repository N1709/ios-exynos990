/* SPDX-License-Identifier: GPL-2.0-only
 * Exynos 990 DSIM0 + M4M4 PHY bring-up (loader side).
 *
 * The bootloader programs the display, shows the boot logo and then hands
 * over with the DSI link shut down but the panel still holding its last
 * frame in self refresh.  DECON is left configured for that panel, yet its
 * shadow commit never completes while the link is down, so no new frame can
 * be pushed.  This file reopens the link with the same register values the
 * vendor driver uses for the S6E3HAB panel (1440x3200, 4 lanes, 1157 Mb/s,
 * DSC 2 slices), in the order dsim_reg.c does it:
 *
 *   isolation bypass -> SYSREG reset assert -> DSIM reset -> PHY defaults
 *   -> PLL lock -> lane enable -> reset release -> timing/config -> start
 *
 * Every value is taken from the vendor tree:
 *   drivers/video/fbdev/exynos/dpu30/dsim_reg.c
 *   drivers/video/fbdev/exynos/dpu30/cal_9830/regs-dsim.h
 *   arch/arm64/boot/dts/exynos/exynos9830-dpu.dtsi
 *   arch/arm64/boot/dts/exynos/exynos9830-display-lcd.dtsi (S6E3HAB)
 */
#include <stdint.h>
#include "dsim_init.h"

#define DSIM		0x190C0000UL
#define PHY		0x190E0000UL	/* BIAS at +0x00, module at +0x100 */
#define DISP_SYSREG	0x19021000UL
#define PMU		0x15860000UL

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
#define P_PLL_CON(n)	(PHY + 0x100 + 4 * (n))
#define P_PLL_STAT	(PHY + 0x140)
#define P_MC_GNR0	(PHY + 0x200)
#define P_MC_GNR1	(PHY + 0x204)
#define P_MC_ANA0	(PHY + 0x208)
#define P_MC_ANA1	(PHY + 0x20C)
#define P_MC_TIME(n)	(PHY + 0x230 + 4 * (n))
#define P_MC_DESKEW	(PHY + 0x250)
#define P_MD(n, reg)	(PHY + 0x300 + 0x100 * (n) + (reg))
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

/* Wait until (rd(a) & mask) == mask, bounded so a dead link cannot hang the
 * loader (the ring is written before this runs, so a timeout is visible). */
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

	/* The PMU isolation bit and the SYSREG reset are left alone: the
	 * bootloader had to open them to show the logo, the previous attempt
	 * died on the first PMU access (the log stopped right here), and the
	 * PMU power domains on this chip are reached through an SMC anyway.
	 * If the PHY below turns out to be held in reset, these come back. */

	/* 1. DSIM clock from OSC, then a full reset. */
	out("d1 reset\n");
	clk = rd(DSIM + R_CLK_CTRL) & ~(1u << 26);
	wr(DSIM + R_CLK_CTRL, clk);
	wr(DSIM + R_SWRST, 1);
	if (poll_set(DSIM + R_SWRST, 0, 1000) == 0 && (rd(DSIM + R_SWRST) & 1)) {
		out("dsim: reset stuck\n");
		return -1;
	}

	/* 4. All lanes on, then the escape clock divider the driver uses for
	 *    an 18 MHz escape clock (prescaler 4, word clock on). */
	wr(DSIM + R_CONFIG, (rd(DSIM + R_CONFIG) & ~0x1fu) | 0x1fu);
	clk = rd(DSIM + R_CLK_CTRL);
	clk = (clk & ~0x1fu) | 0x1fu;		/* LANE_ESCCLK: all lanes */
	clk |= 1u << 17;			/* WORDCLK_EN */
	wr(DSIM + R_CLK_CTRL, clk);

	/* 5. PHY analog defaults (dsim_reg.c:44-86). */
	out("d2 phy\n");
	wr(PHY + 0x00, 0x10);
	wr(PHY + 0x04, 0x110);
	wr(PHY + 0x08, 0x3223);
	wr(PHY + 0x0c, 0x0);
	wr(PHY + 0x10, 0x0);
	for (n = 0; n < 5; n++)
		wr(P_PLL_CON(n), 0);
	wr(P_PLL_CON(5), 0x500);
	wr(P_PLL_CON(6), 0x8e);
	wr(P_PLL_CON(7), 0x1450);
	wr(P_PLL_CON(8), 0xa28);

	/* 6. D-PHY timings for the 1157 Mb/s row of dphy_timing[]. */
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

	/* 7. Common and per-lane analog blocks. */
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

	/* 8. PLL: p=2, m=178, s=2, k=0 (timing,pmsk of the S6E3HAB node),
	 *    lock counter 500 * p, then enable and wait for lock. */
	out("d3 pll\n");
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

	/* 9. Turn the lanes on and wait for PHY_READY on each. */
	out("d4 lanes\n");
	wr(P_MC_GNR0, 0xffffffffu);
	if (poll_set(P_MC_GNR0, PHY_READY, 20000)) {
		kv("mc_gnr", rd(P_MC_GNR0));
		out("dsim: common lane not ready\n");
		return -1;
	}
	for (n = 0; n < 4; n++) {
		wr(P_MD(n, 0x00), 0xffffffffu);
		if (poll_set(P_MD(n, 0x00), PHY_READY, 20000)) {
			kv("md_gnr", rd(P_MD(n, 0x00)));
			out("dsim: data lane not ready\n");
			return -1;
		}
	}
	kv("lanes", 0xf);

	/* 10. Switch the word clock to the PLL. */
	clk = rd(DSIM + R_CLK_CTRL) | (1u << 26);
	wr(DSIM + R_CLK_CTRL, clk);

	/* 11. Fixed link configuration (command mode, RGB24, 4 lanes). */
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

	/* 12. Escape clock back on, then request the HS clock. */
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
