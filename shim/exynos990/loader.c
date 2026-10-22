/* SPDX-License-Identifier: GPL-2.0-only
 * Galaxy S20 family (Samsung Exynos 990, universal9830) XNU loader.
 *
 * sboot loads the boot image v2 we build with tools/mkboot_exynos990.py:
 *   - "kernel"  = this loader Image, at 0x10008000
 *   - "ramdisk" = the XNU payload container (tools/mkpayload.py, "SWEETXNU"
 *     v1: Mach-O segments, ADT and boot-args), at 0x11000000
 *
 * The loader finds that container, copies the kernel segments into the
 * kernel-managed RAM window the payload names (the high DRAM bank at
 * 0x880000000, free of firmware carve-outs), places the ADT and the
 * boot_args behind the kernel and jumps to XNU's physical entry with the
 * MMU and the caches off - the same hand-off shim/a5xelte/xnu_boot.c and
 * shim/dreamlte/loader.c use.
 *
 * Log: the USI debug UART (uart@10540000, USI_PERIC0_UART_DBG).  A USB-TTL adapter
 * on the phone's debug UART pins shows it.  There is no screen console yet:
 * sboot leaves no framebuffer behind, and the DECON handoff for Exynos
 * 9830 is not written (see EXYNOS990_PROGRESS.md).
 */
#include <stdint.h>
#include "../fdt.h"
#include "board.h"
#include "console.h"
#include "ufs_probe.h"
#include "decon_probe.h"

/* ---- payload container (tools/mkpayload.py, "SWEETXNU" v1) ---- */
struct payload_seg {
	uint64_t vmaddr, vmsize, file_off, file_size;
};
struct payload {
	char magic[8];
	uint32_t version, nsegs;
	uint64_t virt_base, entry_va, adt_off, adt_size, kernel_end;
	uint64_t phys_base, mem_size;
	char cmdline[BOOT_LINE_LENGTH];
	struct payload_seg segs[];
};
struct boot_video {
	uint64_t v_baseAddr, v_display, v_rowBytes, v_width, v_height, v_depth;
};
struct boot_args {
	uint16_t Revision, Version;
	uint64_t virtBase, physBase, memSize, topOfKernelData;
	struct boot_video Video;
	uint32_t machineType;
	uint64_t deviceTreeP;
	uint32_t deviceTreeLength;
	char CommandLine[BOOT_LINE_LENGTH];
	uint64_t bootFlags, memSizeActual;
};
_Static_assert(__builtin_offsetof(struct payload, segs) == 0x2a8, "payload layout");
_Static_assert(__builtin_offsetof(struct boot_args, CommandLine) == 108, "boot_args layout");
_Static_assert(sizeof(struct boot_args) == 736, "boot_args size");

uint64_t ex_boot_args;		/* boot_args PA, read by start.S */

/* ---- RAM log sink -------------------------------------------------------
 * Every line the loader prints also goes to TWRP's ramoops pmsg zone
 * (board.h), in persistent_ram format, so the log survives the reset a
 * failed boot causes.  XNU appends to the same ring (pe_serial.c), which
 * makes /sys/fs/pstore/pmsg-ramoops-0 show loader + kernel text.. */
#define KLOG_CAP	(KLOG_SIZE - 12)

static volatile uint32_t *const klog_hdr = (volatile uint32_t *)KLOG_PHYS;
static volatile uint8_t *const klog_data = (volatile uint8_t *)(KLOG_PHYS + 12);
static uint32_t klog_off, klog_len;

static void klog_init(void)
{
	/* Continue a ring left by an earlier run only when its header is
	 * consistent (start == size, both inside the data area). */
	uint32_t start = klog_hdr[1], size = klog_hdr[2];

	if (klog_hdr[0] == 0x43474244u /* "DBGC" */ && start < KLOG_CAP &&
	    size < KLOG_CAP && start == size) {
		klog_off = klog_len = size;
		return;
	}
	klog_hdr[0] = 0x43474244u;
	klog_off = klog_len = 0;
	klog_hdr[1] = 0;
	klog_hdr[2] = 0;
}

static void klog_putc(char c)
{
	klog_data[klog_off] = (uint8_t)c;
	klog_off = (klog_off + 1) % KLOG_CAP;
	if (klog_len < KLOG_CAP)
		klog_len++;
	klog_hdr[1] = klog_off;
	klog_hdr[2] = klog_len;
}

/* ---- tiny console helpers ---- */
static void out(const char *s)
{
	con_puts(s);
	while (*s)
		klog_putc(*s++);
}

static void hex(uint64_t v)
{
	static const char digits[] = "0123456789abcdef";
	char b[19] = "0x";
	int n = 2, started = 0;

	for (int i = 60; i >= 0; i -= 4) {
		int d = (v >> i) & 0xf;
		if (d || started || i == 0) {
			b[n++] = digits[d];
			started = 1;
		}
	}
	b[n] = 0;
	out(b);
}

static void kv(const char *k, uint64_t v)
{
	out(k);
	out("=");
	hex(v);
	out(" ");
}

/* ---- memory helpers (MMU off: everything is Device memory) ---- */
static void copy(uintptr_t dst, uintptr_t src, uint64_t n)
{
	if (!((dst | src) & 7)) {
		for (; n >= 8; n -= 8, dst += 8, src += 8)
			*(volatile uint64_t *)dst = *(volatile uint64_t *)src;
	}
	for (; n; n--, dst++, src++)
		*(volatile uint8_t *)dst = *(volatile uint8_t *)src;
}

static void zero(uintptr_t dst, uint64_t n)
{
	for (; n && (dst & 7); n--, dst++)
		*(volatile uint8_t *)dst = 0;
	for (; n >= 8; n -= 8, dst += 8)
		*(volatile uint64_t *)dst = 0;
	for (; n; n--, dst++)
		*(volatile uint8_t *)dst = 0;
}

static void clean(uintptr_t start, uintptr_t end)
{
	uint64_t ctr;
	__asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
	uintptr_t line = 4UL << ((ctr >> 16) & 0xf);

	for (uintptr_t p = start & ~(line - 1); p < end; p += line)
		__asm__ volatile("dc civac, %0" : : "r"(p) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");
}

/* ---- watchdog ----
 * sboot arms Samsung's watchdog before it hands over (watchdog_cl0@10050000
 * and watchdog_cl2@10060000, timeout-sec = <30> in exynos9830.dts:1990-2012,
 * layout in drivers/watchdog/s3c2410_wdt.c:46-56: WTCON bit 5 enables the
 * timer, bit 0 the reset).  Nothing in the port pets it, so every boot is
 * reset after the timeout - which looks exactly like the logo loop.  Log
 * the state of both controllers and turn them off for the bring-up, so a
 * hang stays a hang and the pmsg log survives.
 */
#define WDT_CL0			0x10050000UL
#define WDT_CL2			0x10060000UL
#define WDT_WTCON		0x00
#define WDT_WTDAT		0x04
#define WDT_WTCNT		0x08
#define WTCON_ENABLE		(1u << 5)

static void wdt_probe(const char *tag, uintptr_t base)
{
	uint32_t con = *(volatile uint32_t *)(base + WDT_WTCON);
	uint32_t dat = *(volatile uint32_t *)(base + WDT_WTDAT);
	uint32_t cnt = *(volatile uint32_t *)(base + WDT_WTCNT);

	kv(tag, con);
	kv("dat", dat);
	kv("cnt", cnt);
	out("\n");
	if (con & WTCON_ENABLE) {
		*(volatile uint32_t *)(base + WDT_WTCON) = 0;
		out("watchdog disabled: ");
		out(tag);
		out("\n");
	}
}

static int magic_ok(const volatile char *m)
{
	const char *want = "SWEETXNU";

	for (int i = 0; i < 8; i++)
		if (m[i] != want[i])
			return 0;
	return 1;
}

static uint64_t fail(const char *why)
{
	out("FAIL: ");
	out(why);
	out("\n");
	return 0;
}

/* ---- payload lookup ----
 * The ramdisk sits at the address the boot image header asks for, but a
 * bootloader is free to relocate it, so the whole boot region is scanned
 * for the container magic as a fallback. */
static uintptr_t find_payload(void)
{
	uintptr_t p = (uintptr_t)RAMDISK_LOAD_ADDR;

	if (magic_ok((const volatile char *)p))
		return p;
	for (p = (uintptr_t)BOOT_SCAN_START; p < (uintptr_t)BOOT_SCAN_END; p += 16)
		if (magic_ok((const volatile char *)p))
			return p;
	return 0;
}

/* A DTB handed over in x0 would let us read /chosen/linux,initrd-start;
 * sboot on Exynos does not pass one, but the loader checks anyway. */
static int fdt_ok(uintptr_t dtb)
{
	if (dtb < LOW_RAM_BASE || dtb >= LOW_RAM_END)
		return 0;
	return *(volatile uint32_t *)dtb == 0xd00dfeedU;
}

/* ---- what the firmware left us ----
 * The boot core's MPIDR decides which ADT cpu node must be "running", and
 * the GIC type says which interrupt layout the kernel expects.  Both are
 * facts we cannot read out of the device tree, so the loader prints them. */
static void hw_facts(void)
{
	uint64_t midr, mpidr, ctrfrq, typer = 0;

	__asm__ volatile("mrs %0, midr_el1" : "=r"(midr));
	__asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(ctrfrq));
	typer = *(volatile uint32_t *)(GICD_BASE + GICD_TYPER);
	kv("MIDR", midr);
	kv("MPIDR", mpidr);
	out("\n");
	kv("CNTFRQ", ctrfrq);
	kv("GICD_TYPER", typer);
	out("\n");
	/* GICD_TYPER[4:0] + 1 = implemented interrupt lines. */
	kv("irq lines", (uint64_t)((typer & 0x1f) + 1));
	out("\n");
}

uint64_t ex_main(uintptr_t dtb, uint64_t el, uint64_t sctlr)
{
	con_init();
	klog_init();
	out("\nEXYNOS990 LOADER BEGIN (ios-exynos990 exynos990 port)\n");
	kv("EL", el);
	kv("SCTLR", sctlr);
	kv("DTB", dtb);
	out("\n");
	if (el != 1)
		return fail("not entered at EL1");
	wdt_probe("wtcon_cl0", WDT_CL0);
	wdt_probe("wtcon_cl2", WDT_CL2);
	hw_facts();

	if (fdt_ok(dtb)) {
		uint64_t rd_start = 0, rd_end = 0;
		if (fdt_chosen_u64(dtb, "linux,initrd-start", &rd_start) &&
		    fdt_chosen_u64(dtb, "linux,initrd-end", &rd_end))
			kv("dt initrd", rd_start);
		kv("dt end", rd_end);
		out("\n");
	} else {
		out("no DTB in x0 (expected on Exynos)\n");
	}

	uintptr_t rd_start = find_payload();
	if (!rd_start)
		return fail("no SWEETXNU payload in the ramdisk");
	kv("payload", rd_start);
	out("\n");

	const volatile struct payload *p = (const volatile struct payload *)rd_start;
	uint64_t nsegs = p->nsegs;
	kv("segs", nsegs);
	kv("virt", p->virt_base);
	kv("entry", p->entry_va);
	out("\n");
	kv("phys", p->phys_base);
	kv("mem", p->mem_size);
	kv("adt", p->adt_size);
	out("\n");
	if (p->version != 1)
		return fail("payload is not version 1");
	if (!nsegs || nsegs > 32)
		return fail("bad payload segment count");
	if (p->phys_base < XNU_RAM_BASE || p->phys_base >= XNU_RAM_END || !p->mem_size ||
	    p->mem_size > XNU_RAM_END - p->phys_base)
		return fail("payload RAM outside the high bank");
	if (p->entry_va < p->virt_base || p->kernel_end < p->virt_base)
		return fail("bad payload VA layout");

	/* The container size is unknown (no length field in v1), so the
	 * segment table bounds are checked against the ramdisk window. */
	uint64_t rd_end = rd_start + 0x4000000UL;	/* 64 MiB */
	if (rd_end > (uintptr_t)BOOT_SCAN_END)
		rd_end = (uintptr_t)BOOT_SCAN_END;
	uint64_t length = rd_end - rd_start;
	if (__builtin_offsetof(struct payload, segs) + nsegs * sizeof(struct payload_seg) > length ||
	    p->adt_off > length || p->adt_size > length - p->adt_off)
		return fail("bad payload header");

	uintptr_t mem_end = p->phys_base + p->mem_size;
	uintptr_t kernel_end = p->phys_base + (p->kernel_end - p->virt_base);
	if (kernel_end >= mem_end)
		return fail("kernel larger than the RAM window");

	/* Read-only UFS probe before the RAM window is wiped: it reports the
	 * host state sboot left, reads the GPT and prints the USERDATA, BOOT
	 * and SYSTEM extents.  The scratch area is inside the XNU window, so
	 * it is zeroed right afterwards. */
	ufs_probe(kv, out, UFS_DMA_BASE);

	/* The bootloader programs the display before it hands over (the
	 * Android DPU driver calls this "LCD_ON_UBOOT"), so the DECON
	 * registers describe the panel of this model.  Read them: the window
	 * geometry, the image size and the address of the buffer the
	 * bootloader draws into.  Read-only. */
	struct decon_fb fb;
	decon_probe(kv, out, &fb);

	/* sboot leaves the previous OS's data in RAM (Android, or a previous
	 * loader run); iBoot hands XNU zeroed memory, so zero it here - but
	 * keep the buffer the display is scanning out, if it lives inside
	 * the window: zeroing it would blank the panel. */
	out("zeroing XNU RAM...\n");
	uintptr_t fb_lo = 0, fb_hi = 0;
	if (fb.found && fb.stride && fb.height) {
		fb_lo = fb.phys;
		fb_hi = fb_lo + (uint64_t)fb.stride * fb.height;
	}
	if (fb_hi > fb_lo && fb_lo < mem_end && fb_hi > p->phys_base) {
		uintptr_t lo = fb_lo < p->phys_base ? p->phys_base : fb_lo;
		uintptr_t hi = fb_hi > mem_end ? mem_end : fb_hi;
		zero(p->phys_base, lo - p->phys_base);
		zero(hi, mem_end - hi);
		out("kept the display buffer ");
		hex(lo);
		out(" ");
		hex(hi - lo);
		out("\n");
	} else {
		zero(p->phys_base, p->mem_size);
	}
	out("zeroed\n");

	for (uint64_t i = 0; i < nsegs; i++) {
		const volatile struct payload_seg *s = &p->segs[i];
		if (s->vmaddr < p->virt_base || s->file_size > s->vmsize ||
		    s->file_off > length || s->file_size > length - s->file_off ||
		    s->vmsize > p->mem_size || s->vmaddr - p->virt_base > p->mem_size - s->vmsize)
			return fail("bad segment");
		uintptr_t dst = p->phys_base + (s->vmaddr - p->virt_base);
		copy(dst, rd_start + s->file_off, s->file_size);
		zero(dst + s->file_size, s->vmsize - s->file_size);
		clean(dst, dst + s->vmsize);
		out("seg ");
		hex(i);
		out(" -> ");
		hex(dst);
		out(" ");
		hex(s->vmsize);
		out("\n");
	}

	uintptr_t adt_pa = kernel_end;
	uintptr_t ba_pa = (adt_pa + p->adt_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	if (ba_pa + PAGE_SIZE > mem_end)
		return fail("no room for ADT/boot_args");
	copy(adt_pa, rd_start + p->adt_off, p->adt_size);
	zero(ba_pa, PAGE_SIZE);

	volatile struct boot_args *ba = (volatile struct boot_args *)ba_pa;
	ba->Revision = 2;
	ba->Version = 2;
	ba->virtBase = p->virt_base;
	ba->physBase = p->phys_base;
	ba->memSize = p->mem_size;
	ba->topOfKernelData = ba_pa + PAGE_SIZE;
	ba->deviceTreeP = p->virt_base + (adt_pa - p->phys_base);
	ba->deviceTreeLength = (uint32_t)p->adt_size;
	for (int i = 0; i < BOOT_LINE_LENGTH; i++)
		ba->CommandLine[i] = p->cmdline[i];
	ba->memSizeActual = p->mem_size;

	/* The display buffer the bootloader left, handed to XNU the way iBoot
	 * does it: PE_current_console() turns boot_args.Video into the console
	 * framebuffer.  Only the 32-bit XRGB/ARGB formats can be used as a
	 * pixel buffer by the console (the data path format the bootloader
	 * chose is printed by the probe as "format"). */
	if (fb.found && fb.width && fb.height && fb.stride) {
		ba->Video.v_baseAddr = fb.phys;
		ba->Video.v_display = 1;	        /* graphics */
		ba->Video.v_rowBytes = fb.stride;
		ba->Video.v_width = fb.width;
		ba->Video.v_height = fb.height;
		ba->Video.v_depth = 32;               /* depth 32, rotate 0, scale 1x */
		kv("fb", fb.phys);
		kv("fb_width", fb.width);
		kv("fb_height", fb.height);
		kv("fb_stride", fb.stride);
	} else {
		out("no display buffer found: XNU boots without a framebuffer\n");
	}
	clean(adt_pa, ba_pa + PAGE_SIZE);

	uintptr_t entry_pa = p->phys_base + (p->entry_va - p->virt_base);
	if (entry_pa >= kernel_end)
		return fail("entry outside the kernel");
	ex_boot_args = ba_pa;
	kv("adt_pa", adt_pa);
	kv("boot_args", ba_pa);
	out("\n");
	kv("entry_pa", entry_pa);
	out("\ncmdline: ");
	for (int i = 0; i < BOOT_LINE_LENGTH && p->cmdline[i]; i++) {
		char c[2] = { p->cmdline[i], 0 };
		out(c);
	}
	out("\n\nJUMP TO XNU\n");
	clean(p->phys_base, mem_end);
	__asm__ volatile("dsb sy" ::: "memory");
	return entry_pa;
}

void ex_exception(uint64_t slot)
{
	uint64_t esr, elr, far;
	__asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
	__asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
	__asm__ volatile("mrs %0, far_el1" : "=r"(far));
	out("\nLOADER EXCEPTION ");
	kv("slot", slot);
	kv("ESR", esr);
	kv("ELR", elr);
	kv("FAR", far);
	out("\n");
	for (;;)
		__asm__ volatile("wfe");
}

/* gcc may emit calls for struct copies; keep the image self-contained. */
void *memset(void *d, int c, unsigned long n)
{
	unsigned char *q = d;

	while (n--)
		*q++ = (unsigned char)c;
	return d;
}

void *memcpy(void *d, const void *s, unsigned long n)
{
	unsigned char *q = d;
	const unsigned char *r = s;

	while (n--)
		*q++ = *r++;
	return d;
}
