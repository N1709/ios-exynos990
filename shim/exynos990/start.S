/* SPDX-License-Identifier: GPL-2.0-only
 * Galaxy S20 family (Exynos 990) XNU loader entry.
 *
 * arm64 Linux Image header so Samsung sboot treats us like a kernel: the
 * boot image v2 puts us at 0x10008000 and enters at EL1 with the MMU and
 * the caches off.  sboot on Exynos passes no DTB in x0 (it stays 0), so
 * the loader must not depend on one.
 *
 * Position independent: sboot loads the image wherever it wants and there
 * is no relocation pass, so only the header's image_size (linked at 0) may
 * be an absolute address.
 */
	.section .text.head, "ax"
	.globl _head
_head:
	b	_start			/* code0 */
	.long	0			/* code1 */
	.quad	0			/* text_offset */
	.quad	_end			/* image_size (linked at 0) */
	.quad	0xa			/* flags: LE, 4K pages, place anywhere */
	.quad	0
	.quad	0
	.quad	0
	.ascii	"ARM\x64"		/* magic */
	.long	0

	.text
	.globl _start
_start:
	msr	daifset, #0xf
	mov	x19, x0			/* DTB, if the bootloader passed one */
	mrs	x20, CurrentEL
	mrs	x21, sctlr_el1
	msr	spsel, #1
	adr	x1, _stack_top
	mov	sp, x1
	adr	x1, vectors
	msr	vbar_el1, x1
	isb
	adr	x1, _bss_start
	adr	x2, _bss_end
1:	cmp	x1, x2
	b.hs	2f
	str	xzr, [x1], #8
	b	1b
2:	/* Durable proof of execution.  Every log sink has lost the race
	 * with whatever runs next, but the ramoops dump zone at 0xeb308000
	 * is only read (never rewritten) by the next kernel: TWRP's
	 * dmesg-ramoops-0 still held a 2026-10-19 panic, so a record put
	 * here survives the reset and is visible as
	 * /sys/fs/pstore/dmesg-ramoops-0 on the next boot.  It is written
	 * before the first C instruction, so its absence means S-Boot never
	 * jumped to this image. */
	movz	x2, #0xeb30, lsl #16
	movk	x2, #0x8000
	adr	x1, alive_blob
	mov	x3, #(alive_blob_end - alive_blob)
6:	ldrb	w4, [x1], #1
	strb	w4, [x2], #1
	subs	x3, x3, #1
	b.ne	6b
	dsb	sy

	mov	x0, x19
	mov	x1, x20
	mov	x2, x21
	bl	ex_main			/* returns XNU entry PA, or 0 */
	cbz	x0, 4f
	mov	x16, x0
	adr	x1, ex_boot_args
	ldr	x0, [x1]		/* boot_args PA for start_first_cpu */
	/* XNU's entry expects the MMU and the caches off.  The data was
	 * written with the MMU off (Device memory) and the copies were
	 * cleaned to PoC anyway. */
	mrs	x14, sctlr_el1
	bic	x14, x14, #1		/* M */
	bic	x14, x14, #4		/* C */
	bic	x14, x14, #0x1000	/* I */
	msr	sctlr_el1, x14
	ic	iallu
	dsb	sy
	isb
	br	x16
4:	wfe
	b	4b

.macro vent slot
	.balign	0x80
	mov	x0, #\slot
	mov	x1, sp
	b	exc_common
.endm

	.balign	0x800
vectors:
	vent 0
	vent 1
	vent 2
	vent 3
	vent 4
	vent 5
	vent 6
	vent 7
	vent 8
	vent 9
	vent 10
	vent 11
	vent 12
	vent 13
	vent 14
	vent 15

exc_common:
	bl	ex_exception		/* never returns */
5:	wfe
	b	5b

	/* persistent_ram record: {sig, start, size} then pstore text.  The
	 * dump zone's data area is 16384-12 bytes and TWRP's kernel parses
	 * it with the "====<sec>.<usec>-D\n" header the ramoops driver
	 * expects (fs/pstore/ram.c:157-175). */
	.section .rodata
	.balign	8
alive_blob:
	.long	0x43474244			/* "DBGC" */
	.long	0				/* start */
	.long	alive_blob_end - alive_blob - 12	/* size */
	.ascii	"====0.0-D\n"
	.ascii	"EXYNOS990 ALIVE: S-Boot jumped to the loader\n"
alive_blob_end:
