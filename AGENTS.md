# ios-star2lte — iOS 16 on the Galaxy S9+ (Exynos 9810)

Status: fresh port. This tree is a snapshot of `wardevsa/ios-a5xelte` (`herolte`,
Galaxy S7, Exynos 8890); no star2lte/Exynos 9810 board code exists yet. S7
constants are a starting point, not facts for this phone.

## Target, and which references apply

- Target is `star2lte` = SM-G965F/DS, **Exynos 9810**. Confirm the exact model
  before anything destructive.
- Do **not** use `syncmodz/ios-s9` (`starqltechn`, SDM845) or the donor
  ios-sweet Qualcomm drivers: different SoC, storage, display, PMIC and touch.
- Local reference trees: `C:\Users\ccnar\ios-s7` (herolte base, same Exynos
  approach) and `C:\Users\ccnar\ios-sweet-s9` (donor).
- Real star2lte boot/recovery artifacts (LineageOS/TWRP) are in
  `C:\Users\ccnar\Downloads`; extract the live DTB from them for addresses.
  Do not commit those files.

## Safety and flashing

- Samsung S-Boot, no `fastboot boot`. Trial kernels go to the BOOT partition
  only, flashed from TWRP. Confirm the by-name path in TWRP (the S7 uses
  `/dev/block/platform/155a0000.ufs/by-name/BOOT`).
- Never write EFS, RECOVERY, PARAM or bootloader partitions. Back up BOOT and
  EFS first. Erasing/installing into USERDATA needs explicit user confirmation.
- Keep device dumps, serials, Apple IPSWs/root images and extracted firmware
  out of git.

## Build and test

- Kernel: `.github/workflows/xnu.yml` on a macOS runner.
  `gh workflow run xnu.yml -R <your-fork> -f machine_config=STAR2LTE`,
  then download the `xnu-kernel-macos-15-intel` artifact; the output is
  `kernel.development.star2lte`.
- Add `STAR2LTE` beside `A5XELTE`/`HEROLTE` in **both** workflow checks:
  the firehose flags (~line 68) and the LSE scan (~line 189). Without the
  first, `libfirehose_kernel` is built arm64e-only and the build fails.
- Shim/loader: `shim/star2lte/build.sh` (copy `shim/herolte/`), needs
  `aarch64-linux-gnu-gcc` or `aarch64-none-elf-gcc`. Neither is installed on
  this Windows host; use WSL/Linux or CI.
- `mkbootimg` is not installed and is not the right tool for Samsung images.
  Pack with `tools/herolte-pack-boot.py` (make a star2lte copy) against a
  stock star2lte `boot.img`, then flash the result from TWRP.
- ADT: create `tools/mkadt_star2lte.py`; never reuse `mkadt_herolte.py`
  addresses or the S7 framebuffer at `0xe2a00000`.

## Repo conventions and pitfalls

- `xnu/patches/` is applied in numeric order by CI. Append board work at 0320+;
  never renumber. `xnu/patches-pending/` is not applied.
- The base is 4 KiB pages, 39-bit kernel VA, Cortex-A53 codegen, GICv2 and no
  LSE atomics (inherited from A5XELTE/HEROLTE). Verify the 9810's GIC before
  relying on GICv2. Changing page size/VA/codegen is kernel-wide work, not a
  board tweak.
- Patch files must stay LF. This Windows checkout has `core.autocrlf=true` and
  no `.gitattributes`, so local `git apply` can fail on CRLF patches; check
  with `git apply --check` under WSL before spending a CI run.
- `NOTES.md` and `HEROLTE_PROGRESS.md` are donor/S7 history. Record this port in
  `STAR2LTE_PROGRESS.md` (create it) and keep source-derived guesses, build
  success and on-device evidence as separate claims.
- Commit completed changes and update `STAR2LTE_PROGRESS.md` before stopping.

## Bring-up order

1. Extract the star2lte DTB; record memory map, GIC, timer, UFS, DECON/DSIM,
   touch, PMIC, GPIO keys and the S-Boot framebuffer address.
2. Add the `STAR2LTE` XNU machine config derived from
   `0200-herolte-board-target.patch`.
3. Shim probe on the phone (screen text, EL/MPIDR/CNTFRQ/FDT).
4. XNU to console with the new ADT; then UFS root, display, touch/keys/battery.
