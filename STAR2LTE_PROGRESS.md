# STAR2LTE progress (Galaxy S9+, SM-G965F/DS, Exynos 9810)

Keep three kinds of claims separate: **source-derived**, **build success**,
**on-device evidence**. Nothing below is an on-device result yet.

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
- Probe v2 (this commit) prints X0-X3 and the word at each, plus a bounded
  FDT scan. Its on-device report is pending.

## Open questions after the probe v1 run

- Answered: S-Boot boots a raw Image with this header (`SEANDROIDENFORCE`
  only, unlocked bootloader); the panel is driven at `0xcc000000`; the
  countdown and PMU reset work; 16 KiB pages are supported.
- Still open: EL (EL1 vs EL2), the boot core's MPIDR, CNTFRQ, and what
  S-Boot actually puts in X0-X3. Probe v2 reports all of these.
- Whether S-Boot loads a DTB of its own; if not, the port uses the
  extracted `star2lte.dtb` directly.
