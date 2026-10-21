#!/usr/bin/env python3
"""Pack the Exynos 990 loader and its XNU payload into an Android boot image.

  mkboot_exynos990.py --kernel Image --ramdisk payload.bin --out boot.img

The Exynos 990 phone boots the Android "boot" partition through Samsung's
sboot, which parses an Android boot image (magic "ANDROID!").  This script
writes that image; it is the "boot"-partition counterpart of
tools/dreamlte_mkboot.py, which patches the raw Samsung BOOT-partition image
the Galaxy S8 port uses.

sboot contract (the defaults below):
  base            0x10000000
  kernel          base + 0x8000    -> 0x10008000
  ramdisk         base + 0x1000000  -> 0x11000000
  page size       4096

The kernel is the loader Image built by shim/exynos990/build.sh, the ramdisk
is the "SWEETXNU" payload from tools/mkpayload.py.  The payload is stored
uncompressed, so the loader finds the container by scanning the ramdisk
region sboot loaded it into.  The device tree of the phone is not part of
this image: Samsung's sboot passes no DTB pointer to the kernel on Exynos,
so the loader does not depend on one.

Header layout (AOSP system/core/mkbootimg/include/bootimg.h, struct
boot_img_hdr_v1, packed, 1648 bytes in the first page):

  0x000  char   magic[8] = "ANDROID!"
  0x008  u32    kernel_size, kernel_addr
  0x010  u32    ramdisk_size, ramdisk_addr
  0x018  u32    second_size, second_addr
  0x020  u32    tags_addr, page_size
  0x028  u32    header_version, os_version
  0x030  char   name[16]
  0x040  char   cmdline[512]
  0x240  u32    id[8]              (32 bytes, SHA-1 zero padded)
  0x260  char   extra_cmdline[1024]
  0x660  u32    recovery_dtbo_size
  0x664  u64    recovery_dtbo_offset
  0x66c  u32    header_size

header_version stays 0, the legacy header sboot expects; sboot therefore
ignores the recovery_* fields, and header_size only documents how many bytes
this script wrote.  The id field follows the old AOSP mkbootimg scheme: a
SHA-1 over the kernel bytes, the kernel size (little endian u32) and the NUL
terminated product name.  Nothing on the device verifies it.

--dtb appends a device tree blob after the ramdisk and records it in
second_size/second_addr (the v0 "second" slot).  It is only there to keep a
reference DTB inside the image; the loader does not use it.
"""
import argparse
import hashlib
import struct
from pathlib import Path

BOOT_MAGIC = b"ANDROID!"
HEADER_SIZE = 1648			# AOSP boot_img_hdr_v1, packed
NAME_SIZE = 16
CMDLINE_SIZE = 512
EXTRA_CMDLINE_SIZE = 1024
ID_SIZE = 32				# uint32_t id[8]

OFF_MAGIC = 0x000
OFF_FIELDS = 0x008			# 10 x uint32
OFF_NAME = 0x030
OFF_CMDLINE = 0x040
OFF_ID = 0x240
OFF_EXTRA_CMDLINE = 0x260
OFF_RECOVERY_DTBO_SIZE = 0x660
OFF_RECOVERY_DTBO_OFFSET = 0x664
OFF_HEADER_SIZE = 0x66c

HEADER_VERSION = 0			# the legacy header Samsung's sboot expects
DEFAULT_BASE = 0x10000000
DEFAULT_KERNEL_OFFSET = 0x8000
DEFAULT_RAMDISK_OFFSET = 0x1000000
DEFAULT_PAGE_SIZE = 4096
DEFAULT_DTB_OFFSET = 0x2000000		# unused by the loader, only recorded


def auto_int(value):
    """Parse an integer written in decimal or with a 0x prefix."""
    return int(value, 0)


def align_up(size, page):
    """Round a size up to a whole number of pages."""
    return (size + page - 1) // page * page


def put_bytes(buf, off, data):
    """Write data at off; the buffer keeps its size and stays zero padded."""
    buf[off:off + len(data)] = data


def split_cmdline(cmdline):
    """Split a command line into the header's cmdline and extra_cmdline fields."""
    data = cmdline.encode()
    if len(data) > CMDLINE_SIZE + EXTRA_CMDLINE_SIZE - 2:
        raise ValueError('command line longer than cmdline + extra_cmdline')
    return data[:CMDLINE_SIZE - 1], data[CMDLINE_SIZE - 1:][:EXTRA_CMDLINE_SIZE - 1]


def image_id(kernel, name):
    """SHA-1 over the kernel, its size and the NUL terminated name (id field)."""
    digest = hashlib.sha1()
    digest.update(kernel)
    digest.update(struct.pack('<I', len(kernel)))
    digest.update(name)
    raw = digest.digest()
    return raw + b'\0' * (ID_SIZE - len(raw))


def build_image(kernel, ramdisk, dtb, name, cmdline, base, kernel_offset,
                ramdisk_offset, page):
    """Return (boot image bytes, header info) for the given parts."""
    if not kernel or not ramdisk:
        raise ValueError('kernel and ramdisk must not be empty')
    if page < HEADER_SIZE:
        raise ValueError(f'page size {page} is smaller than the {HEADER_SIZE}-byte header')
    if len(name) >= NAME_SIZE:
        raise ValueError(f'name too long: {len(name)} bytes, at most {NAME_SIZE - 1}')
    name_field = name.encode() + b'\0' * (NAME_SIZE - len(name))
    main_cmd, extra_cmd = split_cmdline(cmdline)

    kernel_addr = base + kernel_offset
    ramdisk_addr = base + ramdisk_offset
    second_size = len(dtb) if dtb else 0
    second_addr = base + DEFAULT_DTB_OFFSET if dtb else 0

    hdr = bytearray(HEADER_SIZE)
    struct.pack_into('<8s', hdr, OFF_MAGIC, BOOT_MAGIC)
    struct.pack_into('<10I', hdr, OFF_FIELDS,
                     len(kernel), kernel_addr,
                     len(ramdisk), ramdisk_addr,
                     second_size, second_addr,
                     0,				# tags_addr: Exynos has no tags
                     page,
                     HEADER_VERSION,
                     0)				# os_version: an Android field, unused here
    hdr[OFF_NAME:OFF_NAME + NAME_SIZE] = name_field
    put_bytes(hdr, OFF_CMDLINE, main_cmd)
    put_bytes(hdr, OFF_ID, image_id(kernel, name.encode() + b'\0'))
    put_bytes(hdr, OFF_EXTRA_CMDLINE, extra_cmd)
    struct.pack_into('<I', hdr, OFF_RECOVERY_DTBO_SIZE, 0)
    struct.pack_into('<Q', hdr, OFF_RECOVERY_DTBO_OFFSET, 0)
    struct.pack_into('<I', hdr, OFF_HEADER_SIZE, HEADER_SIZE)

    image = bytearray(hdr)
    image += b'\0' * (page - len(image))
    for part in (kernel, ramdisk, dtb):
        image += part
        image += b'\0' * (align_up(len(part), page) - len(part))

    info = dict(kernel_size=len(kernel), kernel_addr=kernel_addr,
                ramdisk_size=len(ramdisk), ramdisk_addr=ramdisk_addr,
                second_size=second_size, second_addr=second_addr,
                page_size=page, name=name, cmdline=cmdline,
                id=bytes(hdr[OFF_ID:OFF_ID + ID_SIZE]))
    return bytes(image), info


def parse_header(blob):
    """Parse a boot image header into a dict of fields."""
    if blob[:8] != BOOT_MAGIC:
        raise ValueError('not an Android boot image')
    fields = struct.unpack_from('<10I', blob, OFF_FIELDS)
    return dict(
        kernel_size=fields[0], kernel_addr=fields[1],
        ramdisk_size=fields[2], ramdisk_addr=fields[3],
        second_size=fields[4], second_addr=fields[5],
        tags_addr=fields[6], page_size=fields[7],
        header_version=fields[8], os_version=fields[9],
        name=blob[OFF_NAME:OFF_NAME + NAME_SIZE].split(b'\0')[0].decode(),
        cmdline=blob[OFF_CMDLINE:OFF_CMDLINE + CMDLINE_SIZE].split(b'\0')[0].decode(),
        extra_cmdline=blob[OFF_EXTRA_CMDLINE:OFF_EXTRA_CMDLINE + EXTRA_CMDLINE_SIZE].split(b'\0')[0].decode(),
        id=blob[OFF_ID:OFF_ID + ID_SIZE],
        header_size=struct.unpack_from('<I', blob, OFF_HEADER_SIZE)[0])


def unpack_image(blob):
    """Split a boot image back into (header info, kernel, ramdisk, second)."""
    info = parse_header(blob)
    page = info['page_size']
    off = page
    kernel = blob[off:off + info['kernel_size']]
    off += align_up(info['kernel_size'], page)
    ramdisk = blob[off:off + info['ramdisk_size']]
    off += align_up(info['ramdisk_size'], page)
    second = blob[off:off + info['second_size']]
    return info, kernel, ramdisk, second


def self_check(image, kernel, ramdisk, dtb):
    """Unpack the image just written and compare it with the parts."""
    info, got_kernel, got_ramdisk, got_second = unpack_image(image)
    if got_kernel != kernel:
        raise SystemExit('self check failed: kernel does not round trip')
    if got_ramdisk != ramdisk:
        raise SystemExit('self check failed: ramdisk does not round trip')
    if got_second != (dtb or b''):
        raise SystemExit('self check failed: second (dtb) does not round trip')
    if info['header_size'] != HEADER_SIZE or info['header_version'] != HEADER_VERSION:
        raise SystemExit('self check failed: unexpected header version or size')
    return info


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--kernel', required=True, type=Path,
                    help='loader Image (shim/exynos990/out/Image)')
    ap.add_argument('--ramdisk', required=True, type=Path,
                    help='SWEETXNU payload (tools/mkpayload.py output)')
    ap.add_argument('--out', required=True, type=Path, help='boot image to write')
    ap.add_argument('--base', type=auto_int, default=DEFAULT_BASE,
                    help=f'load address base (default {DEFAULT_BASE:#x})')
    ap.add_argument('--kernel-offset', type=auto_int, default=DEFAULT_KERNEL_OFFSET,
                    help=f'kernel offset from base (default {DEFAULT_KERNEL_OFFSET:#x})')
    ap.add_argument('--ramdisk-offset', type=auto_int, default=DEFAULT_RAMDISK_OFFSET,
                    help=f'ramdisk offset from base (default {DEFAULT_RAMDISK_OFFSET:#x})')
    ap.add_argument('--page-size', type=auto_int, default=DEFAULT_PAGE_SIZE,
                    help=f'flash page size (default {DEFAULT_PAGE_SIZE})')
    ap.add_argument('--dtb', type=Path,
                    help='optional DTB appended after the ramdisk (recorded as "second")')
    ap.add_argument('--cmdline', default='',
                    help='kernel command line stored in the header')
    ap.add_argument('--name', default='exynos990',
                    help='product name in the header (at most 15 characters)')
    args = ap.parse_args()

    kernel = args.kernel.read_bytes()
    ramdisk = args.ramdisk.read_bytes()
    dtb = args.dtb.read_bytes() if args.dtb else b''

    try:
        image, info = build_image(kernel, ramdisk, dtb, args.name, args.cmdline,
                                  args.base, args.kernel_offset, args.ramdisk_offset,
                                  args.page_size)
    except ValueError as exc:
        raise SystemExit(f'error: {exc}')
    self_check(image, kernel, ramdisk, dtb)
    args.out.write_bytes(image)

    print(f'{args.out}: {len(image)} bytes')
    print(f'  header    : version {HEADER_VERSION}, {HEADER_SIZE} bytes in a {args.page_size}-byte page')
    print(f'  kernel    : {info["kernel_size"]} bytes @ {info["kernel_addr"]:#x}')
    print(f'  ramdisk   : {info["ramdisk_size"]} bytes @ {info["ramdisk_addr"]:#x}')
    if info['second_size']:
        print(f'  dtb       : {info["second_size"]} bytes @ {info["second_addr"]:#x} (second)')
    print(f'  name      : {info["name"]}')
    print(f'  cmdline   : {info["cmdline"] or "(empty)"}')
    print(f'  id        : {info["id"].hex()}')
    print(f'  sha256    : {hashlib.sha256(image).hexdigest()}')


if __name__ == '__main__':
    main()
