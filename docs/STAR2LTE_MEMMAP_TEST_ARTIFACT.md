# STAR2LTE memory-map test package record

Updated: 2026-11-06

## Status

A device-specific Odin AP test package was generated locally from the user's uploaded stock `boot.img.lz4` and the CI-built `memmap.Image`. This is a packaging/artifact-validation result only. The memory-map probe has **not** yet been physically tested on the phone.

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

Physical test remains pending. If testing:
1. Verify the handset is the Exynos SM-G965F/SM-G965FD variant and the input BOOT matches its firmware.
2. In Odin select AP only; do not select Re-Partition or PIT.
3. Keep untouched stock firmware/BOOT available for recovery.
4. Photograph every displayed memory-map page and record the result in `STAR2LTE_PROGRESS.md`.
5. Do not claim the map is validated until the phone has displayed it and the full output has been reviewed.

No XNU kernel was included in this package and no XNU boot is claimed.
