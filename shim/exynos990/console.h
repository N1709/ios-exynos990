/* SPDX-License-Identifier: GPL-2.0-only
 * Exynos 990 loader console (USI debug UART).
 */
#pragma once

void con_init(void);
void con_putc(char c);
void con_puts(const char *s);
