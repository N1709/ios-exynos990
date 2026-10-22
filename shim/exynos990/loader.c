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

/* ---- visible marker ----
 * Every log sink has lost the race so far, and reading one needs a cable,
 * adb and luck.  The panel, on the other hand, is still scanning out the
 * bootloader's framebuffer while the loader runs: painting a band across the
 * top of it answers "did the loader execute?" by eye, with nothing attached.
 *
 *   green  = the loader ran and got as far as building boot_args
 *   blue   = it got past the payload copy and is about to jump to XNU
 *
 * The write only happens when the probe found a buffer that looks sane (a
 * 32-bit address inside the low 3 GiB of DRAM, smaller than 64 MiB), so a
 * bogus DECON register cannot turn this into a data abort.
 */
static void paint_band(const struct decon_fb *fb, uint32_t argb)
{
	if (!fb->found || !fb->width || !fb->height || fb->stride < 64)
		return;
	if (fb->phys < 0x80000000u || fb->phys >= 0xc0000000u)
		return;
	if ((uint64_t)fb->stride * fb->height > 64u * 1024u * 1024u)
		return;

	uint32_t stride = fb->stride / 4;	/* pixels per line, 32 bpp */
	uint32_t lines = fb->height < 160u ? fb->height : 160u;
	volatile uint32_t *p = (volatile uint32_t *)fb->phys;

	for (uint32_t y = 0; y < lines; y++)
		for (uint32_t x = 0; x < stride; x++)
			p[(uint64_t)y * stride + x] = argb;

	__asm__ volatile("dsb sy" ::: "memory");
}

/* ---- durable sink ----
 * TWRP fills the 16 KiB pmsg ring within seconds of starting (the ring is
 * already full when adbd appears), so a failed boot's text cannot be read
 * from there.  The pstore area has a second 16 KiB zone in front of it: the
 * ramoops dump zone at 0xeb308000 (ramoops attaches 0x8000@0xeb308000 and
 * splits it into record_size 16384 + pmsg_size 16384, see the phone's
 * /sys/module/ramoops/parameters).  Nothing writes it on a normal boot -
 * the file /sys/fs/pstore/dmesg-ramoops-0 on this phone was last modified
 * on 2026-10-19 - so the loader writes its log there too and TWRP shows it
 * at leisure:
 *
 *     cat /sys/fs/pstore/dmesg-ramoops-0
 *
 * Format, all little endian (fs/pstore/ram.c:157-199, fs/pstore/ram_core.c):
 *   0x000 persistent_ram header { u32 sig "DBGC", u32 start, u32 size }
 *   0x00c data: "====<sec>.<usec>-D\n" then the text.  The "D" says the
 *         record is not compressed; without a parsable header pstore drops
 *         the record and deletes the zone.
 */
#define DUMP_PHYS		0xeb308000u
#define DUMP_SIZE		0x4000u
#define DUMP_CAP		(DUMP_SIZE - 12u)

static void klog_dump(void)
{
	static const char rec[] = "====0.0-D\n";
	volatile uint32_t *h = (volatile uint32_t *)DUMP_PHYS;
	volatile uint8_t *d = (volatile uint8_t *)(DUMP_PHYS + 12);
	uint32_t n = 0;

	for (const char *s = rec; *s; s++)
		d[n++] = (uint8_t)*s;

	uint32_t start = (klog_len == KLOG_CAP) ? klog_off : 0;
	for (uint32_t i = 0; i < klog_len && n + 2 < DUMP_CAP; i++)
		d[n++] = klog_data[(start + i) % KLOG_CAP];

	d[n++] = '\n';
	h[0] = 0x43474244u;	/* "DBGC" */
	h[1] = 0;		/* start: the record begins at data[0] */
	h[2] = n;		/* size: n bytes of record */

	__asm__ volatile("dsb sy" ::: "memory");
}

/* ---- progress marks ----
 * The loader now runs (sboot accepts the v2 image) but hangs somewhere
 * before klog_dump().  Each mark overwrites the ramoops dump zone with a
 * record naming the last step that was reached, so one boot says exactly
 * where it stops - no UART, no screen, no timing.
 */
static void mark(char c)
{
	static const char rec[] = "====0.0-D\nMARK ";
	volatile uint32_t *h = (volatile uint32_t *)DUMP_PHYS;
	volatile uint8_t *d = (volatile uint8_t *)(DUMP_PHYS + 12);
	uint32_t n = 0;

	for (const char *s = rec; *s; s++)
		d[n++] = (uint8_t)*s;
	d[n++] = (uint8_t)c;
	d[n++] = '\n';
	h[0] = 0x43474244u;	/* "DBGC" */
	h[1] = 0;		/* start */
	h[2] = n;		/* size */
	__asm__ volatile("dsb sy" ::: "memory");
}

/* ---- tiny console helpers ---- */
static void out(const char *s)
{
	int nl = 0;

	con_puts(s);
	while (*s) {
		if (*s == '\n')
			nl = 1;
		klog_putc(*s++);
	}
	/* Mirror every completed line into the ramoops dump zone: when the
	 * loader hangs on a clock-gated block the last line it managed to
	 * print is the only evidence, and it has to be readable after the
	 * fact. */
	if (nl)
		klog_dump();
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
	/* memmove, not memcpy: sboot places the ramdisk *below* the XNU
	 * window (0x88080000 against 0x890000000 here), so the destination
	 * overlaps the tail of the source and a forward copy would overwrite
	 * bytes it has not read yet. */
	if (dst > src && dst < src + n) {
		for (; n; n--)
			((volatile uint8_t *)dst)[n - 1] = ((volatile uint8_t *)src)[n - 1];
		return;
	}
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
	klog_dump();		/* leave the reason where TWRP can read it */
	return 0;
}

/* ---- payload lookup ----
 * The ramdisk sits at the address the boot image header asks for, but a
 * bootloader is free to relocate it, so the whole boot region is scanned
 * for the container magic as a fallback. */
static uintptr_t find_payload(void)
{
	/* Never touch an address outside DRAM: an undecoded address does not
	 * fault here, it stalls the AXI bus and the boot dies silently.  The
	 * header's ramdisk address (0x11000000) is one of those. */
	if (RAMDISK_LOAD_ADDR >= LOW_RAM_BASE && RAMDISK_LOAD_ADDR < LOW_RAM_END &&
	    magic_ok((const volatile char *)RAMDISK_LOAD_ADDR))
		return (uintptr_t)RAMDISK_LOAD_ADDR;

	for (uintptr_t p = (uintptr_t)BOOT_SCAN_START; p < (uintptr_t)BOOT_SCAN_END; p += 16)
		if (magic_ok((const volatile char *)p))
			return p;
	return 0;
}

/* A DTB handed over in x0 would let us read /chosen/linux,initrd-start;
 * sboot on Exynos does not pass one, but the loader checks anyway. */
/* The FDT magic is big endian on the wire: the bytes d0 0d fe ed load as
 * 0xedfe0dd0 on a little-endian core.  Comparing the loaded word with
 * 0xd00dfeed rejected every real device tree, which is why the loader fell
 * through to the payload scan and then hung reading 0x11000000 - an address
 * this SoC does not decode at all. */
static uintptr_t dtb_fdt(uintptr_t dtb)
{
	const volatile uint8_t *b;

	if (dtb < LOW_RAM_BASE || dtb >= LOW_RAM_END)
		return 0;
	b = (const volatile uint8_t *)dtb;
	if (b[0] == 0xd0 && b[1] == 0x0d && b[2] == 0xfe && b[3] == 0xed)
		return dtb;

	/* Samsung packs several FDTs in a table (magic 1e ab b7 d7); sboot
	 * may hand over the table instead of the selected FDT. */
	if (b[0] == 0x1e && b[1] == 0xab && b[2] == 0xb7 && b[3] == 0xd7) {
		volatile uint32_t *w = (volatile uint32_t *)dtb;
		uint32_t eoff = w[5];
		uintptr_t e = dtb + eoff;
		uint32_t off = *(volatile uint32_t *)(e + 4);
		uintptr_t f = dtb + off;

		if (off && f >= LOW_RAM_BASE && f < LOW_RAM_END)
			return f;
	}
	return 0;
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
	mark('0');
	con_init();
	mark('1');
	klog_init();
	mark('2');
	out("\nEXYNOS990 LOADER BEGIN (ios-exynos990 exynos990 port)\n");
	mark('3');
	kv("EL", el);
	kv("SCTLR", sctlr);
	kv("DTB", dtb);
	out("\n");
	mark('4');
	/* CurrentEL[3:2] is the exception level: EL1 reads as 0x4, not 1. */
	if ((el >> 2) != 1)
		return fail("not entered at EL1");
	mark('5');
	/* The watchdog probe used to read 0x10050000/0x10060000 here and the
	 * boot stopped at MARK 4: on this phone an access to a block whose
	 * clock is gated does not fault, it stalls the bus forever.  The
	 * reset loop was sboot rejecting header_version 0, not a watchdog,
	 * so nothing needs to touch those controllers. */
	hw_facts();
	mark('6');
	klog_dump();

	uintptr_t fdt = dtb_fdt(dtb);
	if (fdt) {
		uint64_t rd_start = 0, rd_end = 0;
		kv("fdt", fdt);
		out("\n");
		if (fdt_chosen_u64(fdt, "linux,initrd-start", &rd_start) &&
		    fdt_chosen_u64(fdt, "linux,initrd-end", &rd_end))
			kv("dt initrd", rd_start);
		kv("dt end", rd_end);
		out("\n");
	} else {
		out("no usable FDT in x0\n");
	}

	/* sboot does pass a device tree in x0 (0x8a080000 on this phone) and
	 * it records where it put our ramdisk, so trust it first: the header
	 * address is only a hint once the bootloader relocates things. */
	uintptr_t rd_start = 0;
	if (fdt) {
		uint64_t s = 0;
		if (fdt_chosen_u64(fdt, "linux,initrd-start", &s) && s)
			rd_start = (uintptr_t)s;
	}
	if (!rd_start || !magic_ok((const volatile char *)rd_start))
		rd_start = find_payload();
	if (!rd_start)
		return fail("no SWEETXNU payload in the ramdisk");
	mark('7');
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

	/* The read-only UFS probe is disabled for now: like the watchdog and
	 * the debug UART, the UFS controller may be clock-gated by the time
	 * sboot hands over, and a gated access hangs the bus instead of
	 * faulting.  It only printed diagnostics, so it can come back once
	 * the jump works. */

	/* The bootloader programs the display before it hands over (the
	 * Android DPU driver calls this "LCD_ON_UBOOT"), so the DECON
	 * registers describe the panel of this model.  Read them: the window
	 * geometry, the image size and the address of the buffer the
	 * bootloader draws into.  Read-only. */
	mark('8');
	struct decon_fb fb;
	decon_probe(kv, out, &fb);
	/* First visible sign of life, milliseconds after entry: if this band
	 * never appears the loader did not execute at all. */
	paint_band(&fb, 0xff00ff00u);

	/* No bulk wipe.  sboot leaves the previous OS's data in RAM, but the
	 * VM zeroes every page it hands out and the kernel image, BSS and
	 * boot_args are written explicitly below.  The wipe was also harmful:
	 * the payload sits at 0x88080000 and ends at 0x89066290, so zeroing
	 * the window at 0x890000000 erased the payload's own ADT and segment
	 * table before they were read, and this is where the boot stopped
	 * ("zeroing" was the last line). */
	mark('9');
	mark('9');

	for (uint64_t i = 0; i < nsegs; i++) {
		const volatile struct payload_seg *s = &p->segs[i];
		if (s->vmaddr < p->virt_base || s->file_size > s->vmsize ||
		    s->file_off > length || s->file_size > length - s->file_off ||
		    s->vmsize > p->mem_size || s->vmaddr - p->virt_base > p->mem_size - s->vmsize)
			return fail("bad segment");
		uintptr_t dst = p->phys_base + (s->vmaddr - p->virt_base);
		copy(dst, rd_start + s->file_off, s->file_size);
		zero(dst + s->file_size, s->vmsize - s->file_size);
		out("seg ");
		hex(i);
		out(" -> ");
		hex(dst);
		out(" ");
		hex(s->vmsize);
		out("\n");
	}

	mark('A');
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
	__asm__ volatile("dsb sy" ::: "memory");

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
	mark('B');
	out("\n\nJUMP TO XNU\n");
	klog_dump();
	paint_band(&fb, 0xff0000ffu);	/* blue: about to branch into XNU */
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
