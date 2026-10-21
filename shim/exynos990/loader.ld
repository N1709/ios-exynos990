/* SPDX-License-Identifier: GPL-2.0-only
 * Linker script for the Exynos 990 loader.
 *
 * The image is position independent: sboot loads it at 0x10008000 and
 * there is no relocation pass, so everything is linked at 0 and addressed
 * with PC-relative instructions.
 */
ENTRY(_head)
SECTIONS
{
	. = 0;
	.text : { *(.text.head) *(.text*) }
	.rodata : { *(.rodata*) }
	.data : { *(.data*) }
	. = ALIGN(16);
	_bss_start = .;
	.bss : { *(.bss*) *(COMMON) }
	. = ALIGN(16);
	_bss_end = .;
	. += 0x8000;
	. = ALIGN(16);
	_stack_top = .;
	_end = .;
	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
