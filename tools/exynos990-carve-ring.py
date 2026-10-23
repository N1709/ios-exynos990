#!/usr/bin/env python3
"""Turn the parked pstore ring in /proc/last_kmsg back into text.

The loader copies the 16 KiB ramoops dump zone (0xeb308000) to
0xFD910000 + 0x180000 on every run and tags it with "PREVRING1" and a
32-bit length (shim/exynos990/loader.c, prev_ring_park).  This reads the
copy out of a last_kmsg dump and prints it the way persistent_ram stores
it: the record's own header says where the oldest byte is.

    tools/exynos990-carve-ring.py lastkmsg.txt [--raw]

With --raw the text is printed in memory order, which is what to look at
when the header is the thing that is broken.
"""
import re
import struct
import sys

MARK = b"PREVRING1"


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    raw = open(sys.argv[1], "rb").read()
    i = raw.find(MARK)
    if i < 0:
        sys.exit("no PREVRING1 in %s: the loader did not run, or the "
                 "recovery kernel overwrote that page" % sys.argv[1])
    n, = struct.unpack_from("<I", raw, i + len(MARK))
    ring = raw[i + len(MARK) + 4:i + len(MARK) + 4 + n]
    print(f"parked ring at 0x{i:x}, {len(ring)} bytes", file=sys.stderr)

    sig, start, size = struct.unpack_from("<III", ring, 0)
    cap = len(ring) - 12
    print(f"header sig={sig:#x} ({sig.to_bytes(4, 'little')!r}) "
          f"start={start} size={size} cap={cap}", file=sys.stderr)

    data = ring[12:]
    if "--raw" in sys.argv or sig != 0x43474244:
        out = data
    elif size == 0:
        out = b""
    elif size > cap:
        print("size > cap: the record is invalid, showing raw bytes",
              file=sys.stderr)
        out = data
    elif start and start % cap:
        out = data[start:size] + data[:start] if start < size else \
            data[start:] + data[:size]
    else:
        out = data[:size]

    text = bytes(c if 32 <= c < 127 or c in (9, 10) else 10 for c in out)
    sys.stdout.write(text.decode("latin1"))
    if not text.endswith("\n"):
        print()


main()
