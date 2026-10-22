/* SPDX-License-Identifier: GPL-2.0-only
 * Exynos 990 display probe (read-only, loader side).
 */
#pragma once
#include <stdint.h>

/* What the probe found about the panel the bootloader is scanning out. */
struct decon_fb {
	uint64_t phys;		/* physical address of the buffer */
	uint32_t width;		/* pixels */
	uint32_t height;	/* pixels */
	uint32_t stride;	/* bytes per line */
	uint32_t format;	/* IDMA_IMG_FORMAT value */
	int running;		/* DECON is scanning out */
	int found;		/* a buffer was found */
};

void decon_probe(void (*kv)(const char *, uint64_t), void (*out)(const char *),
		 struct decon_fb *fb);
