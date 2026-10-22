#!/usr/bin/env python3
"""Build the Exynos 990 (Exynos 9830) ADT for the XNU kernel.

Same schema as tools/mkadt_dreamlte.py (Galaxy S8, Exynos 8895) with the
board facts of the Exynos 990 family (Galaxy S20 and friends), all read from
the Android kernel tree:

  /home/n1709/app/experimental/android_kernel_samsung_exynos990/
      arch/arm64/boot/dts/exynos/exynos9830.dts          (SoC)
      arch/arm64/boot/dts/exynos/exynos9830-display-lcd.dtsi (panels)
      arch/arm64/boot/dts/samsung/exynos990-*.dtsi        (boards)

Board facts used here:

  - DRAM: the high bank at 0x880000000.  The Exynos maps its DRAM banks in
    the 36-bit space: the low 2 GiB at 0x80000000 and the next bank at
    0x880000000 (the S8 shows the same layout: 0x80000000-0xbc7fffff,
    0xc0000000-0xffffffff, 0x880000000-0x8ffffffff).  The bank's first
    0xAD00000 bytes are sec_rdx_bootdev, the bootloader's RAM-dump carveout
    (the phone's /proc/iomem shows it as reserved), so XNU starts after it.
    Only 512 MiB are handed to XNU: the S8 port panicked in kmem_range_init
    with the whole 2 GiB bank, so the same conservative size is used here
    until the 990 has been measured (see --dram-size).
  - GICv2 (GIC-400): GICD 0x10101000, GICC 0x10102000, "arm,cortex-a15-gic"
    (exynos9830.dts: gic:interrupt-controller@10100000).  The reg property
    holds the two pairs as offsets from the /arm-io ranges base 0x10000000,
    because pe_arm_get_soc_base_phys() adds that base again
    (xnu/patches/0086-a5xelte-gicv2-platform.patch).
  - Timer: the architected timer of exynos9830.dts (arm,armv8-timer) lists
    PPI 13/14/11/10 at 26 MHz.  On the Exynos GIC a PPI n is INTID 16 + n
    (only the 16 local SGIs are implemented), so the physical timer (PPI
    14) is INTID 30 and the virtual one (PPI 11) INTID 27 - the values the
    A5XELTE and S8 ports use.  The default here is the physical timer, as
    on the S8: the EL1 virtual timer does not reach the secondary cores on
    Exynos.  MUST be confirmed on the phone (XNU prints the INTID of an
    unexpected interrupt); --timer-intid overrides it.
  - Timebase 26 MHz (exynos9830.dts timer node, clock-frequency).
  - CPUs: 4x Cortex-A55 (MPIDR 0x000-0x003, cluster 0), 2x Cortex-A76
    (MPIDR 0x004-0x005, cluster 1), 2x Exynos M5 (MPIDR 0x100-0x101,
    cluster 2) - the layout of exynos9830.dts (cpu0@0000 .. cpu7@0101).
  - UFS host at 0x13100000 (standard HCI registers, 0x200 bytes) plus the
    Exynos vendor block at 0x13101100 (exynos9830.dts ufs: ufs@0x13100000).
    The reg layout is what bsd/dev/sweet_ufs.c expects: reg[0]/reg[1] are
    the standard registers, reg[2]/reg[3] the vendor ones.
  - No fbcon node: the DECON framebuffer handoff does not exist yet for this
    board, and a guessed framebuffer address is worse than none (a wrong
    address makes pe_serial.c map foreign memory).  A commented-out
    template is kept below for the day the handoff exists.

The identity stays the donor's iPhone X (D22AP) userland, and the root
compatible carries the names the existing XNU platform-expert personality
matches, so the port reuses the A5XELTE/DREAMLTE driver personality.
"""
import argparse
import os
from pathlib import Path

import adt
import mkadt

# --------------------------------------------------------------------------
# SoC constants (arch/arm64/boot/dts/exynos/exynos9830.dts)
# --------------------------------------------------------------------------

# High DRAM bank; overridable on the command line.  The bank starts at
# 0x880000000, but its first 0xAD00000 bytes are sec_rdx_bootdev, the
# bootloader's RAM-dump carveout (exynos9830-rmem.dtsi:65-70, and the
# phone's /proc/iomem shows "880000000-88acfffff : reserved"), so XNU is
# placed past it.
DRAM_BASE = 0x890000000
# 512 MiB: the S8 port panicked in kmem_range_init with the full 2 GiB bank.
DRAM_SIZE = 0x20000000
# armv8-timer clock-frequency in exynos9830.dts.
TIMEBASE = 26000000

# /arm-io "ranges" child base: the Exynos peripheral window starts here
# (clock controller 0x1a330000, GIC 0x10100000, UFS 0x13100000, DECON
# 0x19050000).  The GIC reg property is relative to this base.
SOC_BASE = 0x10000000

# GICv2 distributor and CPU interface (exynos9830.dts, gic node).
GICD_PHYS, GICD_SIZE = 0x10101000, 0x1000
GICC_PHYS, GICC_SIZE = 0x10102000, 0x1000

# GIC INTID of the architected timer on this board.
#
# The Exynos GIC (ARM Cortex-A15 GIC-400) implements only the 16 local
# SGIs, so its PPI window starts at INTID 16: PPI n is INTID 16 + n, not
# 32 + n as a textbook GICv2 would suggest.  Three independent facts
# agree on that mapping:
#   - the A5XELTE port (Exynos 7580) runs its virtual timer (PPI 11) on
#     INTID 27 = 16 + 11 (xnu/patches/0085, A5XELTE.h);
#   - the S8 port (Exynos 8895) runs its physical timer (PPI 14) on
#     INTID 30 = 16 + 14 (xnu/patches/0124, A5XELTE.h);
#   - XNU's own GICv3 path rejects any timer INTID outside 16..31 as
#     "not a PPI" (pexpert/arm/pe_fiq.c).
# The Exynos 990 uses the same physical timer as the S8: on Exynos the
# EL1 virtual timer does not deliver on the secondary cores, which is why
# the S8 port had to switch (boot 22/24 of the S8 log).  UNVERIFIED on
# the phone: if the kernel reports an unexpected timer INTID, change it
# with --timer-intid (27 = PPI 11 virtual, 30 = PPI 14 physical).
VTIMER_INTID = 30

# UFS host: standard HCI registers then the Exynos vendor HCI block, then
# the UFS protector (FMP/SMU).  The protector carries the descriptor type
# (UFSPRSECURITY, DESCTYPE) the kernel needs to build its PRDT entries.
UFS_HCI_BASE, UFS_HCI_SIZE = 0x13100000, 0x200
UFS_VS_BASE, UFS_VS_SIZE = 0x13101100, 0x200
UFS_PROT_BASE, UFS_PROT_SIZE = 0x132A0000, 0x100

# --------------------------------------------------------------------------
# Board list: board code -> panel facts
# --------------------------------------------------------------------------
#
# The canvas is the native portrait resolution of the panel, i.e. what the
# DECON/DPU is programmed for; the panel names are the DDI nodes of the
# Android tree so the resolution can be re-checked there.

MODELS = {
    'x1s': dict(
        name='Galaxy S20 (SM-G981B)',
        canvas=(1440, 3200),
        # exynos9830-display-lcd.dtsi: s6e3hab_ddi, resolution = <1440 3200>
        panel='s6e3hab',
    ),
    'y2s': dict(
        name='Galaxy S20+ (SM-G986B)',
        canvas=(1440, 3200),
        # exynos990-y2s_common.dtsi: panel-ddi-info = <&s6e3hab>
        panel='s6e3hab',
    ),
    'z3s': dict(
        name='Galaxy S20 Ultra (SM-G988B)',
        canvas=(1440, 3200),
        # exynos990-z3s_common.dtsi: panel-ddi-info = <&s6e3hab>
        panel='s6e3hab',
    ),
    'r8s': dict(
        name='Galaxy S20 FE (SM-G781B)',
        canvas=(1080, 2400),
        # exynos990-r8s_common.dtsi: panel-ddi-info = <&ea8076 &ea8079
        # &s6e3fc3>; exynos990-r8s_panel_display_modes.dtsi: 1080x2400
        panel='ea8076/ea8079/s6e3fc3',
    ),
    'c1s': dict(
        name='Galaxy Note 20 (SM-N981B)',
        canvas=(1080, 2400),
        # exynos9830-display-lcd.dtsi: s6e3hab_fhd_ddi, resolution = <1080 2400>
        panel='s6e3hab_fhd',
    ),
    'c2s': dict(
        name='Galaxy Note 20 Ultra (SM-N986B)',
        canvas=(1440, 3088),
        # exynos9830-display-lcd.dtsi: s6e3hac_ddi, resolution = <1440 3088>
        panel='s6e3hac',
    ),
}

# The port targets the whole Exynos 990 family; the default is the phone
# this port is being brought up on: Galaxy S20 Ultra (z3s, SM-G988B).
DEFAULT_MODEL = 'z3s'

# --------------------------------------------------------------------------
# CPU layout (exynos9830.dts, cpus node)
# --------------------------------------------------------------------------
#
# logical id, MPIDR (cpu@ unit address), cluster id, core inside the cluster,
# core type.  The MPIDR values are the unit addresses of the Linux cpu
# nodes: cpu0@0000 .. cpu3@0003 (A55), cpu4@0004, cpu5@0005 (A76),
# cpu6@0100, cpu7@0101 (Exynos M5).
#
# The Linux tree names the cores after the ARM implementation models
# ("arm,ananke" = A55, "arm,enyo" = A76, "arm,lion" = M5); the Cortex names
# are used here because that is what the other ports in this repo use and
# what the kernel's CPU code recognises.

CPUS = (
    (0, 0x0000, 0, 0, 'a55'),
    (1, 0x0001, 0, 1, 'a55'),
    (2, 0x0002, 0, 2, 'a55'),
    (3, 0x0003, 0, 3, 'a55'),
    (4, 0x0004, 1, 0, 'a76'),
    (5, 0x0005, 1, 1, 'a76'),
    (6, 0x0100, 2, 0, 'm5'),
    (7, 0x0101, 2, 1, 'm5'),
)

# Core type -> (compatible, clock-frequency, shared L2 cache id or None).
# The clock values are the published Exynos 990 limits; the Linux tree has
# no clock-frequency for its cpu nodes (it uses moce ratio tables), and XNU
# only uses the property informationally.
CORE_TYPES = {
    'a55': ('arm,cortex-a55', 1800000000, None),
    'a76': ('arm,cortex-a76', 2000000000, None),
    'm5': ('arm,cortex-m55', 1500000000, 6),   # both M5 cores share one L2
}


def set_prop(node, name, value):
    for i, (key, flags, _) in enumerate(node.props):
        if key == name:
            node.props[i] = (key, flags, value)
            return
    node.props.append((name, 0, value))


def child(node, name):
    for candidate in node.children:
        if candidate.prop('name') == mkadt.cstr(name):
            return candidate
    raise ValueError(f'missing donor ADT node {name}')


def cpu_props(logical, mpidr, cluster, core, kind):
    """Properties of one cpu node, in the style of the S8 generator."""
    compatible, clock, shared_l2 = CORE_TYPES[kind]
    return [
        ('device_type', mkadt.cstr('cpu')),
        ('compatible', mkadt.cstr(compatible) + mkadt.cstr('ARM,v8')),
        # The MPIDR, not the logical id: the kernel boots the core that
        # the firmware left running and uses the MPIDR to find it.
        ('reg', mkadt.u32(mpidr)),
        ('cpu-id', mkadt.u32(logical)),
        ('cluster-id', mkadt.u32(cluster)),
        ('cluster-type', mkadt.u32(0)),
        ('die-cluster-id', mkadt.u32(cluster)),
        ('cluster-core-id', mkadt.u32(core)),
        # Every A55 and A76 has its own L2 in the Linux tree
        # (L2_ANANKE_0..3, L2_ENYO_0..1); the two M5 cores share one
        # (L2_LION).
        ('l2-cache-id', mkadt.u32(logical if shared_l2 is None else shared_l2)),
        ('l3-cache-id', mkadt.u32(0)),
        ('state', mkadt.cstr('running' if logical == 0 else 'stopped')),
        ('timebase-frequency', mkadt.u32(TIMEBASE)),
        ('clock-frequency', mkadt.u32(clock)),
        ('bus-frequency', mkadt.u32(100000000)),
        ('memory-frequency', mkadt.u32(3200000000)),
        ('peripheral-frequency', mkadt.u32(TIMEBASE)),
        ('fixed-frequency', mkadt.u32(TIMEBASE)),
    ]


def build(model=DEFAULT_MODEL, canvas=None, dram_base=DRAM_BASE,
          dram_size=DRAM_SIZE, timer_intid=VTIMER_INTID):
    board = MODELS[model]
    if canvas is None:
        canvas = board['canvas']
    root = mkadt.build(seed=os.urandom(64), qemu=True)
    # The kernel's SweetPlatformExpert personality matches 'samsung,a5xelte'
    # and 'samsung,dreamlte' (KernelConfigTables.cpp, patch 0124); without
    # one of them IOKit panics "Unable to find driver for this platform".
    # IONameMatch accepts any listed name, so the board lists its own name
    # first and the two working ones behind it.
    set_prop(root, 'compatible', mkadt.cstr('samsung,exynos990') +
             mkadt.cstr('samsung,a5xelte') + mkadt.cstr('samsung,dreamlte') +
             mkadt.cstr('AppleARM'))
    chosen = child(root, 'chosen')
    set_prop(chosen, 'dram-base', mkadt.u64(dram_base))
    set_prop(chosen, 'dram-size', mkadt.u64(dram_size))
    set_prop(chosen, 'firmware-version', mkadt.cstr(f'{model}-entry'))
    set_prop(chosen, 'system-firmware-version', mkadt.cstr(f'{model}-entry'))
    cpus = child(root, 'cpus')
    cpu0 = child(cpus, 'cpu0')
    for key, value in cpu_props(*CPUS[0]):
        if key != 'cpu-id':
            set_prop(cpu0, key, value)
    for logical, mpidr, cluster, core, kind in CPUS[1:]:
        cpus.children.append(mkadt.node(
            f'cpu{logical}', cpu_props(logical, mpidr, cluster, core, kind)))
    arm_io = child(root, 'arm-io')
    set_prop(arm_io, 'device_type', mkadt.cstr('exynos990-io'))
    set_prop(arm_io, 'compatible', mkadt.cstr('samsung,exynos9830-io') +
             mkadt.cstr('arm-io'))
    # The peripheral window: 0x10000000 (clock controller 0x1a330000, GIC
    # 0x10100000, UFS 0x13100000, DECON 0x19050000) up to 0x100000000.
    set_prop(arm_io, 'ranges', mkadt.u64(0, SOC_BASE, 0xf0000000))
    arm_io.children = [n for n in arm_io.children if n.prop('name') == mkadt.cstr('wdt')]
    arm_io.children.append(mkadt.node('gic', [
        ('compatible', mkadt.cstr('arm,cortex-a15-gic')),
        ('interrupt-controller', mkadt.cstr('master')),
        # INTID of the architected timer: 27 = PPI 11 (virtual), 30 = PPI
        # 14 (physical) - the Exynos GIC starts its PPI window at INTID 16.
        # UNVERIFIED on the phone; the wrong INTID means the timer
        # interrupt never fires.
        ('virtual-timer-intid', mkadt.u32(timer_intid)),
        # Offsets from the /arm-io ranges base (0x10000000), because the
        # kernel adds that base itself (pe_arm_get_soc_base_phys()).
        ('reg', mkadt.u64(GICD_PHYS - SOC_BASE, GICD_SIZE,
                          GICC_PHYS - SOC_BASE, GICC_SIZE)),
    ]))
    arm_io.children.append(mkadt.node('disp0', [
        ('device_type', mkadt.cstr('display-subsystem')),
        # AppleCLCD (SweetFramebuffer.cpp) matches 'samsung,exynos990-display'
        # (or the donor strings); without it no main display exists and
        # SpringBoard aborts.
        ('compatible', mkadt.cstr('samsung,exynos990-display') +
         mkadt.cstr('samsung,a5xelte-display')),
        # The native portrait resolution of the panel of this board.
        ('canvas-width', mkadt.u32(canvas[0])),
        ('canvas-height', mkadt.u32(canvas[1])),
    ]))
    # The donor's Qualcomm framebuffer and UFS nodes must go: their
    # addresses belong to the SWEET phone and the drivers would poke
    # foreign memory.
    root.children = [n for n in root.children if n.prop('name') not in
                     (mkadt.cstr('sweet-fbcon'), mkadt.cstr('sweet-ufs'))]
    # bsd/dev/sweet_ufs.c (A5XELTE config, EXYNOS_UFS) reads the node by
    # name and expects reg[0]/reg[1] = standard HCI registers,
    # reg[2]/reg[3] = Exynos vendor HCI block (HCI_UTRL_NEXUS_TYPE etc.),
    # reg[4]/reg[5] = the UFS protector (UFSPRSECURITY, DESCTYPE).
    root.children.append(mkadt.node('exynos990-ufs', [
        ('compatible', mkadt.cstr('samsung,exynos-ufs')),
        ('reg', mkadt.u64(UFS_HCI_BASE, UFS_HCI_SIZE,
                          UFS_VS_BASE, UFS_VS_SIZE,
                          UFS_PROT_BASE, UFS_PROT_SIZE)),
    ]))
    # TODO(Exynos 990): add the framebuffer console node once the DECON
    # handoff exists.  pe_serial.c looks the console up by name
    # (SecureDTFindEntry("name", "a5xelte-fbcon", ...)) and maps reg[0]
    # with reg[1] bytes; the physical address of the DECON window is not
    # known yet, so a guessed address is not published here.
    #
    # root.children.append(mkadt.node('samsung-fbcon', [
    #     ('reg', mkadt.u64(FB_PHYS, FB_W * FB_H * 4)),
    #     ('width', mkadt.u32(FB_W)),
    #     ('height', mkadt.u32(FB_H)),
    #     ('stride', mkadt.u32(FB_W * 4)),
    # ]))
    return root


def parse_canvas(text):
    try:
        width, height = (int(part) for part in text.lower().split('x'))
    except ValueError:
        raise argparse.ArgumentTypeError(f'bad canvas {text!r}, want WxH') from None
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError(f'bad canvas {text!r}, want WxH')
    return (width, height)


def auto_int(text):
    """Accept 0x-prefixed and decimal sizes/addresses."""
    return int(text, 0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--model', default=DEFAULT_MODEL, choices=sorted(MODELS),
                        help='board code (default: %(default)s)')
    parser.add_argument('--canvas', type=parse_canvas, default=None,
                        help='display size presented to iOS, WxH '
                             '(default: the native panel of the board)')
    parser.add_argument('--dram-base', type=auto_int, default=DRAM_BASE,
                        help='physical base of the XNU RAM window '
                             '(default: 0x%x)' % DRAM_BASE)
    parser.add_argument('--dram-size', type=auto_int, default=DRAM_SIZE,
                        help='size of the XNU RAM window '
                             '(default: 0x%x)' % DRAM_SIZE)
    parser.add_argument('--timer-intid', type=auto_int, default=VTIMER_INTID,
                        help='GICv2 INTID of the architected timer '
                             '(27 = PPI 11 virtual, 30 = PPI 14 physical; '
                             'default: %(default)s)')
    parser.add_argument('--dump', action='store_true',
                        help='print the tree after writing it')
    args = parser.parse_args()
    board = MODELS[args.model]
    canvas = args.canvas or board['canvas']
    blob = adt.serialize(build(model=args.model, canvas=args.canvas,
                                dram_base=args.dram_base,
                                dram_size=args.dram_size,
                                timer_intid=args.timer_intid))
    args.output.write_bytes(blob)
    print(f'{args.output}: {len(blob)} bytes')
    print(f'  model {args.model} ({board["name"]}), panel {board["panel"]}, '
          f'canvas {canvas[0]}x{canvas[1]}')
    if args.dump:
        adt.dump(adt.parse(blob)[0])
