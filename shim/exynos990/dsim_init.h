/* SPDX-License-Identifier: GPL-2.0-only
 * Exynos 990 DSIM0 + M4M4 PHY bring-up (loader side).
 */
#pragma once

/* Reopen the DSI link the bootloader shut down.  Returns 0 on success; the
 * caller is expected to print the checkpoints either way. */
int dsim_bringup(void (*kv)(const char *, uint64_t), void (*out)(const char *));
