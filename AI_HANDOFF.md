# AI Handoff — STAR2LTE / XNU Bring-up

Last updated: 2026-11-06. This is a working handoff, not an authority over live repository state. Always inspect the current default branch, open PRs, Actions, README and progress log before changing anything.

## Project

- Repository: https://github.com/ccnar0/ios-star2lte
- Target: Samsung Galaxy S9+ Exynos 9810, `star2lte`; physical test evidence is from SM-G965F. SM-G965FD is also a stated target; verify variant and firmware before every device-specific operation.
- Goal: research and implement the boot/platform support required to bring up XNU-derived kernel components on this device.
- This is experimental research, **not a working iOS ROM**. The XNU kernel has not been ported to or booted on STAR2LTE.

## Mandatory rules for AI contributors

1. Read `README.md`, `STAR2LTE_PROGRESS.md`, `shim/star2lte/README.md`, the relevant source, recent commits, open PRs and workflow definitions before editing.
2. Work on a dedicated branch and open a focused PR. Do not push directly to `main`.
3. Never merge a PR without the owner's explicit approval.
4. Never say a test passed unless it was actually run. Separate static checks, CI builds, host tests, image validation and physical-device tests.
5. Do not invent register addresses, IRQs, clocks, memory ranges, boot arguments or hardware behavior. Cite source files or device evidence; label inferences and unknowns.
6. No automatic destructive operations. Do not format, erase, repartition, or flash BOOT, RECOVERY, SYSTEM, VENDOR, USERDATA, EFS, modem, PIT or other device storage. Any destructive operation requires explicit, specific approval.
7. The owner previously said they expect to format /data during testing. That is not blanket authorization for an agent or script to format it.
8. Keep stock firmware, DTBs, private device data, generated boot images and Apple firmware outside Git. Do not redistribute Apple's copyrighted firmware/userland.
9. Respect source licenses and preserve notices. Do not copy code from incompatible or unclear licenses.
10. HEROLTE (Galaxy S7 / Exynos 8890), A5XELTE and SWEET are reference work only. Their successful builds do not prove STAR2LTE support.
11. Keep changes small. PR description must list changed files, reason, checks run/not run, risks, and exact next device test.

## Confirmed physical-device evidence

The owner ran the STAR2LTE stage-0 handoff probe v2 on an SM-G965F using a repacked One UI stock BOOT image, installed through Odin AP. The phone booted the raw ARM64 Image and displayed the report.

Observed values recorded in `STAR2LTE_PROGRESS.md`:

- S-Boot enters at EL1.
- `SCTLR_EL1 = 0x30C5183E`: MMU off; I-cache and D-cache on.
- Probe image load address: `0x80150000`.
- X0 points to a valid FDT at `0x8A000000`; magic `D00DFEED`.
- FDT size: `0x4A29B` (303,771 bytes).
- `CNTFRQ_EL0 = 0`; project uses 26 MHz from device-tree evidence as a fallback.
- Boot CPU: Cortex-A55 r1p1, MPIDR affinity 0.
- 16 KiB translation granule is supported; current XNU base configuration remains 4 KiB.
- Probe reports the previous DECON trigger value `0x3070`.
- Probe countdown completes and the device requests reboot into recovery.

The user also supplied a separate TWRP diagnostic report:
- Model/product: SM-G965F / Galaxy S9+; device `star2lte`; hardware `samsungexynos9810`.
- DT model: `Samsung Star2 rev26 board based on EXYNOS9810`; compatible strings include `Samsung, Star2 rev26` and `Samsung,EXYNOS9810`.
- Board revision 26; bootloader `G965FXXUHFVG4`.
- TWRP 3.7.0_9-0; recovery kernel 4.9.118-g36e6ddff22b9.
- Recovery-visible MemTotal: 5,756,336 kB.
- CPUs 0–3: Cortex-A55; CPUs 4–7: Samsung Mongoose M3.
- By-name partition mappings are documented in the original diagnostic report and `STAR2LTE_PROGRESS.md`.
- TWRP did not mount /data. Its ext4/encryptable fstab entry does not establish corruption.

Treat `/proc/iomem` as a Linux resource map, not a canonical hardware register map. Do not blindly reuse its addresses in XNU.

## Current stage and immediate next test

### Already physically boot-tested
`shim/star2lte/out/probe.Image` is the stage-0 handoff probe. It is built by `.github/workflows/star2lte-probe.yml`, packed into a copy of the stock One UI BOOT image with `tools/star2lte-pack-boot.py`, wrapped in an Odin AP tar, and has already booted on the physical phone.

### Built but NOT physically tested
`shim/star2lte/out/memmap.Image` is a second bare-metal image that walks the FDT passed in X0 and displays DRAM banks and reserved-memory ranges. It has CI build and synthetic-DTB host-test evidence only. It has **not** yet been packed/flashed/run on the phone.

The next useful device test is this memory-map probe, not an XNU kernel. It is needed to select a safe RAM window for a future XNU loader. Follow `shim/star2lte/README.md`:
1. Build/download the current `star2lte-probe` artifact containing `memmap.Image`.
2. Use the exact matching One UI stock BOOT image as input to `tools/star2lte-pack-boot.py`.
3. Produce a new output image and an Odin tar whose member is named `boot.img`.
4. In Odin, use AP only; do not select Re-Partition and do not flash PIT or other partitions.
5. Keep the untouched stock AP/BOOT as the recovery path.
6. Photograph every memory-map page and provide it for analysis.
7. Record DRAM and reserved ranges in `STAR2LTE_PROGRESS.md`; only then choose an XNU RAM window.

Do not claim the memory-map probe has been device-tested until the owner reports the result.

## XNU readiness: current blocker

The repository README explicitly states:
- STAR2LTE XNU machine configuration: not started.
- XNU loader, UFS and DECON/DSIM drivers: not started.
- iOS userland: not running on this device.
- The existing `xnu.yml` workflow defaults to HEROLTE and its existing machine configs are HEROLTE/A5XELTE/SWEET. A green default XNU workflow is not an S9+ build.

Therefore do not package a generic XNU kernel and call it bootable for STAR2LTE. Before a meaningful XNU boot attempt, the project needs at least:
- a validated live DRAM/reserved-memory map;
- an explicit STAR2LTE XNU machine/platform configuration;
- a loader/trampoline that handles the observed EL1 entry, MMU-off state, caches-on state, FDT pointer in X0 and 26 MHz timer fallback;
- verified exception/vector setup, page tables and memory ownership;
- a working serial or framebuffer diagnostic path;
- a staged plan for GIC-400 interrupts, timer, SMP/PSCI and storage.
A kernel compile is not a device boot. A boot splash is not proof of working drivers or userspace.

## Useful repository paths

- `README.md`: project status and overview.
- `STAR2LTE_PROGRESS.md`: evidence, probe results, unresolved questions and next steps.
- `docs/STAR2LTE_REFERENCE_SOURCES.md`: external hardware/source references.
- `shim/star2lte/`: stage-0 and memory-map probes.
- `tools/star2lte-pack-boot.py`: host-side Samsung BOOT repacker; it never flashes.
- `tools/mkadt_star2lte.py`: experimental Apple Device Tree generator; requires validated DRAM arguments.
- `.github/workflows/star2lte-probe.yml`: builds both probe images and host-tests FDT parsing.
- `.github/workflows/star2lte-twrp-diagnostic.yml`: builds a read-only TWRP diagnostic ZIP.
- `.github/workflows/xnu.yml`: existing XNU workflow; inspect machine config selection carefully.

## TWRP diagnostic note

The read-only diagnostic ZIP generated a report on the device. The first run showed:
`/tmp/updater[26]: >&23 : illegal file descriptor name`.
The UI-print helper was changed to validate decimal OUTFD and write through `/proc/self/fd/$OUTFD`. Check the live PR/commit and test the new artifact before calling the fix verified. The diagnostic ZIP does not install XNU and does not flash partitions.

## Required test-report format

For every test, report:
- commit SHA and exact workflow/command;
- artifact filename and SHA-256;
- target model and firmware;
- test category: static, CI build, host test, artifact validation, or physical-device test;
- expected and actual results;
- logs/photos and any caveats;
- whether any device partition was written.

## First instruction to a new AI

Read this file, then inspect the live repository and current PR/Actions state. Give the owner a short evidence-based status report: what has physically booted, what is only CI/host-tested, and what blocks XNU. Do not duplicate an open PR. Prioritize getting the live memory-map probe tested on the phone, then produce a STAR2LTE-specific XNU platform gap analysis. Work in a branch and PR; never merge without approval.
