#!/usr/bin/env python3
"""Build the star2lte (SM-G965F/DS, Galaxy S9+, Exynos 9810) ADT.

Derived from mkadt_herolte.py. Addresses come from STAR2LTE_PROGRESS.md
(LineageOS boot.img DTB + the probe v2 on-device run). Status:
source-derived, never booted under XNU. The XNU RAM window is deliberately
NOT defaulted: choose it from the memmap probe's DRAM/reserved-memory
report and pass --dram-base/--dram-size. Storage is the Exynos 9810 UFS
(/star2lte-ufs); no touch or GPU nodes yet.
"""
import argparse
import os
from pathlib import Path

import adt
import mkadt

SOC_BASE = 0x10000000       # arm-io ranges base; child regs are relative
FB_BASE, FB_WIDTH, FB_HEIGHT = 0xcc000000, 1440, 2960   # S-Boot scanout
TIMEBASE = 26000000         # CNTFRQ_EL0 reads 0 under S-Boot: hardcode 26 MHz
# GIC-400 (DTB /interrupt-controller@10100000)
GICD, GICC = 0x10101000, 0x10102000
# UFS (exact star2lte vendor DTS /ufs@286392320): four reg tuples, IRQ 242.
# Keep the DTB's actual resource sizes; do not round them up to page size.
UFS_HCI, UFS_VENDOR, UFS_UNIPRO, UFS_PROT = 0x11120000, 0x11121100, 0x11110000, 0x11130000
UFS_HCI_SIZE, UFS_VENDOR_SIZE, UFS_UNIPRO_SIZE, UFS_PROT_SIZE = 0x200, 0x200, 0x8000, 0x100
UFS_IRQ = 242
# Nominal Exynos 9810 clocks (public spec, not measured): 4x A55, 4x Mongoose M3.
A55_HZ, M3_HZ = 1794000000, 2704000000


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


def build(canvas, dram, scale=3):
    root = mkadt.build(seed=os.urandom(64), qemu=True)
    if scale == 2:
        # iPhone XR screen, 414x896 points at @2x (828x1792): the closest
        # iPhone aspect (2.16) to the 1440x2960 panel (2.06) with 2.5x
        # fewer pixels for the CPU renderer. Boot args: sweetfb_scale=1
        # sweetfb_lw=828 sweetfb_lh=1792 sweetfb_gpu=1.
        set_prop(child(root, 'chosen'), 'display-scale', mkadt.u32(2))
        product = child(root, 'product')
        set_prop(product, 'artwork-device-subtype', mkadt.u32(1792))
        set_prop(product, 'artwork-scale-factor', mkadt.u32(2))
        canvas = (828, 1792)
    # Userland is the iPhone X IPSW (iPhone10,3 / D22AP). 'samsung,a5xelte'
    # stays for the shared SweetPlatformExpert IONameMatch until the
    # STAR2LTE machine config registers its own.
    set_prop(root, 'compatible', mkadt.cstr('D22AP') + mkadt.cstr('iPhone10,3') +
             mkadt.cstr('samsung,a5xelte') + mkadt.cstr('samsung,star2lte') +
             mkadt.cstr('samsung,exynos9810') + mkadt.cstr('AppleARM'))
    chosen = child(root, 'chosen')
    set_prop(chosen, 'dram-base', mkadt.u64(dram[0]))
    set_prop(chosen, 'dram-size', mkadt.u64(dram[1]))
    set_prop(chosen, 'firmware-version', mkadt.cstr('star2lte-entry'))
    set_prop(chosen, 'system-firmware-version', mkadt.cstr('star2lte-entry'))
    # MobileGestalt derives the UDID from these (see mkadt_herolte.py).
    # Synthetic, locally administered addresses, not the phone's.
    for name, last in (('wifi0', 1), ('bluetooth0', 2), ('ethernet0', 3)):
        set_prop(chosen, f'mac-address-{name}', bytes((0x02, 0x53, 0x39, 0x50, 0x4c, last)))
    set_prop(child(root, 'product'), 'artwork-display-gamut', mkadt.cstr('sRGB'))
    cpus = child(root, 'cpus')
    cpu0 = child(cpus, 'cpu0')
    # Probe v2: S-Boot enters on a Cortex-A55 (MIDR 0x410FD051), MPIDR 0x80000000
    # = affinity 0. DTB: A55 cpu@0000-0003 = MPIDR 0x000-0x003, Mongoose M3
    # cpu@0100-0103 = MPIDR 0x100-0x103. XNU sees one homogeneous cluster:
    # logical 0..3 = A55, 4..7 = Mongoose; cpus=4 keeps the A55s only.
    set_prop(cpu0, 'compatible', mkadt.cstr('arm,cortex-a55') + mkadt.cstr('arm,cortex-a53') +
             mkadt.cstr('ARM,v8'))
    set_prop(cpu0, 'reg', mkadt.u32(0x000))
    set_prop(cpu0, 'timebase-frequency', mkadt.u32(TIMEBASE))
    for logical in range(1, 8):
        mongoose = logical >= 4
        core = logical - 4 if mongoose else logical
        cpus.children.append(mkadt.node(f'cpu{logical}', [
            ('device_type', mkadt.cstr('cpu')),
            # SweetCPU matches IONameMatch 'arm,cortex-a53' (KernelConfigTables).
            ('compatible', (mkadt.cstr('samsung,mongoose') if mongoose else mkadt.cstr('arm,cortex-a55')) +
                           mkadt.cstr('arm,cortex-a53') + mkadt.cstr('ARM,v8')),
            ('reg', mkadt.u32((0x100 | core) if mongoose else core)),
            ('cpu-id', mkadt.u32(logical)),
            ('cluster-id', mkadt.u32(0)),
            ('cluster-type', mkadt.u32(0)),
            ('die-cluster-id', mkadt.u32(0)),
            ('cluster-core-id', mkadt.u32(logical)),
            ('l2-cache-id', mkadt.u32(1 if mongoose else 0)),
            ('l3-cache-id', mkadt.u32(0)),
            ('state', mkadt.cstr('stopped')),
            ('timebase-frequency', mkadt.u32(TIMEBASE)),
            ('clock-frequency', mkadt.u32(M3_HZ if mongoose else A55_HZ)),
            ('bus-frequency', mkadt.u32(100000000)),
            ('memory-frequency', mkadt.u32(1794000000)),
            ('peripheral-frequency', mkadt.u32(TIMEBASE)),
            ('fixed-frequency', mkadt.u32(TIMEBASE)),
        ]))
    arm_io = child(root, 'arm-io')
    set_prop(arm_io, 'device_type', mkadt.cstr('star2lte-io'))
    set_prop(arm_io, 'compatible', mkadt.cstr('samsung,exynos9810-io') + mkadt.cstr('arm-io'))
    set_prop(arm_io, 'ranges', mkadt.u64(0, SOC_BASE, 0xf0000000))
    arm_io.children = [n for n in arm_io.children if n.prop('name') == mkadt.cstr('wdt')]
    arm_io.children.append(mkadt.node('gic', [
        ('compatible', mkadt.cstr('arm,cortex-a15-gic')),
        ('interrupt-controller', mkadt.cstr('master')),
        ('virtual-timer-intid', mkadt.u32(27)),
        ('reg', mkadt.u64(GICD - SOC_BASE, 0x1000, GICC - SOC_BASE, 0x1000)),
    ]))
    arm_io.children.append(mkadt.node('disp0', [
        ('device_type', mkadt.cstr('display-subsystem')),
        ('compatible', mkadt.cstr('samsung,a5xelte-display') + mkadt.cstr('samsung,star2lte-display')),
        ('canvas-width', mkadt.u32(canvas[0])),
        ('canvas-height', mkadt.u32(canvas[1])),
    ]))
    # launchd starts com.apple.wifid only if the tree has a node named "wlan".
    arm_io.children.append(mkadt.node('wlan', [
        ('device_type', mkadt.cstr('wlan')),
        ('compatible', mkadt.cstr('star2lte,wlan')),
    ]))
    root.children = [n for n in root.children if n.prop('name') not in
                     (mkadt.cstr('sweet-fbcon'), mkadt.cstr('sweet-ufs'))]
    # Absolute addresses for the (not yet ported) Exynos 9810 UFS driver.
    root.children.append(mkadt.node('star2lte-ufs', [
        ('reg', mkadt.u64(UFS_HCI, UFS_HCI_SIZE, UFS_VENDOR, UFS_VENDOR_SIZE,
                         UFS_UNIPRO, UFS_UNIPRO_SIZE, UFS_PROT, UFS_PROT_SIZE)),
        ('interrupts', mkadt.u32(UFS_IRQ)),
    ]))
    # pe_serial.c looks this node up by name for the kprintf console.
    root.children.append(mkadt.node('a5xelte-fbcon', [
        ('reg', mkadt.u64(FB_BASE, FB_WIDTH * FB_HEIGHT * 4)),
        ('width', mkadt.u32(FB_WIDTH)),
        ('height', mkadt.u32(FB_HEIGHT)),
        ('stride', mkadt.u32(FB_WIDTH * 4)),
    ]))
    return root


def parse_int(text):
    return int(text, 0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--dram-base', type=parse_int, required=True,
                        help='XNU RAM base, from the memmap probe (no default on purpose)')
    parser.add_argument('--dram-size', type=parse_int, required=True,
                        help='XNU RAM size in bytes, from the memmap probe')
    parser.add_argument('--canvas', default=f'{FB_WIDTH}x{FB_HEIGHT}',
                        help='presented display WxH (828x1792 with --scale2)')
    parser.add_argument('--scale2', action='store_true',
                        help='iPhone XR screen, 828x1792 @2x (needs research/ipsw-x/d22ap.adt)')
    args = parser.parse_args()
    canvas = tuple(int(v) for v in args.canvas.lower().split('x'))
    blob = adt.serialize(build(canvas, (args.dram_base, args.dram_size), 2 if args.scale2 else 3))
    args.output.write_bytes(blob)
    print(f'{args.output}: {len(blob)} bytes')
