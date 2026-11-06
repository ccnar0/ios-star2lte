# ios-star2lte — Galaxy S9+ (star2lte), Exynos 9810

## Active target and repository layout

The active target is the Samsung Galaxy S9+ Exynos variant, SM-G965F/DS
(star2lte). Snapdragon S9+ (starqltechn/SDM845) is not compatible.

This repository also contains substantial historical/reference work for
Galaxy A5 2016 (a5xelte), Galaxy S7 (herolte), and other donor targets.
Do not treat those board-specific addresses, drivers, patches, or boot
procedures as valid for star2lte without independent verification.

Active star2lte work:
- `shim/star2lte/`: S-Boot handoff and FDT memory-map probes.
- `tools/star2lte-pack-boot.py`: host-side Samsung BOOT image packer.
- `tools/mkadt_star2lte.py`: experimental ADT generator; not XNU-boot-tested.
- `.github/workflows/star2lte-probe.yml`: probe build and host parser test.
- `STAR2LTE_PROGRESS.md`: source facts, build evidence, on-device results.

## Verified device evidence

The stage-0 v2 probe has run on the physical SM-G965F:
- S-Boot enters at EL1, with the MMU off and caches on.
- X0 points to a valid S-Boot FDT at 0x8A000000.
- The probe image was loaded at 0x80150000.
- CNTFRQ_EL0 reads zero; the 26 MHz timer value is source-derived fallback.
- The v2 probe displayed its report and requested a reboot into recovery.

The FDT memory-map image has only been built and host-tested. It has NOT
been flashed or run on the phone. The live DRAM/reserved-memory map is
still the immediate blocker.

XNU has not been ported to or booted on star2lte. There is no completed
STAR2LTE XNU machine configuration, XNU loader, UFS driver, or display
driver. Do not describe this project as a working iOS port.

## Flashing and recovery safety

The currently tested procedure is a repacked One UI stock BOOT image
flashed through Odin's AP slot. The older TWRP/LineageOS instructions in
historical notes are not the current validated procedure.

- Confirm the exact Exynos SM-G965F/DS model before any device test.
- Preserve the untouched stock AP firmware and original BOOT image.
- Do not select Odin Re-Partition and do not flash PIT or unrelated
  partitions.
- Never write EFS, modem, USERDATA, RECOVERY, PARAM, or bootloader
  partitions as part of the current probe work.
- Keep firmware images, DTBs, device dumps, calibration data, generated
  BOOT images, and Apple IPSWs/root images out of Git.
- The packer is host-side only; it does not flash a device.
- Any new device write or change to the boot/recovery sequence requires
  explicit review. Prefer read-only probes and a known stock recovery path.

See `shim/star2lte/README.md` for the exact current packaging steps and
`STAR2LTE_PROGRESS.md` for evidence and unresolved assumptions.

## Build and test

The supported probe CI workflow is:

```sh
gh workflow run star2lte-probe.yml -R ccnar0/ios-star2lte
gh run watch -R ccnar0/ios-star2lte
gh run download -R ccnar0/ios-star2lte -n star2lte-probe -D out
```

It builds both `probe.Image` and `memmap.Image`, runs the synthetic-DTB
host test, and uploads the two raw arm64 Images. A passing host test does
not validate the live phone DTB or prove the memory map.

The XNU workflow currently has historical machine configurations and
must not be assumed to support STAR2LTE. Add and validate a dedicated
machine configuration and its required compiler/architecture checks
before attempting a STAR2LTE XNU build.

## Repository conventions

- `xnu/patches/` is applied in numeric order by the XNU workflow. Never
  renumber existing patches; use the next unused range only after the
  target-specific patch plan is reviewed.
- `xnu/patches-pending/` is not applied by the normal build.
- Keep patch files LF and run `git apply --check` before spending a CI run.
- Keep source-derived assumptions, successful builds, host tests, and
  physical-device observations explicitly separated in progress notes.
- Historical `NOTES.md`, `HEROLTE_PROGRESS.md`, and `A5XELTE_PROGRESS.md`
  describe other bring-up efforts; do not copy their device-specific
  conclusions into star2lte without verification.

## Bring-up order

1. Capture the live S-Boot DTB memory and reserved-memory map using
   `memmap.Image`; record every page and dropped-record count.
2. Choose a safe XNU RAM window only after checking DRAM, reserved
   ranges, DTB, initrd, framebuffer, and probe/kernel load locations.
3. Validate the GIC, timer, CPU topology, and peripheral register ranges
   against star2lte sources and on-device evidence.
4. Design and review a dedicated STAR2LTE XNU machine configuration.
5. Bring up a minimal XNU console before adding UFS, display, touch,
   power, and other drivers.
