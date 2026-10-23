#!/usr/bin/env python3
"""Check that an EXYNOS990 kernel can run under its own TCR_EL1.

Why this exists
---------------
XNU's arm64 ``start.s`` builds its bootstrap tables with 2 MiB blocks and then
enables the MMU with the TCR value stored in ``_sysreg_restore`` (offset 0,
``tcr_el1``).  The EXYNOS990 kernel is a 4 KiB-page build, so that value has
``T1SZ = 25``: TTBR1 only covers the top 512 GiB of the address space, i.e.
``0xffffff8000000000`` and above.

The development build links the kernel at the macOS ``ARM_LARGE_MEMORY``
address ``0xfffffe0007004000`` unless the board is listed in patch 0093.
That address sits *between* the TTBR0 and TTBR1 windows: ``_start`` and
``common_start`` still run (they use physical addresses and the identity
map), the tables are written correctly, and then the ``ret`` into
``arm_init`` faults, the exception vector fetch faults too, and the CPU
halts with no output at all.  Nothing can be seen from the phone.

Usage
-----
    tools/exynos990-verify-kernel.py KERNEL [--phys-base 0x891004000]

It prints the link base, the TTBR1 window check, the entry point and the two
offsets the loader hard-codes (mark page and bootstrap pagetables), then exits
non-zero when the kernel is outside its own TTBR1 window.
"""
import argparse
import re
import struct
import sys

LC_SEGMENT_64 = 0x19
LC_SYMTAB = 0x2
LC_UNIXTHREAD = 0x5
LC_MAIN = 0x80000028

TTBR1_SHIFT = 39          # 4 KiB pages, TCR_EL1.T1SZ = 25 on this kernel
TTBR1_WINDOW = 1 << TTBR1_SHIFT
TTBR1_START = (1 << 64) - TTBR1_WINDOW

# Offsets the loader depends on, relative to __TEXT (the Mach header).
XMARK_OFF = 0x1488000     # _intstack_low_guard
PT_OFF = 0x172000         # _bootstrap_pagetables

PAYLOAD_MAGIC = b'SWEETXNU'


def parse(path):
    buf = open(path, 'rb').read()
    magic, cputype, cpusub, filetype, ncmds, sizeofcmds, flags, _ = \
        struct.unpack_from('<IiiIIIII', buf, 0)
    if magic != 0xfeedfacf:
        sys.exit(f'{path}: not a 64-bit little-endian Mach-O (magic {magic:#x})')
    off = 32
    segs, symtab, entry = [], None, None
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<II', buf, off)
        if cmd == LC_SEGMENT_64:
            name = buf[off + 8:off + 24].split(b'\0')[0].decode()
            vmaddr, vmsize, fileoff, filesize = struct.unpack_from('<4Q', buf, off + 24)
            segs.append(dict(name=name, vmaddr=vmaddr, vmsize=vmsize,
                             fileoff=fileoff, filesize=filesize))
        elif cmd == LC_SYMTAB:
            symtab = struct.unpack_from('<IIII', buf, off + 8)
        elif cmd == LC_MAIN:
            entry, = struct.unpack_from('<Q', buf, off + 8)
        elif cmd == LC_UNIXTHREAD:
            # arm_thread_state64: x0..x28, fp, lr, sp, pc, cpsr
            entry, = struct.unpack_from('<Q', buf, off + 16 + 32 * 8)
        off += size
    syms = {}
    if symtab:
        symoff, nsyms, stroff, _ = symtab
        for i in range(nsyms):
            nx, ntype, _, _, nvalue = struct.unpack_from('<IBBHQ', buf, symoff + 16 * i)
            if nvalue:
                end = buf.index(b'\0', stroff + nx)
                syms[buf[stroff + nx:end].decode('latin1')] = nvalue
    return buf, segs, syms, entry


def loader_constants(path):
    """The two absolute values the loader must agree with this kernel about."""
    out = {}
    try:
        src = open(path).read()
    except OSError:
        return out
    for name in ('XMARK_PHYS', 'PT_KVA_OFF'):
        m = re.search(r'#define\s+' + name + r'\s+\(?\s*(0x[0-9a-fA-F]+)', src)
        if m:
            out[name] = int(m.group(1), 16)
    return out


def tcr_fields(v):
    return dict(t0sz=v & 0x3f, tg0=(v >> 14) & 3, t1sz=(v >> 16) & 0x3f,
                tg1=(v >> 30) & 3, ips=(v >> 32) & 7)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('kernel')
    ap.add_argument('--phys-base', type=lambda s: int(s, 0), default=None,
                    help='physical address of __TEXT (default: from --payload)')
    ap.add_argument('--payload', default=None,
                    help='SWEETXNU payload built from this kernel: takes the '
                         'physical placement from it instead of guessing')
    ap.add_argument('--loader', default='shim/exynos990/loader.c',
                    help='loader source whose XMARK_PHYS/PT_KVA_OFF are checked')
    a = ap.parse_args()

    buf, segs, syms, entry = parse(a.kernel)
    text = next(s for s in segs if s['name'] == '__TEXT')
    base = text['vmaddr']
    ok = base >= TTBR1_START

    phys_base = a.phys_base
    if a.payload:
        hdr = open(a.payload, 'rb').read(0x48)
        if hdr[:8] != PAYLOAD_MAGIC:
            sys.exit(f'{a.payload}: not a {PAYLOAD_MAGIC.decode()} payload')
        virt, = struct.unpack_from('<Q', hdr, 0x10)
        pbase, = struct.unpack_from('<Q', hdr, 0x30 + 8)
        phys_base = pbase + (base - virt)
        print(f'payload                virt_base {virt:#x} phys_base {pbase:#x} '
              f'-> __TEXT PA {phys_base:#x}')
        if virt != base & ~(0x2000000 - 1):
            print(f'  warning: payload was built from a different kernel '
                  f'(expected virt_base {base & ~(0x2000000 - 1):#x})')
    if phys_base is None:
        phys_base = 0x891004000
        print('note: no --payload/--phys-base, assuming __TEXT at '
              f'{phys_base:#x}')
    a.phys_base = phys_base

    print(f'__TEXT                 {base:#018x}')
    print(f'TTBR1 window (4 KiB)   {TTBR1_START:#018x} .. 0xffffffffffffffff')
    print(f'link base reachable    {"yes" if ok else "NO"}')

    if '_sysreg_restore' in syms:
        va = syms['_sysreg_restore']
        off = va - base
        tcr = struct.unpack_from('<Q', buf, off)[0] if 0 <= off <= len(buf) - 8 else None
        if tcr is not None:
            f = tcr_fields(tcr)
            granule = {0: '4 KiB', 1: '64 KiB', 2: '16 KiB'}.get(f['tg0'], '?')
            print(f'TCR_EL1_BOOT           {tcr:#018x}  T1SZ={f["t1sz"]} TG0={granule} '
                  f'IPS={f["ips"]}')
            if f['t1sz'] <= TTBR1_SHIFT and not ok:
                print('  -> the link base is outside the window this TCR covers')
    else:
        print('TCR_EL1_BOOT           _sysreg_restore not in the symtab')

    if entry is not None:
        print(f'entry                  +{entry - base:#x} -> {entry:#018x} '
              f'(PA {a.phys_base + entry - base:#x})')
    for name, want in (('_intstack_low_guard', XMARK_OFF),
                       ('_bootstrap_pagetables', PT_OFF)):
        if name in syms:
            got = syms[name] - base
            flag = 'ok' if got == want else f'CHANGED (was +{want:#x})'
            print(f'{name:22} +{got:#x} -> {syms[name]:#018x} '
                  f'(PA {a.phys_base + got:#x}) {flag}')
    if '_intstack_top' in syms and '_intstack_low_guard' in syms:
        gap = syms['_intstack_top'] - syms['_intstack_low_guard']
        print(f'_intstack_top gap      {gap:#x} (loader reads the stack sentinel '
              f'at guard + 0x{gap - 0x10:x})')

    stale = []
    consts = loader_constants(a.loader)
    if '_intstack_low_guard' in syms and consts.get('XMARK_PHYS') is not None:
        want = a.phys_base + syms['_intstack_low_guard'] - base
        got = consts['XMARK_PHYS']
        print(f'loader XMARK_PHYS      {got:#x} (must be {want:#x}) '
              f'{"ok" if got == want else "STALE"}')
        if got != want:
            stale.append('XMARK_PHYS')
    if '_bootstrap_pagetables' in syms and consts.get('PT_KVA_OFF') is not None:
        want = syms['_bootstrap_pagetables'] - base
        got = consts['PT_KVA_OFF']
        print(f'loader PT_KVA_OFF      +{got:#x} (must be +{want:#x}) '
              f'{"ok" if got == want else "STALE"}')
        if got != want:
            stale.append('PT_KVA_OFF')

    return 0 if ok and not stale else 1


if __name__ == '__main__':
    sys.exit(main())
