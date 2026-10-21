#!/usr/bin/env python3
"""Check the Exynos 990 board configuration in an XNU tree.

The XNU kernel only builds on macOS, so the board configuration is checked
statically here instead: every file that tests the 990 must be able to see
the macro, every board constant the 990 uses must be defined, the donor
names must be gone and the machine config must be wired up.

  python3 tools/check-exynos990-config.py /path/to/xnu

Exit code 0 means the tree is consistent.
"""
import os
import re
import sys

STALE = [
    'A5_FB_PHYS', 'A5_FB_WIDTH', 'A5_FB_HEIGHT', 'A5_FB_STRIDE',
    'A5_KLOG_PHYS', 'A5_KLOG_SIZE', 'A5XELTE_VTIMER_INTID',
    'A5_ALIVE_PA', 'A5_PMU_BASE', 'a5xelte-fbcon', 'a5xelte-emmc',
]
RENAMED = [
    ('bsd/dev/a5xelte_keys.c', 'bsd/dev/samsung_keys.c'),
    ('bsd/dev/a5xelte_touch.c', 'bsd/dev/samsung_touch.c'),
    ('bsd/dev/a5xelte_emmc.c', 'bsd/dev/samsung_emmc.c'),
]
BOARD_MACROS = [
    'SWEET_FB_PHYS', 'SWEET_FB_WIDTH', 'SWEET_FB_HEIGHT', 'SWEET_FB_STRIDE',
    'SWEET_FB_2M_BLOCKS', 'SWEET_ENTRY_MARKER_PHYS', 'SWEET_BOOT_MPIDR',
    'SWEET_KEYS_DEFAULT', 'SWEET_TOUCH_DEFAULT', 'SWEET_KLOG_PHYS',
    'SWEET_KLOG_SIZE', 'SWEET_USE_PHYS_TIMER', 'SWEET_VTIMER_INTID',
    'SWEET_PMU_BASE', 'SWEET_ALIVE_BASE', 'SWEET_HAS_FB', 'SWEET_HAS_KLOG',
    'SWEET_SMP_MAX_CPUS',
]
SOURCE_EXT = ('.c', '.cpp', '.h', '.s', '.S')

fails = []


def check(cond, what):
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond:
        fails.append(what)
    return cond


def sources(tree):
    for dirpath, dirnames, filenames in os.walk(tree):
        dirnames[:] = [d for d in dirnames if d not in ('.git', 'build', 'obj')]
        for f in filenames:
            if f.endswith(SOURCE_EXT):
                yield os.path.join(dirpath, f)


def read(path):
    try:
        return open(path, errors='replace').read()
    except OSError:
        return ''


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    tree = os.path.abspath(sys.argv[1])
    print(f'XNU tree: {tree}')

    print('1. machine config')
    mk = read(os.path.join(tree, 'makedefs', 'MakeInc.def'))
    check('MACHINE_FLAGS_ARM64_EXYNOS990' in mk and '-DARM64_BOARD_CONFIG_EXYNOS990' in mk,
          'MakeInc.def defines MACHINE_FLAGS_ARM64_EXYNOS990')
    bc = read(os.path.join(tree, 'pexpert', 'pexpert', 'arm64', 'board_config.h'))
    check(re.search(r'#ifdef ARM64_BOARD_CONFIG_EXYNOS990\s*\n#include <pexpert/arm64/EXYNOS990.h>', bc) is not None,
          'board_config.h includes EXYNOS990.h for the 990 config')
    hdr = os.path.join(tree, 'pexpert', 'pexpert', 'arm64', 'EXYNOS990.h')
    check(os.path.exists(hdr), 'pexpert/pexpert/arm64/EXYNOS990.h exists')
    mkmake = read(os.path.join(tree, 'pexpert', 'pexpert', 'arm64', 'Makefile'))
    check('EXYNOS990.h' in mkmake, 'the arm64 Makefile installs EXYNOS990.h')

    print('2. macro visibility')
    missing = []
    for path in sources(tree):
        text = read(path)
        if 'defined(EXYNOS990)' not in text:
            continue
        if 'board_config.h' in text or 'ARM64_BOARD_CONFIG_EXYNOS990' in text:
            continue
        missing.append(os.path.relpath(path, tree))
    check(not missing, 'every file that tests EXYNOS990 can see the macro' +
          ('' if not missing else ': ' + ', '.join(missing)))

    print('3. board constants')
    header = read(hdr)
    undef = [m for m in BOARD_MACROS if f'#define {m} ' not in header]
    check(not undef, 'EXYNOS990.h defines every board constant' +
          ('' if not undef else ': ' + ', '.join(undef)))
    # a constant used in the tree but defined nowhere is a compile error
    used = set()
    defined = set()
    for path in sources(tree):
        text = read(path)
        used |= set(re.findall(r'\bSWEET_[A-Z0-9_]+', text))
        defined |= set(re.findall(r'#define\s+(SWEET_[A-Z0-9_]+)', text))
    undefined = sorted(m for m in used - defined
                       if m not in ('SWEET_FB', 'SWEET_STRIDE', 'SWEET_SCALE',
                                    'SWEET_ROW_HEIGHT', 'SWEET_COL_WIDTH', 'SWEET_COLS',
                                    'SWEET_FBCON', 'SWEET_HFS'))
    check(not undefined, 'no SWEET_* constant is used without a definition' +
          ('' if not undefined else ': ' + ', '.join(undefined)))

    print('4. donor names')
    stale_hits = []
    for path in sources(tree):
        text = read(path)
        for name in STALE:
            if name in text:
                stale_hits.append(f'{os.path.relpath(path, tree)}: {name}')
    check(not stale_hits, 'no donor constant or node name is left' +
          ('' if not stale_hits else ': ' + ', '.join(stale_hits)))
    for old, new in RENAMED:
        check(not os.path.exists(os.path.join(tree, old)) and os.path.exists(os.path.join(tree, new)),
              f'{old} -> {new}')

    print('5. device tree node names')
    serial = read(os.path.join(tree, 'pexpert', 'arm', 'pe_serial.c'))
    ufs = read(os.path.join(tree, 'bsd', 'dev', 'sweet_ufs.c'))
    check('samsung-fbcon' in serial, 'the framebuffer console node is samsung-fbcon')
    emmc = read(os.path.join(tree, 'bsd', 'dev', 'samsung_emmc.c'))
    check('samsung-emmc' in emmc, 'the eMMC node is samsung-emmc')
    check('/exynos990-ufs' in ufs, 'the UFS node is /exynos990-ufs')

    print()
    if fails:
        print(f'{len(fails)} check(s) failed')
        return 1
    print('all checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
