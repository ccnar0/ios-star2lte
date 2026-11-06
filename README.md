# iOS 16 bring-up for Samsung Galaxy S9+ (star2lte)

**Target:** Samsung Galaxy S9+ Exynos, **SM-G965F / SM-G965FD**, SoC Exynos 9810.  
**Current stage (2026-11-06):** Stage 0 boot-handoff probing is confirmed on the physical phone. The next task is to read the live S-Boot memory map before selecting a safe XNU RAM window. **XNU has not been ported to or booted on star2lte yet.**

This repository contains earlier HEROLTE (Galaxy S7) and A5XELTE work as reference material. Those targets are not the current device. The active S9+ work lives in `shim/star2lte/`, `tools/star2lte-*`, `.github/workflows/star2lte-probe.yml`, and [STAR2LTE_PROGRESS.md](STAR2LTE_PROGRESS.md).

## Current status

| Area | Status |
| --- | --- |
| Device identification | Exynos 9810 / SM-G965F and SM-G965FD target; Snapdragon variants are not supported |
| Boot chain | Odin AP can install a repacked BOOT image; S-Boot executes our bare-metal arm64 probe |
| Entry state | On-device probe confirms EL1, MMU off, caches on |
| Device tree handoff | Confirmed: S-Boot passes a valid DTB in X0 at `0x8A000000` (303,771 bytes) |
| Kernel load address | Probe v2 reports `0x80150000` |
| Timer | `CNTFRQ_EL0` is zero; measured probe uses the 26 MHz fallback from the device tree |
| Page-size capability | 16 KiB translation granule is supported; the XNU base configuration is still 4 KiB |
| Memory map | FDT walker passes host tests; **live phone memory/reserved ranges still need to be captured** |
| XNU machine config | Not started |
| XNU loader, UFS and DECON/DSIM drivers | Not started |
| iOS userland | Not running on this device |

The complete evidence, probe measurements, source-derived hardware facts, and unresolved questions are tracked in [STAR2LTE_PROGRESS.md](STAR2LTE_PROGRESS.md). Build success, source-derived assumptions, and on-device results are intentionally kept separate.

## Device facts

The initial hardware map was derived from the user's LineageOS 23.2 boot image and Samsung Exynos 9810 device-tree sources. Important values include:

- CPU clusters: four Cortex-A55 cores and four Mongoose M3 cores.
- GIC-400 at `0x10101000` / `0x10102000`.
- 26 MHz architectural timer.
- UFS 2.1 controller at `0x11120000`, with vendor, UniPro, and protector blocks.
- DECON_F at `0x16030000`, DSIM at `0x16080000`, DPHY at `0x16160000`.
- Display: 1440 × 2960; framebuffer address `0xCC000000` is source-derived and must be validated against the live memory map.
- Touch controller: Samsung sec_ts at I²C address `0x48`.

These are not all confirmed runtime mappings. In particular, the usable DRAM window, reserved regions, UFS register sizing, and nominal peripheral clocks must be validated before the XNU loader or drivers use them.

## Stage 0: boot-handoff probe

The probe is a small bare-metal arm64 Image packed into a copy of the stock Samsung BOOT image. It reports firmware handoff state on the display and then reboots into recovery.

The v2 probe was run on the physical SM-G965F:

| Field | Observed value |
| --- | --- |
| Exception level | EL1 |
| `SCTLR_EL1` | `0x30C5183E` (MMU off; I-cache and D-cache on) |
| Image load address | `0x80150000` |
| X0 | `0x8A000000` (valid FDT magic `D00DFEED`) |
| FDT size | `0x4A29B` |
| `CNTFRQ_EL0` | `0` |
| Boot CPU | Cortex-A55 r1p1, MPIDR affinity 0 |
| 16 KiB granule | Supported |
| Previous DECON trigger | `0x3070` |

The probe does not write storage, EFS, regulators, the MMU, or the bootloader. It does request a PMU reset into recovery after its report. The phone's display has a known fault in its top half, so the report is positioned in the working lower area.

## Next step: live memory-map capture

`shim/star2lte/fdtmem.c` walks the FDT for DRAM banks, reserved-memory nodes, header reservations, and `/chosen` initrd ranges. It passes a host test using a synthetic DTB. The resulting `memmap.Image` has **only been built and host-tested; it has not yet been flashed or run on the phone**.

After collecting the live S-Boot map, record every DRAM and reserved range in [STAR2LTE_PROGRESS.md](STAR2LTE_PROGRESS.md). Only then choose the XNU RAM window and persistent log location. Do not reuse the old HEROLTE loader's assumed addresses without validating them on star2lte.

## Build the probe

The GitHub Actions workflow builds the probe on Ubuntu:

```sh
gh workflow run star2lte-probe.yml -R ccnar0/ios-star2lte
gh run watch -R ccnar0/ios-star2lte
gh run download -R ccnar0/ios-star2lte -n star2lte-probe -D out
```

Or build locally on Linux/WSL with the AArch64 GNU cross compiler:

```sh
sh shim/star2lte/build.sh
```

See [shim/star2lte/README.md](shim/star2lte/README.md) for probe packaging, Odin AP testing, recovery instructions, and the memory-map probe procedure. Always keep the untouched stock BOOT/AP firmware available as the recovery path and verify the exact device variant before testing.

## Repository map

| Path | Purpose |
| --- | --- |
| `shim/star2lte/` | S9+ bare-metal handoff and memory-map probes |
| `tools/star2lte-pack-boot.py` | Packs a probe Image while preserving the Samsung BOOT structure |
| `tools/mkadt_star2lte.py` | Experimental Apple Device Tree generator; requires a validated DRAM window |
| `.github/workflows/star2lte-probe.yml` | CI build for the S9+ probes |
| `STAR2LTE_PROGRESS.md` | Device facts, test evidence, current blockers, and next steps |
| `shim/herolte/`, `tools/*herolte*` | Galaxy S7 reference implementation; not the S9+ target |
| `shim/a5xelte/`, `docs/a5xelte-*` | Galaxy A5 2016 reference work |

## Safety and scope

- This is experimental kernel/boot-chain research, not a usable phone OS.
- Do not use these instructions on Snapdragon S9+ variants or other Samsung models.
- Back up the original boot image and preserve a known-good stock AP firmware before any boot-image test.
- Do not write to EFS, modem, USERDATA, or partition tables as part of the current probe stage.
- Keep device-specific backups, DTBs, calibration data, firmware images, and generated iOS root images out of Git.
- Apple's iOS firmware and userland are not included in this repository. Do not redistribute Apple's copyrighted images.

## Credits

The work builds on Apple's published XNU sources, Samsung/LineageOS Exynos device-tree and kernel sources, and the existing HEROLTE/A5XELTE research in this repository. See individual source files and patch headers for component-specific attribution and licenses.
