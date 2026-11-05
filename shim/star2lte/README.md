# star2lte stage-0 probe

`probe.Image` is a bare-metal arm64 Image packed into a Samsung S9+ BOOT
image. It prints the S-Boot handoff on the panel, then reboots into TWRP.
It does not touch storage, EFS, regulators, the MMU or the bootloader.

## Build

CI (recommended, no local toolchain):

```sh
gh workflow run star2lte-probe.yml -R ccnar0/ios-star2lte
gh run watch -R ccnar0/ios-star2lte
gh run download -R ccnar0/ios-star2lte -n star2lte-probe -D out
```

Local Linux/WSL with `gcc-aarch64-linux-gnu`:

```sh
sh build.sh            # writes out/probe.Image
```

## Pack against the stock BOOT

Use the LineageOS/TWRP `boot.img` that is currently on the phone. Keep it
and the output off Git.

```sh
python3 ../../tools/star2lte-pack-boot.py info STOCK_BOOT.img
python3 ../../tools/star2lte-pack-boot.py pack STOCK_BOOT.img \
    out/probe.Image star2lte-probe-boot.img
```

## Flash from TWRP (BOOT only)

1. Back up the current boot partition:
   - TWRP **Backup** -> select **Boot** -> swipe.
   - or `dd if=/dev/block/by-name/boot of=/sdcard/boot-stock.img`
     (confirm the real by-name path under `/dev/block/platform/*/by-name/`).
2. `adb push star2lte-probe-boot.img /sdcard/`
3. TWRP **Install** -> **Install Image** -> pick the file -> **Boot**.
4. Reboot to system. The probe draws its report, waits 90 s, then
   reboots into TWRP.

If the screen stays black, power-cycle with **Power + Vol Down + Bixby**
(hold ~10 s) and restore `boot-stock.img`. If the text appears but the
reboot does not happen, hold **Power + Vol Up + Bixby** to enter TWRP.

## What to photograph

All lines from `STAR2LTE PROBE 1` down to `TWRP EN ...`:

- `EL`, `SCTLR`, `FDT`, `FDT SIZE`
- `MIDR`, `MPIDR` (boot cluster/core)
- `MMFR0`, `PFR0`, `ISAR0`, `CNTFRQ`
- `TRIG` (previous DECON trigger value)
- `INITRD`, `INITRD END`
- `16K PAGES: YES/NO`

Those values decide the next step: the `STAR2LTE` XNU machine config,
the ADT memory layout, and whether the 4 KiB-page XNU is required.
