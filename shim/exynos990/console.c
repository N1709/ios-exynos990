/* SPDX-License-Identifier: GPL-2.0-only
 * Exynos 990 loader console: the USI debug UART (uart@10540000,
 * USI_PERIC0_UART_DBG in exynos9830-usi.dtsi).
 *
 * The USI block is shared with the SPI and I2C controllers, so the loader
 * puts it into UART mode the way drivers/tty/serial/samsung.c does for
 * Exynos (exynos_usi_init): pulse USI_RESET, then request the clock.
 */
#include <stdint.h>
#include "board.h"
#include "console.h"

#define REG(off)	(*(volatile uint32_t *)(UART_BASE + (off)))

/* Give up instead of hanging the boot when the UART is not wired up. */
#define TX_TIMEOUT	2000000UL

void con_init(void)
{
	/* USI_RESET is active high: set it, then clear it, like the driver. */
	REG(UART_USI_CON) = UART_USI_CON_RESET;
	REG(UART_USI_CON) = 0;
	/* CLKREQ_ON = 1, CLKSTOP_ON = 0 (USI_OPTION[2:1] = 2'h1). */
	REG(UART_USI_OPTION) = UART_USI_OPTION_CLKREQ_ON;

	/* UART mode with the transmitter enabled; keep the rest of sboot's
	 * configuration (baud divider, ...) untouched. */
	uint32_t ulcon = REG(UART_ULCON);
	if ((ulcon & (UART_ULCON_UART | UART_ULCON_TX)) !=
	    (UART_ULCON_UART | UART_ULCON_TX))
		REG(UART_ULCON) = ulcon | UART_ULCON_UART | UART_ULCON_TX;
}

void con_putc(char c)
{
	uint32_t spin = TX_TIMEOUT;

	while (!(REG(UART_UTRSTAT) & UART_UTRSTAT_TXE)) {
		if (!--spin)
			return;
	}
	REG(UART_UTXH) = (uint32_t)(unsigned char)c;
}

void con_puts(const char *s)
{
	while (*s)
		con_putc(*s++);
}
