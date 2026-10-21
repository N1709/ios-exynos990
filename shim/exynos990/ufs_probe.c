/* SPDX-License-Identifier: GPL-2.0-only
 * Exynos 990 UFS read-only probe, run by the loader before XNU (MMU off).
 *
 * S-Boot read BOOT from this host, so the link is up. Like
 * bsd/dev/sweet_ufs.c we reuse that state: point the transfer request list
 * at our own memory and issue polled SCSI commands to LUN 0.
 *
 * Exynos 990 (universal9830) differences, from the Android kernel tree
 * (drivers/scsi/ufs/ufs-exynos.c, exynos_ufs_set_features() and
 * exynos_ufs_init_host()):
 *   - UFSHCD_QUIRK_PRDT_BYTE_GRAN: UTRD offsets/lengths are in bytes and
 *     the PRDT length is entries * entry size (the S8 port had the same
 *     behaviour under UFSHCD_QUIRK_BROKEN_DWORD_UTRD);
 *   - UFSHCD_QUIRK_BROKEN_REQ_LIST_CLR: UTRLCLR is inverted;
 *   - each PRDT entry covers at most 4 KiB (PRDT_SET_SIZE(12) in
 *     HCI_TXPRDT_ENTRY_SIZE);
 *   - HCI_UTRL_NEXUS_TYPE bit = 1 marks a slot as a SCSI command;
 *   - PRDT entry stride is 16 bytes, or 128 with FMP (UFSP UFSPRSECURITY
 *     DESCTYPE 3, set by the secure monitor).
 * Only TEST UNIT READY and READ(10) are issued. Nothing is written.
 *
 * Register bases: exynos9830.dts "ufs: ufs@0x13100000" (HCI standard
 * block), 0x13101100 (Exynos vendor block) and 0x132A0000 (protector).
 */
#include <stdint.h>
#include "ufs_probe.h"

#define HCI		0x13100000UL	/* UFSHCI standard registers */
#define VS_HCI		0x13101100UL	/* Exynos vendor HCI registers */
#define UFSP		0x132A0000UL	/* UFS protector (FMP/SMU) */

#define CAP		0x00
#define VER		0x08
#define IS		0x20
#define IE		0x24
#define HCS		0x30
#define HCE		0x34
#define UTRLBA		0x50
#define UTRLBAU		0x54
#define UTRLDBR		0x58
#define UTRLRSR		0x60
#define TXPRDT_ENTRY	0x00
#define RXPRDT_ENTRY	0x04
#define UTRL_NEXUS	0x40
#define UFSPRSECURITY	0x10

#define TIMER_HZ	26000000UL

static inline uint32_t rd(uintptr_t a) { return *(volatile uint32_t *)a; }
static inline void wr(uintptr_t a, uint32_t v) { *(volatile uint32_t *)a = v; }

static uint64_t ticks(void)
{
	uint64_t v;
	__asm__ volatile("isb; mrs %0, cntpct_el0" : "=r"(v));
	return v;
}

/* DMA layout (MMU off: Device memory, no cache maintenance needed). */
#define D_UTRL		0x0000
#define D_UCD		0x1000		/* cmd 512 | resp 512 | PRDT at +1024 */
#define D_DATA		0x4000		/* 64 KiB */
#define D_SIZE		0x14000

static uintptr_t dma;
static uint32_t prdt_stride = 16;
static uint8_t tag;
static uint8_t cmd_lun;		/* UPIU LUN of the next command */

static void bzero_dev(uintptr_t a, uint32_t n)
{
	for (uint32_t i = 0; i < n; i += 4)
		wr(a + i, 0);
}

static uint8_t b8(uintptr_t a)
{
	return *(volatile uint8_t *)a;
}

/* Sense data of the last response UPIU (fixed format, after 2-byte length
 * at offset 30): key | ASC | ASCQ, plus the response's header word. */
static uint64_t last_sense(void)
{
	uintptr_t r = dma + D_UCD + 512;
	uint32_t len = (uint32_t)b8(r + 30) << 8 | b8(r + 31);
	return (uint64_t)len << 32 | (uint32_t)(b8(r + 32 + 2) & 0xf) << 16 |
	       (uint32_t)b8(r + 32 + 12) << 8 | b8(r + 32 + 13);
}

/* One SCSI command in slot 0; data from the device into D_DATA. */
static int scsi(const uint8_t *cdb, unsigned cdb_len, uint32_t len, uint32_t *ocs_out)
{
	uintptr_t utrd = dma + D_UTRL, ucd = dma + D_UCD;
	uint64_t ucd_pa = ucd, data_pa = dma + D_DATA;

	bzero_dev(utrd, 32);
	bzero_dev(ucd, 1024 + 16 * 128);
	/* COMMAND UPIU */
	*(volatile uint8_t *)(ucd + 0) = 0x01;
	*(volatile uint8_t *)(ucd + 1) = len ? 0x40 : 0x00;	/* R: data in */
	*(volatile uint8_t *)(ucd + 2) = cmd_lun;
	*(volatile uint8_t *)(ucd + 3) = ++tag;
	*(volatile uint8_t *)(ucd + 12) = (uint8_t)(len >> 24);
	*(volatile uint8_t *)(ucd + 13) = (uint8_t)(len >> 16);
	*(volatile uint8_t *)(ucd + 14) = (uint8_t)(len >> 8);
	*(volatile uint8_t *)(ucd + 15) = (uint8_t)len;
	for (unsigned i = 0; i < cdb_len; i++)
		*(volatile uint8_t *)(ucd + 16 + i) = cdb[i];

	uint32_t n = 0;
	for (uint32_t done = 0; done < len; done += 4096, n++) {
		uint32_t chunk = len - done < 4096 ? len - done : 4096;
		uintptr_t e = ucd + 1024 + n * prdt_stride;
		wr(e + 0, (uint32_t)(data_pa + done));
		wr(e + 4, (uint32_t)((data_pa + done) >> 32));
		wr(e + 12, chunk - 1);
	}
	/* CT=1 (UFS storage), DD=2 (device to memory) when reading, I=1. */
	wr(utrd + 0, (1u << 28) | ((len ? 2u : 0u) << 25) | (1u << 24));
	wr(utrd + 8, 0x0f);					/* OCS: invalid until done */
	wr(utrd + 16, (uint32_t)ucd_pa);
	wr(utrd + 20, (uint32_t)(ucd_pa >> 32));
	wr(utrd + 24, (512u << 16) | 512u);			/* resp offset | length, bytes */
	wr(utrd + 28, (1024u << 16) | (n * prdt_stride));	/* PRDT offset | length, bytes */
	__asm__ volatile("dsb sy" ::: "memory");

	wr(VS_HCI + UTRL_NEXUS, rd(VS_HCI + UTRL_NEXUS) | 1);	/* slot 0 = SCSI command */
	wr(HCI + UTRLDBR, 1);
	uint64_t end = ticks() + TIMER_HZ * 2;
	while (rd(HCI + UTRLDBR) & 1) {
		if (ticks() > end) {
			*ocs_out = 0xdead;
			return -1;
		}
	}
	wr(HCI + IS, rd(HCI + IS));
	uint32_t ocs = rd(utrd + 8) & 0xff;
	*ocs_out = ocs | (uint32_t)b8(ucd + 512) << 8 | (uint32_t)b8(ucd + 512 + 6) << 16 |
		   (uint32_t)b8(ucd + 512 + 7) << 24;
	if (ocs || b8(ucd + 512) != 0x21 || b8(ucd + 512 + 6) || b8(ucd + 512 + 7))
		return -2;
	return 0;
}

static int read_blocks(uint32_t lba, uint32_t count, uint32_t *st)
{
	uint8_t cdb[10] = { 0x28, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16),
			    (uint8_t)(lba >> 8), (uint8_t)lba, 0, 0, (uint8_t)count, 0 };
	return scsi(cdb, 10, count * 4096, st);
}

static int name_is(uintptr_t e, const char *want)
{
	for (int c = 0; c < 36; c++) {
		uint16_t ch = (uint16_t)(b8(e + 56 + 2 * c) | b8(e + 57 + 2 * c) << 8);
		if (ch != (uint8_t)want[c])
			return 0;
		if (!ch)
			return 1;
	}
	return 0;
}

static uint64_t le64(uintptr_t a)
{
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--)
		v = v << 8 | b8(a + i);
	return v;
}

void ufs_probe(void (*kv)(const char *, uint64_t), void (*out)(const char *),
	       uintptr_t dma_base)
{
	out("UFS probe (read-only)\n");
	kv("CAP", rd(HCI + CAP));
	kv("VER", rd(HCI + VER));
	kv("HCS", rd(HCI + HCS));
	kv("HCE", rd(HCI + HCE));
	out("\n");
	kv("UTRLBA", (uint64_t)rd(HCI + UTRLBAU) << 32 | rd(HCI + UTRLBA));
	kv("DBR", rd(HCI + UTRLDBR));
	kv("RSR", rd(HCI + UTRLRSR));
	kv("IS", rd(HCI + IS));
	kv("IE", rd(HCI + IE));
	out("\n");
	kv("TXPRDT", rd(VS_HCI + TXPRDT_ENTRY));
	kv("RXPRDT", rd(VS_HCI + RXPRDT_ENTRY));
	kv("NEXUS", rd(VS_HCI + UTRL_NEXUS));
	uint32_t sec = rd(UFSP + UFSPRSECURITY);
	kv("UFSPRSEC", sec);
	out("\n");
	prdt_stride = ((sec >> 19) & 3) == 3 ? 128 : 16;
	kv("prdt_stride", prdt_stride);

	/* Readiness is HCS DP+UTRLRDY.  The S8 needed this because its
	 * driver had UFSHCD_QUIRK_USE_OF_HCE (HCE reads 0 while the host
	 * runs); the 9830 driver does not set that quirk, so HCE is printed
	 * for comparison but not used as the gate. */
	if ((rd(HCI + HCS) & 3) != 3) {
		out("UFS: host not ready, skipping commands\n");
		return;
	}
	if (rd(HCI + UTRLDBR)) {
		out("UFS: bootloader requests outstanding, skipping\n");
		return;
	}
	dma = dma_base;
	if ((dma_base >> 32) && !(rd(HCI + CAP) & (1u << 24))) {
		out("UFS: no 64-bit DMA, skipping\n");
		return;
	}
	bzero_dev(dma, D_SIZE);
	wr(HCI + IE, 0);
	wr(HCI + IS, rd(HCI + IS));
	wr(HCI + UTRLRSR, 0);
	wr(HCI + UTRLBA, (uint32_t)(dma + D_UTRL));
	wr(HCI + UTRLBAU, (uint32_t)((dma + D_UTRL) >> 32));
	wr(HCI + UTRLRSR, 1);
	kv("ours", (uint64_t)rd(HCI + UTRLBAU) << 32 | rd(HCI + UTRLBA));
	kv("RSR", rd(HCI + UTRLRSR));
	out("\n");
	if (rd(HCI + UTRLBA) != (uint32_t)(dma + D_UTRL) || !(rd(HCI + UTRLRSR) & 1)) {
		out("UFS: host did not take our list\n");
		return;
	}

	static const uint8_t tur[6] = { 0 };
	uint32_t st = 0;
	int rc = -1;
	for (int i = 0; i < 3 && rc; i++) {
		rc = scsi(tur, 6, 0, &st);
		kv("TUR rc", (uint64_t)(int64_t)rc);
		kv("st", st);
		kv("sense(len|key|asc|ascq)", last_sense());
		out("\n");
	}
	/* REQUEST SENSE (18 bytes): the device's view of what is wrong. */
	static const uint8_t rs[6] = { 0x03, 0, 0, 0, 18, 0 };
	rc = scsi(rs, 6, 18, &st);
	kv("REQSENSE rc", (uint64_t)(int64_t)rc);
	kv("st", st);
	{
		uintptr_t d = dma + D_DATA;
		kv("data key", b8(d + 2) & 0xf);
		kv("asc", b8(d + 12));
		kv("ascq", b8(d + 13));
	}
	out("\n");
	/*
	 * Sense NOT READY / 04h: S-Boot left the LU stopped. Linux ufshcd wakes
	 * it with START STOP UNIT. Try START=1 on LUN 0, then power condition
	 * ACTIVE (byte 4 = 1 << 4) on the UFS Device well-known LUN (0xD0).
	 */
	static const uint8_t ssu_start[6] = { 0x1b, 0, 0, 0, 0x01, 0 };
	static const uint8_t ssu_active[6] = { 0x1b, 0, 0, 0, 0x10, 0 };
	for (int step = 0; step < 2; step++) {
		cmd_lun = step ? 0xd0 : 0;
		rc = scsi(step ? ssu_active : ssu_start, 6, 0, &st);
		kv(step ? "SSU wlun ACTIVE rc" : "SSU lun0 START rc", (uint64_t)(int64_t)rc);
		kv("st", st);
		kv("sense", last_sense());
		cmd_lun = 0;
		rc = scsi(tur, 6, 0, &st);
		kv("TUR rc", (uint64_t)(int64_t)rc);
		kv("st", st);
		out("\n");
		if (!rc)
			break;
		scsi(rs, 6, 18, &st);
		uintptr_t d = dma + D_DATA;
		kv("sense key", b8(d + 2) & 0xf);
		kv("asc", b8(d + 12));
		kv("ascq", b8(d + 13));
		out("\n");
	}
	for (int attempt = 0; attempt < 2; attempt++) {
		rc = read_blocks(1, 1, &st);
		kv("READ lba1 rc", (uint64_t)(int64_t)rc);
		kv("st", st);
		kv("sense", last_sense());
		kv("stride", prdt_stride);
		out("\n");
		if (!rc)
			break;
		prdt_stride = prdt_stride == 16 ? 128 : 16;	/* try the other layout */
	}
	if (rc)
		return;
	uintptr_t d = dma + D_DATA;
	char sig[9];
	for (int i = 0; i < 8; i++)
		sig[i] = (char)b8(d + i);
	sig[8] = 0;
	out("GPT sig: ");
	out(sig);
	out("\n");
	uint64_t entries = le64(d + 72);
	uint32_t nent = (uint32_t)le64(d + 80) & 0xffffffff;
	kv("entries_lba", entries);
	kv("n", nent);
	out("\n");
	if (nent > 128 || entries > 64)
		return;
	rc = read_blocks((uint32_t)entries, 4, &st);	/* 128 entries x 128 bytes = 16 KiB */
	if (rc) {
		kv("READ entries rc", (uint64_t)(int64_t)rc);
		kv("st", st);
		out("\n");
		return;
	}
	for (uint32_t i = 0; i < nent; i++) {
		uintptr_t e = d + i * 128;
		if (name_is(e, "USERDATA") || name_is(e, "BOOT") || name_is(e, "SYSTEM")) {
			char nm[10];
			int c;
			for (c = 0; c < 9 && b8(e + 56 + 2 * c); c++)
				nm[c] = (char)b8(e + 56 + 2 * c);
			nm[c] = 0;
			out(nm);
			out(": ");
			kv("first", le64(e + 32));
			kv("last", le64(e + 40));
			out("\n");
		}
	}
	out("UFS probe done\n");
}
