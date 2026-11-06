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

## Memory-map probe (`memmap.Image`)

Next on-device step after probe v2. It walks the FDT S-Boot passes in X0
(`0x8A000000`) and lists every DRAM bank (`M`), reserved-memory node with a
base (`R`), size-only reserved pool (`D`) and header memory reservation
(`V`), 16 rows per page, flipping pages every 20 s for 300 s, then reboots
to recovery like `probe.Image`. Row format: `kind name base size` (hex).
`INITRD S/E` rows (20, 21) show `/chosen` initrd if present.

Build: `sh shim/star2lte/build.sh` (also made by the `star2lte-probe` CI job,
artifact `star2lte-probe`). Pack and flash exactly like `probe.Image`
(`tools/star2lte-pack-boot.py` against the stock OneUI `boot.img`, Odin AP;
`star2lte-stock-boot.tar` is the undo path). Photograph every page.

The walker (`fdtmem.c`) has a host test:

```sh
dtc -I dts -O dtb test_memmap.dts -o /tmp/t.dtb
gcc -O2 -Wall -Wextra host_fdtmem_test.c fdtmem.c -o /tmp/t && /tmp/t /tmp/t.dtb
```

Use the result to choose the XNU RAM window and loader/log addresses;
`tools/mkadt_star2lte.py` requires `--dram-base/--dram-size` for that reason.
