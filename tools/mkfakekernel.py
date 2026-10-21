#!/usr/bin/env python3
"""Build a small Mach-O that stands in for the XNU kernel.

The Exynos 990 boot chain (loader -> payload -> boot image) can be tested
end to end without the real kernel: mkpayload.py only needs a 64-bit Mach-O
with LC_SEGMENT_64 commands and an LC_UNIXTHREAD entry point, and the loader
only copies the segments flat.  This file produces exactly that, so the
self test can walk the whole chain and check every field the loader reads.

  mkfakekernel.py OUT [--vmaddr 0xffffff8000200000] [--size 0x200000]

The segment contents are filled with a repeating marker (the segment address)
so the self test can prove that the right bytes would be copied to the right
place.
"""
import argparse
import struct

MH_MAGIC_64 = 0xfeedfacf
CPU_TYPE_ARM64 = 0x0100000C
CPU_SUBTYPE_ARM64_ALL = 0
MH_EXECUTE = 0x2
LC_SEGMENT_64 = 0x19
LC_UNIXTHREAD = 0x5

HEADER_SIZE = 32
SEG_CMD_SIZE = 72


def segment_command(name, vmaddr, vmsize, fileoff, filesize, maxprot, initprot):
    segname = name.encode().ljust(16, b'\0')
    return struct.pack('<II16s4Q4I', LC_SEGMENT_64, SEG_CMD_SIZE, segname,
                       vmaddr, vmsize, fileoff, filesize,
                       maxprot, initprot, 0, 0)


def build(vmaddr, size):
    entry = vmaddr + 0x1000

    # arm_thread_state64: x0..x28, fp, lr, sp, pc, cpsr
    state = [0] * 32 + [entry, 0]
    thread = struct.pack('<II', 6, len(state)) + struct.pack('<%dQ' % len(state), *state)
    unixthread = struct.pack('<II', LC_UNIXTHREAD, 8 + len(thread)) + thread

    segs = [
        ('__TEXT', vmaddr, size, 7, 5),
        ('__LINKEDIT', vmaddr + 0x800000, 0x1000, 7, 1),
        ('__DATA', vmaddr + 0x1000000, 0x1000, 7, 3),
    ]

    sizeofcmds = SEG_CMD_SIZE * len(segs) + len(unixthread)
    out = bytearray(struct.pack('<8I', MH_MAGIC_64, CPU_TYPE_ARM64, CPU_SUBTYPE_ARM64_ALL,
                                MH_EXECUTE, len(segs) + 1, sizeofcmds, 0, 0))
    body_off = HEADER_SIZE + sizeofcmds

    # the file body: one page per segment, the __TEXT segment carries the
    # pattern that marks the kernel bytes
    pages = []
    for name, seg_vmaddr, vmsize, mp, ip in segs:
        pages.append((name, seg_vmaddr, vmsize, mp, ip, bytes(bytearray((i * 7 + 1) & 0xff for i in range(0x1000)))))

    cmds = []
    off = body_off
    for name, seg_vmaddr, vmsize, mp, ip, data in pages:
        if name == '__TEXT':
            # the mach header lives inside __TEXT, so it starts at file offset 0
            cmds.append(segment_command(name, seg_vmaddr, vmsize, 0, off + len(data), mp, ip))
        else:
            cmds.append(segment_command(name, seg_vmaddr, vmsize, off, len(data), mp, ip))
            off += len(data)
    for c in cmds:
        out += c
    out += unixthread
    assert len(out) == body_off, (len(out), body_off)
    for name, seg_vmaddr, vmsize, mp, ip, data in pages:
        out += data
    return bytes(out)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('--vmaddr', default='0xffffff8000200000')
    ap.add_argument('--size', default='0x200000')
    a = ap.parse_args()
    blob = build(int(a.vmaddr, 0), int(a.size, 0))
    open(a.out, 'wb').write(blob)
    print(f'{a.out}: {len(blob)} bytes, entry 0x{int(a.vmaddr, 0) + 0x1000:x}')
