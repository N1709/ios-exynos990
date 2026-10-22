#!/usr/bin/env python3
"""Self test of the Exynos 990 boot chain (loader, payload, boot image).

Everything the loader reads at run time is checked here without a phone:

  1. the loader image builds and holds no absolute relocation,
  2. the ADT of every model parses and carries the nodes the kernel looks up
     (/chosen dram-base/dram-size, /cpus/*/reg, /exynos990-ufs/reg),
  3. the payload container has the layout the loader expects (magic, version,
     segment table, ADT offset, RAM window),
  4. the boot image v2 puts the loader at the address board.h claims and the
     payload in the ramdisk, and the ramdisk still holds the payload magic,
  5. the payload RAM window and the ADT agree with each other.

Usage:  python3 tools/exynos990-selftest.py [--model z3s ...]
"""
import argparse
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import adt  # noqa: E402

MODELS = ['x1s', 'y2s', 'z3s', 'r8s', 'c1s', 'c2s']
BOOT_BASE = 0x10000000
KERNEL_OFFSET = 0x8000
RAMDISK_OFFSET = 0x1000000
ANDROID_BOOT_MAGIC = b'ANDROID!'

failures = []


def check(cond, what):
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond:
        failures.append(what)
    return cond


def board_constants():
    """Read the addresses the loader uses out of shim/exynos990/board.h."""
    consts = {}
    raw = {}
    path = os.path.join(ROOT, 'shim', 'exynos990', 'board.h')
    for line in open(path):
        line = line.split('/*')[0].strip()
        if not line.startswith('#define'):
            continue
        parts = line.split(None, 2)
        if len(parts) < 3:
            continue
        name, value = parts[1], parts[2].split()[0]
        raw[name] = value
    # resolve the aliases (XNU_RAM_BASE = HIGH_RAM_BASE) in a few passes
    for _ in range(4):
        for name, value in raw.items():
            if name in consts:
                continue
            try:
                consts[name] = int(value.rstrip('UL'), 0)
            except ValueError:
                if value in consts:
                    consts[name] = consts[value]
    return consts


def build_loader(tmp):
    print('1. loader image')
    out = subprocess.run(['bash', 'build.sh'], cwd=os.path.join(ROOT, 'shim', 'exynos990'),
                         capture_output=True, text=True)
    if out.returncode != 0:
        print(out.stdout + out.stderr)
        check(False, 'loader builds')
        return None
    img = os.path.join(ROOT, 'shim', 'exynos990', 'out', 'Image')
    check(os.path.exists(img), 'loader image exists')
    size = os.path.getsize(img)
    check(size > 0, f'loader image is {size} bytes')
    # the arm64 Image header: magic "ARM\x64" at offset 56
    with open(img, 'rb') as f:
        head = f.read(64)
    check(head[56:60] == b'ARM\x64', 'arm64 Image header magic')
    return img


def adt_nodes(blob):
    root, _ = adt.parse(blob)
    return root


def check_adt(model, tmp):
    print(f'2. ADT ({model})')
    path = os.path.join(tmp, f'{model}.adt')
    out = subprocess.run([sys.executable, os.path.join(HERE, 'mkadt_exynos990.py'),
                          '--model', model, path], capture_output=True, text=True)
    if out.returncode != 0:
        print(out.stdout + out.stderr)
        check(False, f'ADT generated ({model})')
        return None
    blob = open(path, 'rb').read()
    root = adt_nodes(blob)
    children = {c.name: c for c in root.children}
    chosen = children.get('chosen')
    check(chosen is not None, '/chosen exists')
    if chosen is None:
        return None
    dram_base = struct.unpack('<Q', chosen.prop('dram-base'))[0]
    dram_size = struct.unpack('<Q', chosen.prop('dram-size'))[0]
    consts = board_constants()
    check(dram_base == consts['XNU_RAM_BASE'],
          f'/chosen dram-base = 0x{dram_base:x} == XNU_RAM_BASE')
    # The first 0xAD00000 of the high bank is sec_rdx_bootdev, the
    # bootloader's RAM-dump carveout (exynos9830-rmem.dtsi:65-70, and the
    # phone's /proc/iomem shows it as reserved): XNU must start past it.
    check(dram_base >= 0x88AD00000,
          f'/chosen dram-base 0x{dram_base:x} is past sec_rdx_bootdev')
    check(dram_size > 0, f'/chosen dram-size = 0x{dram_size:x}')
    cpus = next((c for c in root.children if c.name == 'cpus'), None)
    check(cpus is not None, '/cpus exists')
    cpu_nodes = cpus.children if cpus is not None else []
    check(len(cpu_nodes) == 8, f'8 cpu nodes ({len(cpu_nodes)})')
    mp = []
    for c in cpu_nodes:
        reg = c.prop('reg')
        if reg is None or len(reg) != 4:
            continue
        mp.append(struct.unpack('<I', reg)[0])
    check(len(mp) == 8 and len(set(mp)) == 8, f'8 distinct cpu MPIDRs {sorted(mp)}')
    check(mp == sorted(mp), 'cpu nodes are ordered by MPIDR')
    ufs = children.get('exynos990-ufs')
    check(ufs is not None, '/exynos990-ufs exists')
    if ufs is not None:
        reg = ufs.prop('reg')
        check(reg is not None and len(reg) == 48, 'ufs reg has 3 ranges (HCI + vendor + protector)')
        if reg is not None and len(reg) == 48:
            hci, hci_size, vendor, vendor_size, prot, prot_size = struct.unpack('<6Q', reg)
            check(hci == 0x13100000, f'ufs HCI base 0x{hci:x}')
            check(vendor == 0x13101100, f'ufs vendor base 0x{vendor:x}')
            check(prot == 0x132A0000, f'ufs protector base 0x{prot:x}')
    return blob, path


def check_payload(kernel, adt_path, tmp, model):
    print(f'3. payload ({model})')
    payload = os.path.join(tmp, f'{model}.payload')
    out = subprocess.run([sys.executable, os.path.join(HERE, 'mkpayload.py'),
                          kernel, adt_path, payload,
                          '--cmdline', 'debug=0xc serial=3 cpus=1 keepsyms=1'],
                         capture_output=True, text=True)
    if out.returncode != 0:
        print(out.stdout + out.stderr)
        check(False, 'payload built')
        return None
    blob = open(payload, 'rb').read()
    magic, version, nsegs = struct.unpack_from('<8sII', blob, 0)
    (virt_base, entry_va, adt_off, adt_size, kernel_end,
     phys_base, mem_size) = struct.unpack_from('<7Q', blob, 0x10)
    check(magic == b'SWEETXNU', 'payload magic')
    check(version == 1, 'payload version 1')
    check(0 < nsegs <= 32, f'{nsegs} segments')
    check(adt_off + adt_size <= len(blob), 'ADT inside the payload')
    check(blob[adt_off:adt_off + adt_size] == open(adt_path, 'rb').read(),
          'payload carries the generated ADT')
    consts = board_constants()
    check(phys_base >= consts['XNU_RAM_BASE'] and phys_base < consts['XNU_RAM_END'],
          f'phys_base 0x{phys_base:x} inside the XNU window')
    check(mem_size > 0 and phys_base + mem_size <= consts['XNU_RAM_END'],
          f'mem_size 0x{mem_size:x} fits the XNU window')
    root = adt_nodes(open(adt_path, 'rb').read())
    chosen = next(c for c in root.children if c.name == 'chosen')
    adt_base = struct.unpack('<Q', chosen.prop('dram-base'))[0]
    adt_size_ = struct.unpack('<Q', chosen.prop('dram-size'))[0]
    check((adt_base, adt_size_) == (phys_base, mem_size),
          'ADT dram-base/dram-size match the payload RAM window')
    check(entry_va >= virt_base and kernel_end >= virt_base, 'VA layout sane')
    segs = []
    for i in range(nsegs):
        segs.append(struct.unpack_from('<4Q', blob, 0x2a8 + 32 * i))
    ok = all(s[1] >= s[3] and s[2] + s[3] <= len(blob) for s in segs)
    check(ok, 'segment table inside the payload')
    return blob, payload


def check_boot_image(image, payload_blob, tmp, model):
    print(f'4. boot image ({model})')
    boot = os.path.join(tmp, f'{model}.boot.img')
    out = subprocess.run([sys.executable, os.path.join(HERE, 'mkboot_exynos990.py'),
                          '--kernel', image, '--ramdisk',
                          os.path.join(tmp, f'{model}.payload'), '--out', boot],
                         capture_output=True, text=True)
    if out.returncode != 0:
        print(out.stdout + out.stderr)
        check(False, 'boot image built')
        return
    blob = open(boot, 'rb').read()
    check(blob[:8] == ANDROID_BOOT_MAGIC, 'ANDROID! magic')
    kernel_size, kernel_addr, ramdisk_size, ramdisk_addr = struct.unpack_from('<4I', blob, 8)
    check(kernel_addr == BOOT_BASE + KERNEL_OFFSET,
          f'kernel at 0x{kernel_addr:x} (0x{BOOT_BASE + KERNEL_OFFSET:x})')
    check(ramdisk_addr == BOOT_BASE + RAMDISK_OFFSET,
          f'ramdisk at 0x{ramdisk_addr:x} (0x{BOOT_BASE + RAMDISK_OFFSET:x})')
    page = struct.unpack_from('<I', blob, 0x24)[0] or 4096
    # page 0: header, then the kernel, then the ramdisk
    k_off = page
    r_off = page + (kernel_size + page - 1) // page * page
    check(blob[k_off + 56:k_off + 60] == b'ARM\x64', 'kernel slot holds the loader image')
    check(blob[r_off:r_off + 8] == b'SWEETXNU', 'ramdisk slot holds the payload')
    check(blob[r_off:r_off + len(payload_blob)] == payload_blob, 'ramdisk payload intact')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--model', action='append', default=None)
    args = ap.parse_args()
    models = args.model or MODELS

    with tempfile.TemporaryDirectory() as tmp:
        image = build_loader(tmp)
        if image is None:
            return 1
        kernel = os.path.join(tmp, 'fakekernel')
        subprocess.run([sys.executable, os.path.join(HERE, 'mkfakekernel.py'), kernel],
                       check=True, capture_output=True)
        for model in models:
            res = check_adt(model, tmp)
            if res is None:
                continue
            blob, adt_path = res
            res = check_payload(kernel, adt_path, tmp, model)
            if res is None:
                continue
            _, payload = res
            check_boot_image(image, open(payload, 'rb').read(), tmp, model)

    print()
    if failures:
        print(f'{len(failures)} check(s) failed:')
        for f in failures:
            print('  - ' + f)
        return 1
    print('all checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
