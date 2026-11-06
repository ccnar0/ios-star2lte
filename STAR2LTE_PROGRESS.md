# STAR2LTE progress (Galaxy S9+, SM-G965F/DS, Exynos 9810)

Keep three kinds of claims separate: **source-derived**, **build success**,
and **on-device evidence**. The handoff probe v1/v2 sections below contain
physical-device observations; the memory-map probe section is still build
and host-test evidence only.

## Device facts from the user's LineageOS 23.2 `boot.img` DTB

Extracted from `lineage-23.2-*-star2lte.zip` (not in git):

| Item | Value | Source |
| --- | --- | --- |
| SoC | Exynos 9810 (`samsung,exynos9810`) | `/compatible`, DTBH chip 9810 |
| Board id | platform `0x50a6`, subtype `0x217584da` | DTBH entry 0 |
| CPUs | A55 `cpu@0000-0003` (MPIDR 0x000-0x003), Mongoose M3 `cpu@0100-0103` (0x100-0x103) | `/cpus/cpu@*` |
| GIC | GIC-400, distributor `0x10101000`, CPU `0x10102000` | `/interrupt-controller@10100000` |
| Timer | 26 MHz (`0x18cba80`), virtual INTID 27, physical INTID 26 | `/timer` |
| PSCI | SMC, `cpu_on 0xc4000003`, `cpu_off 0x84000002` | `/psci` |
| PMU / reset | `0x14060000`; recovery = `SYSIP_DAT0 (0x810) = 0xf`, then `SWRESET (0x400) = 1` | `/reboot` + LineageOS `exynos-reboot.c` |
| UFS | HCI `0x11120000`, vendor HCI `0x11121100`, UNIPRO `0x11110000`, protector `0x11130000`, IRQ 242, UFS 2.1 | `/ufs@286392320` |
| Display | DECON_F `0x16030000`, DSIM `0x16080000`, DPHY `0x16160000`, 1440x2960 | `/decon_f@...`, `/dsim@...` |
| Framebuffer | `0xcc000000`, 1440x2960, stride 5760, ARGB8888 | mainline `exynos9810-starlte.dts`, uniLoader `board-starlte.c` |
| DECON trigger | `0x16030000 + 0x70 = 0x1281` | uniLoader `board-starlte.c` |
| Touch | `sec,sec_ts` at 0x48 on `/hsi2c@104B0000`, project `Star2 G965`, fw `tsp_sec/y761_star2.fw` | DTB |
| PMIC | `samsung,s2mps18mfd@00` on SPEEDY `0x141C0000` | DTB |
| Keys | power `gpa2-4`, vol-down `gpa0-4`, vol-up `gpa0-3`, Bixby/wink `gpa0-6` | DTB + mainline starlte DTS |
| RAM | `0x80000000+0x3c800000`, `0xc0000000+0x20000000`, `0xe1900000+0x1e700000`, `0x8_80000000+0x80000000` | mainline starlte DTS |

Boot image layout from the same `boot.img`: page 2048, kernel at
`0x10008000`, ramdisk at `0x11000000`, tags at `0x10000100`, arm64
`text_offset 0x80000`, flags `0xa`, DTBH v2 with one chip-9810 entry
(dt size `0x4a000`). S-Boot is *assumed* to pass the FDT in X0 and the
initrd at `0x82000000-0x821fffff` (32 MiB); the probe v1 run saw
`FDT BAD`, so the X0 assumption is not yet confirmed on this device.

## Stage 0: handoff probe

Added `shim/star2lte/` (probe.S, probe.c, fbtext.[ch], image.ld,
build.sh) and `tools/star2lte-pack-boot.py`.

v1 prints, on the firmware framebuffer: EL, SCTLR_EL1, FDT pointer,
loaded address, MIDR, MPIDR, MMFR0, PFR0, ISAR0, CNTFRQ, the previous
DECON trigger value, initrd start/end and the `TGran16` result, then
holds 90 s.

v2 keeps those but also dumps all four entry argument registers (X0-X3),
the 32-bit word each one points at, and the legacy ATAGS word at
`0x10000100`, then holds 180 s. Pointer reads are limited to RAM ranges
the DTB lists plus, while running from the low alias, the 32 MiB the
stock kernel occupied; an unmapped read with the MMU off would abort.

Both versions finish by writing PMU `SYSIP_DAT0 = 0xf` and `SWRESET = 1`
(Exynos 9810) to reboot into recovery. No storage, EFS, regulator, MMU or
bootloader writes.

CI: `.github/workflows/star2lte-probe.yml` builds `probe.Image` on
Ubuntu with `gcc-aarch64-linux-gnu` and uploads the `star2lte-probe`
artifact. Pack locally against the stock OneUI `boot.img` and flash
through Odin's AP slot (no TWRP on the phone):

```sh
python tools/star2lte-pack-boot.py pack research/star2lte/oneui-boot.img \
    shim/star2lte/out/probe.Image research/star2lte/probe-oneui-boot.img \
    --partition-size 67108864
```

Put `probe-oneui-boot.img` in a tar as `boot.img` and flash it as AP.
`star2lte-stock-boot.tar` (the untouched `boot.img.lz4` from the same AP
firmware) is the undo path, also flashed as AP.

## Build status and on-device evidence (2026-11-06)

- CI `star2lte-probe` runs **42122702768**, **42123037232** and
  **42123564845** passed on `ubuntu-latest`. Artifact `star2lte-probe` =
  `probe.Image` (v1, 10,960 bytes).
- The panel has a hardware fault in the **top half**, so run 42123564845
  moved the report to `TOP=1560`; 20 lines end by y=2682.
- v1 packed against the OneUI 11 stock `boot.img` (`G965FXXUHFVG4`):
  `probe-oneui-boot.img`, 2,738,192 bytes, SHA-256
  `0d4dd2c8621f1092524960248ee050c16615c9e0f46d467636812e0b8886e86b`;
  kernel 10,960 B, stock ramdisk 901,990 B, DTBH v2 with six chip-9810
  entries, `SEANDROIDENFORCE` tail. Packaged as `star2lte-probe-boot.tar`
  for Odin AP.
- **On-device (probe v1):** Odin accepted the tar in AP and S-Boot booted
  the raw Image, so with the bootloader unlocked the `SEANDROIDENFORCE`
  marker is enough; no extra signature check was seen.
- The report appeared in the working bottom half and the countdown ran to
  zero, after which the phone rebooted by itself (PMU reset works).
- Reported values: `FDT BAD`, `INITRD ?`, `16K PAGES: YES`. EL was not
  recorded; X0-X3 were not printed by v1.
- `FDT BAD` is treated as the v1 range check being too narrow (it rejected
  any pointer below `0x80000000`, while the header load address is
  `0x10008000`), not as proof that S-Boot passes no DTB.
- Probe v2 prints X0-X3 and the word at each. Its physical-device report
  is recorded in the following section.
- v2 `probe.Image`: 11,984 bytes; CI run **42134174676** passed. The image
  was packed and run on the physical phone; its measured results are recorded
  below. The current CI run **42226798280** also rebuilt both images and
  passed the host FDT parser test.

## On-device probe v2 results (2026-11-06)

Flashed `star2lte-probe-boot.tar` (v2, packed against OneUI) as Odin AP;
S-Boot booted it and the report was photographed.

| Field | Value | Meaning |
| --- | --- | --- |
| EL | `1` | S-Boot enters at **EL1**; no EL2 drop needed |
| SCTLR_EL1 | `0x30C5183E` | MMU off (M=0), I- and D-cache on |
| IMAGE | `0x80150000` | actual load address, not `0x10008000` |
| X0 | `0x8A000000` | **DTB pointer**; the word there is `D00DFEED` |
| FDTSZ | `0x4A29B` (303,771 B) | S-Boot's own DTB, about one `0x4A000` DTBH entry |
| X1 | `0x80000000` | first word there is 0 |
| X2 | `0x150000` | unknown |
| X3 | `0x80150000` | kernel base again |
| CNTFRQ_EL0 | `0` | firmware left it unset; use 26 MHz (`0x18CBA80`) |
| MIDR | `0x410FD051` | Cortex-A55 r1p1 boot core |
| MPIDR | `0x80000000` | CPU0 (bit31 RES1, affinity 0) |
| TGran16 | YES | 16 KiB pages supported; port still uses the 4 KiB base |
| TRIG | `0x3070` | previous DECON trigger value |

- `FDT BAD` in the v1 run did not reproduce: v2 found a valid FDT at
  `0x8A000000`. v1's range check would have accepted that address, so the
  v1 false negative is unexplained; the v1 run may have used the earlier
  LineageOS-packed image. Treat the DTB handoff as confirmed.
- `TAGS` read `----`: the image runs at `0x80150000`, so the legacy tag
  address `0x10000100` is outside the ranges the probe will dereference.
- The countdown used the 26 MHz fallback and the phone rebooted on time.

Implications for the XNU port:

- The trampoline stays at EL1; no EL2→EL1 transition is required.
- Use S-Boot's DTB at `0x8A000000` (X0), not only the extracted copy.
- Hardcode the 26 MHz counter frequency; `CNTFRQ_EL0` is not programmed.
- The kernel loads at `0x80150000` and must stay below 4 GiB for the
  39-bit kernel VA setup.

## Open questions for the XNU bring-up

- Whether the 4 KiB base, 39-bit VA and Cortex-A53 codegen carry over to
  the 9810 unchanged (A55 is ARMv8.2 but backwards compatible).
- GIC-400 at `0x10101000` with the existing GICv2 driver.
- UFS controller differences from the S7's exynos-ufs.
- DECON_F/DSIM differences from herolte.

## Memory-map probe and ADT builder (2026-11-06, build/host-test only)

Three kinds of claims, kept apart:

- **Host-tested:** `shim/star2lte/fdtmem.c` (FDT walker for `/memory*`,
  `/reserved-memory/*`, memreserve block, `/chosen` initrd) passes
  `host_fdtmem_test.c` against a synthetic DTB built with `dtc`. The first
  run caught a real bug (node names were truncated to 12 characters, so
  `reserved-memory` children were never read); fixed. The synthetic DTB is
  not S-Boot's DTB.
- **Build success:** current CI run **42226798280** builds `probe.Image`
  (11,984 B) and `memmap.Image` (18,416 B) with the Ubuntu AArch64
  cross-toolchain, `-Wall -Wextra -Werror`, no absolute relocations, and
  valid arm64 Image headers. The synthetic-DTB host test passes. The
  memory-map image is not yet packed, flashed, or run on the phone.
- **Source-derived:** `tools/mkadt_star2lte.py` builds an ADT from the
  facts in this file (GIC-400 `0x10101000/0x10102000`, A55 MPIDR 0x000-0x003
  as logical CPUs 0-3, Mongoose 0x100-0x103 as 4-7, 26 MHz timebase, UFS
  bases, FB `0xcc000000` 1440x2960). It ran here only with a synthetic
  `/product` node (the real D22AP ADT is an Apple artifact). UFS register
  sizes (one page each) and the nominal clocks are placeholders. It
  **requires** `--dram-base/--dram-size`; the XNU RAM window is not chosen
  yet because the reserved-memory map has not been read off the phone.

Next on-device step: pack `memmap.Image` like the probe (Odin AP, stock
`boot.img` undo path), photograph every page, record the DRAM banks and
reserved ranges here, then pick the XNU RAM window and the loader log page.
Not started: the `STAR2LTE` XNU machine config patch (0320+), the
`xnu_boot` loader, and the 9810 UFS/DECON drivers. Open: the old herolte
loader assumes a window below the DTB at `0x8A000000` and a log page at
`0x8ffff000`; neither is known to be free on this phone.

`tools/validate-repo.py` already reports a patch-sequence gap
(`0129` -> `0200`) inherited from the herolte base; unrelated to this work.


## External board-source cross-check (2026-11-06)

The LineageOS 24.0 kernel tree contains the exact `exynos9810-star2lte_eur_open_26.dts` board source. It uses root and reserved-memory cell widths of 2 address cells / 1 size cell, and documents fixed modem, CP log, audio, secure camera, Samsung crash-log, and camera-firmware reservations. It also marks TIMA and RKP regions disabled and has size-only dynamic pools. Importantly, this board DTS has no root DRAM `memory` node: firmware supplies that information. These values are static kernel-DTS cross-checks only; they do not replace the live S-Boot DTB capture and must not be used to choose the XNU RAM window.

Upstream `exynos9810.dtsi` corroborates the A55/Mongoose MPIDR layout, GIC-400, PSCI SMC calls, and 26 MHz timer. Upstream `starlte` and uniLoader sources are sibling-device references, not drop-in star2lte code. See [docs/STAR2LTE_REFERENCE_SOURCES.md](docs/STAR2LTE_REFERENCE_SOURCES.md) for the source links, address table, and limitations.
