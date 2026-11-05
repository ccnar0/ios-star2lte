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
(dt size `0x4a000`). S-Boot passes the FDT in X0 and the initrd at
`0x82000000-0x821fffff` (32 MiB).

## Stage 0: handoff probe

Added `shim/star2lte/` (probe.S, probe.c, fbtext.[ch], image.ld,
build.sh) and `tools/star2lte-pack-boot.py`.

The probe prints, on the firmware framebuffer:

- EL, SCTLR_EL1, FDT pointer, FDT magic/size, loaded address
- MIDR, MPIDR (which core S-Boot booted), MMFR0, PFR0, ISAR0, CNTFRQ
- previous DECON trigger value, initrd start/end
- whether 16 KiB pages (`TGran16`) are supported

Then it waits 90 s, writes PMU `SYSIP_DAT0 = 0xf` and `SWRESET = 1`
(Exynos 9810) to reboot into TWRP. It performs no storage, EFS,
regulator, MMU or bootloader writes.

CI: `.github/workflows/star2lte-probe.yml` builds `probe.Image` on
Ubuntu with `gcc-aarch64-linux-gnu` and uploads the `star2lte-probe`
artifact. Pack locally with:

```sh
python3 tools/star2lte-pack-boot.py pack STOCK_BOOT.img \
    shim/star2lte/out/probe.Image star2lte-probe-boot.img
```

Then back up BOOT and flash only BOOT from TWRP.

## Open questions for the first on-device run

- Does S-Boot boot a raw Image packed with this header, or does it
  enforce a signature beyond `SEANDROIDENFORCE`?
- Actual MPIDR of the boot core and the FDT address S-Boot passes.
- Whether `0xcc000000` is live on star2lte (confirmed for starlte).
- CNTFRQ (expect 26 MHz) and whether the countdown reaches TWRP.
- Real BOOT partition size (`blockdev --getsize64`).
- `/dev/block/by-name/BOOT` path in TWRP (S7 used
  `/dev/block/platform/155a0000.ufs/by-name/BOOT`; the S9 may differ).
