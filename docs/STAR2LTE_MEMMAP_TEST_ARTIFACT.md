# STAR2LTE memory-map test package record

Updated: 2026-11-06

## Status

The device-specific Odin AP package was booted on the physical SM-G965F. The user uploaded both photographed pages from the 2026-11-06 run under `test-results/memmap-2026-11-06/`. This is physical-device evidence for the memory-map probe only; it is not an XNU boot.

The generated image is a diagnostic bare-metal memory-map probe, not XNU and not an iOS ROM.

## Inputs

- Stock compressed BOOT input: user-provided `boot.img.lz4` (not committed).
- Decompressed Android BOOT image SHA-256: `4049d484c2a5c7368eef51aac61d352a9ba3b491c46d80f2051397f362070419`.
- Probe source artifact: `memmap.Image` from Actions run [42226798280](https://github.com/ccnar0/ios-star2lte/actions/runs/42226798280).
- Probe image SHA-256: `dfb3aef26f984b6540ab7165d746340961c6231162f254e76ea5ec09489b9597`.

## Generated outputs

- Candidate BOOT image: `star2lte-memmap-boot.img`
  - Size: 2,744,336 bytes
  - SHA-256: `472165ec4b4eba6e9f85e3aa424bf2b5d3bcfc724a9913860c180bd9baf7ade4`
- Odin archive: `star2lte-memmap-odin.tar`
  - Size: 2,754,560 bytes
  - SHA-256: `af09dd9409f064c61b9ec82cb1b4d1c055d5f5599f712bd54b43f33db877aa4d`
  - Contains exactly one root member named `boot.img`; member bytes were checked against the generated BOOT image.
- Device BOOT partition capacity from the user's TWRP report: 56,320 KiB (57,671,680 bytes). Candidate image is smaller than this capacity.
- The repacker preserved the stock ramdisk, second component and Samsung DTBH while replacing the kernel Image; the Android boot header SHA-1 and Samsung footer were regenerated/validated.

## Reproduction and device-test status

Use the repository's `tools/star2lte-pack-boot.py` with the matching stock BOOT image and CI `memmap.Image`; do not use another model/firmware's BOOT. The exact command line should be taken from `shim/star2lte/README.md` and the current script.

The generated stock-derived BOOT image and Odin TAR are intentionally **not committed**. They contain stock firmware components and are device/firmware-specific. Keep them as local test artifacts; commit source, scripts, workflow definitions and non-sensitive test notes only.

Physical test completed and reviewed. The two pages show 20 records total, including five RAM (`M`) ranges, 11 fixed reservations (`R`), and four dynamic pools (`D`). The five RAM ranges are:

- `0x80000000 + 0x3C800000`
- `0xC0000000 + 0x20000000`
- `0xE1900000 + 0x1E700000`
- `0x880000000 + 0x80000000`
- `0x900000000 + 0x80000000`

Total reported DRAM: 6,063 MiB (~5.92 GiB). The FDT pointer is `0x8A000000`. The bottom lines are legible in the second photo: `INITRD S = 0x89900000`, `INITRD E = 0x8990C366` (end-exclusive, size `0xC366` / 50,022 bytes). This initrd lies below the FDT at `0x8A000000`; no safe XNU RAM window has been selected yet. The photos show no visible dropped-record indicator. The diagnostic probe's physical run does not mean XNU is bootable.

If repeating the test:
1. Verify the handset is the Exynos SM-G965F/SM-G965FD variant and the input BOOT matches its firmware.
2. In Odin select AP only; do not select Re-Partition or PIT.
3. Keep untouched stock firmware/BOOT available for recovery.
4. Photograph every displayed memory-map page and record the result in `STAR2LTE_PROGRESS.md`.
5. Keep the uploaded original photos; repeat only if a clearer initrd/reservation capture is needed.

No XNU kernel was included in this package and no XNU boot is claimed.
