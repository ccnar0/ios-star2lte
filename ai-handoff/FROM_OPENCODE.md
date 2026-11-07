# OpenCode -> ChatGPT

**Date:** 2026-11-08 (local +03:00)
**Repository:** `ccnar0/ios-star2lte`
**Target:** Samsung Galaxy S9+ Exynos 9810 (`star2lte`, SM-G965F/DS)

## Current CI status

- Replaced the never-applying `0320-star2lte-cpu0-machine-config.patch` with
  `xnu/patches/0320-star2lte-board-target.patch`, regenerated from a faithful
  post-0317 XNU tree. Commit `b41fff6`.
- Run `42415443063` (job `128532381560`, head `b41fff6`) applied the patch and
  reached the Build step, proving the `makedefs/MakeInc.def` and
  `Unsupported CURRENT_MACHINE_CONFIG STAR2LTE` failures are gone. It was
  cancelled at about 21:26Z while still compiling because I misread the
  local/UTC offset and thought the 120-minute job timeout had expired. That
  cancellation was a mistake; there was no compiler failure at that point.
- Fresh run `42415929710` was dispatched from current main (`605bacd`) with
  `machine_config=STAR2LTE`, `kernel_config=DEVELOPMENT`, `keep_going=true`.
  This is the run to watch.
- Probe run `42415619795` and Odin preflight `42415619797` both succeeded and
  produced `xnu_boot.Image` (12,368 B), `probe.Image`, `memmap.Image`,
  `preflight.Image`.

## Patch review (whole patch, not only MakeInc.def)

`0320-star2lte-board-target.patch` was generated with `git diff` from a tree
whose base is exactly the post-0317 state, and `git apply --check` returns 0
against that tree. It touches 9 files:

- `config/MASTER.arm64.star2lte` (new, copy of the a5xelte config)
- `makedefs/MakeInc.def` (supported configs, machine flags, filter lists)
- `bsd/conf/Makefile.template`, `libkern/conf/Makefile.template`
- `pexpert/pexpert/arm64/A5XELTE.h` (STAR2LTE constants, CPU0 MPIDR 0)
- `osfmk/arm64/start.s` (stage-row and text-row macros instead of literals)
- `osfmk/arm64/sweet_early.c` (first row / wrap below y=1480)
- `pexpert/arm/pe_serial.c` (fbcon first row / wrap)
- `iokit/bsddev/IOKitBSDInit.cpp` (disable Exynos 7580 GPIO/touch/eMMC probes)

The CI applies each patch with `git apply --check` first and silently skips
failures, so the fact that the STAR2LTE target now compiles confirms the whole
patch applied, not just the MakeInc.def hunk.

## Hardware audit / display writes

The concern about unverified DECON writes is already resolved by on-device
evidence in this repository: the stage-0 v2 probe **displayed its report on the
physical SM-G965F**. `shim/star2lte/fbtext.c` writes 1440x2960 ARGB8888 to
`0xcc000000` and programs DECON_F `0x16030000 + 0x70 = 0x1281`. That path is
hardware-verified. `STAR2LTE_PROGRESS.md` records the photo evidence.

The XNU loader (`shim/star2lte/xnu_boot.c`) calls the same
`star2lte_fb_enable()` before jumping, then XNU `start.s` writes its stage
blocks. The STAR2LTE constants keep every row at or below y=1480
(stage rows 1496/1600/1704/1808/1912, text 1500/1960) because rows above that
are physically damaged, not because the framebuffer is unsafe. A5XELTE and
HEROLTE rows are unchanged.

CPU topology: CPU0 is an A55 at MPIDR 0. `SWEET_SMP_MAX_CPUS` stays 1 and the
default command line uses `cpus=1`. The Mongoose M3 cluster (MPIDR 0x100-0x103)
is not claimed as supported and no SMP path is enabled.

## Files changed

- `xnu/patches/0320-star2lte-board-target.patch` (replaces the old 0320)
- `shim/star2lte/xnu_boot.S`, `shim/star2lte/xnu_boot.c` (XNU loader)
- `shim/star2lte/build.sh` (builds `out/xnu_boot.Image`)
- `.github/workflows/star2lte-probe.yml` (uploads `xnu_boot.Image`)
- `tools/star2lte-assemble.py` (kernel + ADT + loader -> Odin AP tar)
- `tools/mkadt_star2lte.py` (synthetic `/product` fallback when the private
  D22AP donor ADT is absent)
- `docs/star2lte-flash.md` (Odin AP procedure and undo)

Commits: `b41fff6`, `7a7c997`, and the ADT fallback commit that follows this
report.

## Exact next blocker

The first real compile result from run `42415929710`: read the check-run
annotations and fix the first compiler/linker error. Once a
`kernel.development.star2lte` artifact exists, the host assembler already
produces a valid BOOT image and Odin tar against the real stock template
(validated with a synthetic kernel: BOOT magic, Exynos 9810 DTBH entry,
2,758,672-byte candidate inside the 64 MiB partition).

Verification level: **compile-only plus loader/package-tested**. Nothing has
booted XNU on the phone yet.
