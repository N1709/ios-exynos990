#!/usr/bin/env python3
"""Write the two artifacts the phone is flashed with.

    exynos990-kernel-package.py BOOT_IMG MODEL [OUTDIR]

BOOT_IMG is the Android boot image built by exynos990-make-boot.sh (its kernel
slot holds the loader, its ramdisk slot the XNU payload).  This makes

    OUTDIR/xnu.img          a copy of BOOT_IMG, ready for the boot partition
    OUTDIR/xnu-kernel.zip   the same image plus a shell update-binary, so TWRP
                            can install it: it writes the boot partition and
                            nothing else, and refuses any device but MODEL

The zip holds the image uncompressed on purpose: it is already ~17 MB, and
storing it keeps TWRP's install quick on a phone that spends most of its time
in recovery.
"""
import hashlib
import os
import struct
import sys
import zipfile

MODEL = {'z3s': 'SM-G988B', 'y2s': 'SM-G985F', 'x1s': 'SM-G981B',
         'r8s': 'SM-G980F', 'c1s': 'SM-G986B', 'c2s': 'SM-G986B'}


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    boot, model = sys.argv[1], sys.argv[2]
    outdir = sys.argv[3] if len(sys.argv) > 3 else 'out'
    if model not in MODEL:
        sys.exit(f'unknown model {model}')

    img = open(boot, 'rb').read()
    if img[:8] != b'ANDROID!':
        sys.exit(f'{boot}: not an Android boot image')
    kernel_size, = struct.unpack_from('<I', img, 8)

    os.makedirs(outdir, exist_ok=True)
    xnu_img = os.path.join(outdir, 'xnu.img')
    open(xnu_img, 'wb').write(img)

    ub = open(os.path.join(os.path.dirname(__file__),
                           'xnu-kernel-update-binary.sh'), 'rb').read()
    readme = (
        f'xnu-kernel.zip - Exynos 990 (Exynos 9830) XNU loader + kernel\n'
        f'\n'
        f'Model : {model} ({MODEL[model]})\n'
        f'Image : xnu.img, {len(img)} bytes, sha256 {hashlib.sha256(img).hexdigest()}\n'
        f'Layout: boot header v2, loader {kernel_size} bytes in the kernel slot,\n'
        f'        XNU payload in the ramdisk slot.\n'
        f'\n'
        f'Flash: install this zip in TWRP, or write xnu.img to the "boot"\n'
        f'partition by hand:\n'
        f'  adb push xnu.img /tmp/xnu.img\n'
        f'  adb shell dd if=/tmp/xnu.img of=/dev/block/by-name/boot bs=4M conv=fsync\n'
        f'\n'
        f'The zip writes the boot partition only.  It refuses to run on a\n'
        f'device whose ro.product.device is not {model}.\n'
    )

    zpath = os.path.join(outdir, 'xnu-kernel.zip')
    with zipfile.ZipFile(zpath, 'w') as z:
        z.writestr(zipfile.ZipInfo('xnu.img'), img, compress_type=zipfile.ZIP_STORED)
        zi = zipfile.ZipInfo('META-INF/com/google/android/update-binary')
        zi.external_attr = 0o100755 << 16
        zi.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(zi, ub)
        z.writestr('META-INF/com/google/android/updater-script',
                   f'# {model}: the update-binary is a shell script\n')
        z.writestr('README.txt', readme)

    print(f'  {xnu_img}: {len(img)} bytes')
    print(f'  {zpath}: {os.path.getsize(zpath)} bytes')


if __name__ == '__main__':
    main()
