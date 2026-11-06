# STAR2LTE stage-0 probes and XNU preflight

These are diagnostic bare-metal arm64 images for the Samsung Galaxy S9+
Exynos 9810 (SM-G965F/DS). They run from the Samsung S-Boot handoff and use
the firmware framebuffer. They do not write storage, EFS, regulators, the
MMU, or the bootloader. The preflight test also leaves the MMU disabled.

## Build

Run the GitHub Actions workflow **star2lte-probe**. Its artifact contains:

- `probe.Image`: raw S-Boot handoff register/CPU probe.
- `memmap.Image`: live FDT memory-map capture.
- `preflight.Image`: CPU0/EL1/MMU-off/4 KiB/FDT/initrd/image/framebuffer
  range validation before any XNU handoff is attempted.

The images are raw arm64 Image payloads, not flashable ZIPs or Android boot
images. Package one with the existing host-only `tools/star2lte-pack-boot.py`
against the untouched stock One UI BOOT image, as documented below.

## XNU preflight acceptance

The preflight image checks:
1. CPU affinity 0, EL1 entry, and MMU disabled.
2. 4 KiB translation granule support.
3. FDT header and complete FDT extent inside a live DRAM bank.
4. initrd start/end from `/chosen`, with the full range inside a live DRAM bank.
5. FDT, initrd, and loaded image do not overlap.
6. the known S-Boot framebuffer extent is inside a live DRAM bank.

It prints each result and holds the screen for 180 seconds, then returns to
the existing tested recovery reboot path. **PASS is only a loader preflight;
it is not an XNU kernel boot.** It does not allocate an XNU RAM window, create
page tables, enter XNU, start secondary cores, or access UFS.

The known physical photo values were initrd `0x89900000..0x8990C366` and
FDT `0x8A000000..0x8A04A29B`. The test reads the current values from the
live FDT rather than hard-coding those values.

## Flash safety

Use only on Exynos SM-G965F/DS. Keep the untouched stock AP/BOOT firmware.
Flash a repacked candidate through Odin AP only, with Re-Partition disabled;
never flash PIT, EFS, modem, or USERDATA for these tests. Keep the known-good
stock AP ready for recovery. The packer does not flash the device.
