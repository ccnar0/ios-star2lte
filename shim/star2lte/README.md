# star2lte stage-0 probes

These are diagnostic bare-metal arm64 images for the Samsung Galaxy S9+
**Exynos 9810** (SM-G965F/DS). They run from the Samsung S-Boot handoff,
write text to the firmware framebuffer, and request a reboot into recovery.
They do not write storage, EFS, regulators, the MMU, or the bootloader.

**The current tested path is a repacked One UI stock BOOT image flashed
through Odin's AP slot.** The older TWRP/LineageOS instructions are not the
validated procedure for the current S9+ test.

## Build

GitHub Actions is the recommended route if you do not have an AArch64
cross-toolchain installed:

```sh
gh workflow run star2lte-probe.yml -R ccnar0/ios-star2lte
gh run watch -R ccnar0/ios-star2lte
gh run download -R ccnar0/ios-star2lte -n star2lte-probe -D out
```

The artifact contains both `probe.Image` and `memmap.Image`.

To build locally on Linux/WSL with `gcc-aarch64-linux-gnu`:

```sh
sh shim/star2lte/build.sh
```

This creates `shim/star2lte/out/probe.Image` and
`shim/star2lte/out/memmap.Image`. The build does not package or flash
anything.

## Before flashing

1. Confirm the phone is the **Exynos SM-G965F/DS** variant. Do not use
   these images on Snapdragon S9+ models.
2. Keep the untouched stock One UI AP firmware and its original BOOT image
   available. Verify that you can restore the stock AP with Odin before
   experimenting.
3. Keep all stock firmware, extracted images, generated BOOT images and
   tar files outside the Git repository.
4. Use the One UI stock `boot.img` extracted from the AP firmware as the
   packer's input. Do not substitute a LineageOS/TWRP boot image for the
   procedure recorded in the project progress log.

## Pack a probe for Odin

First inspect the stock image:

```sh
python3 tools/star2lte-pack-boot.py info STOCK_BOOT.img
```

Pack either diagnostic image (use a fresh output filename each time):

```sh
python3 tools/star2lte-pack-boot.py pack STOCK_BOOT.img \
  shim/star2lte/out/probe.Image probe-oneui-boot.img \
  --partition-size 67108864

# Or, for the memory-map capture:
python3 tools/star2lte-pack-boot.py pack STOCK_BOOT.img \
  shim/star2lte/out/memmap.Image memmap-oneui-boot.img \
  --partition-size 67108864
```

Put the generated image in a tar archive with the member name
`boot.img` (Odin expects the partition filename, not the generated
filename):

```sh
mkdir -p odin-ap
cp probe-oneui-boot.img odin-ap/boot.img
tar -cf star2lte-probe-boot.tar -C odin-ap boot.img
```

For the memory-map image, copy `memmap-oneui-boot.img` instead and use a
different tar filename.

In Odin, load the generated tar in the **AP** slot. Do not select
Re-Partition, and do not flash PIT or other partitions. The packer is a
host-side image tool; it never flashes the device itself.

## Probe v2: handoff report

The v2 probe displays the S-Boot entry state, X0-X3, the DTB address and
size, CPU identity, timer frequency, and page-size capability. It waits
180 seconds and then requests recovery.

Photograph the full report. The v2 results already recorded in
`STAR2LTE_PROGRESS.md` confirm EL1 entry, the DTB at `0x8A000000`,
kernel load at `0x80150000`, and a zero `CNTFRQ_EL0` value.

## Memory-map probe (`memmap.Image`)

This is the next on-device step; it has **not yet been flashed or run on
the phone**. It walks the FDT passed in X0 and lists:

- `M`: DRAM bank from `/memory*`
- `R`: fixed `/reserved-memory/*` range
- `D`: size-only dynamic reserved pool
- `V`: FDT header memory-reservation entry

It shows 16 records per page, changes page every 20 seconds, and runs for
300 seconds before requesting recovery. Photograph every page, including
the page counter and any dropped-record count. The `INITRD S/E` lines
show `/chosen` initrd bounds when present.

Do not choose an XNU RAM window or persistent log address from the
mainline device tree alone. Record the live map first; then check that the
chosen window does not overlap the DTB, initrd, reserved regions, or
framebuffer.

## Host test for the FDT memory walker

From the repository root on Linux/WSL with `dtc` and GCC:

```sh
dtc -I dts -O dtb shim/star2lte/test_memmap.dts -o /tmp/star2lte-test.dtb
gcc -std=c11 -O2 -Wall -Wextra -Werror \
  shim/star2lte/host_fdtmem_test.c shim/star2lte/fdtmem.c \
  -o /tmp/star2lte-fdtmem-test
/tmp/star2lte-fdtmem-test /tmp/star2lte-test.dtb
```

The synthetic DTB tests the parser, not the phone's S-Boot DTB. A passing
host test is not evidence that the memory map has been captured on-device.

## Recovery and scope

If the phone does not boot after a test, restore the untouched stock AP
firmware with Odin using the known-good recovery procedure. Do not
experiment with EFS, modem, USERDATA, PIT, or partition tables as part of
this stage.

This project is experimental boot-chain/kernel research, not a usable
iOS port. XNU has not yet been ported to or booted on star2lte.
