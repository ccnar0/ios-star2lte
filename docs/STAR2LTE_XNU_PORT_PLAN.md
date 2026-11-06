# STAR2LTE XNU port plan

Updated: 2026-11-06

## Decision

A STAR2LTE XNU bring-up is technically plausible and can start now, but the
repository does **not** yet contain a STAR2LTE XNU machine configuration or a
validated XNU loader. The live S-Boot handoff and memory-map probes are useful
prerequisites, not evidence that an XNU kernel is bootable.

The immediate goal is a CPU0-only, non-destructive first-kernel milestone:
build a STAR2LTE-specific XNU target, enter a minimal loader from the known
S-Boot state, report each stage on the framebuffer, and stop safely before
using any unverified RAM window or peripheral.

## Evidence from this device

Physical SM-G965F evidence recorded in `STAR2LTE_PROGRESS.md`:

- S-Boot enters at EL1, with MMU disabled and I/D caches enabled.
- The bare-metal Image was loaded at `0x80150000`.
- X0 contains a valid FDT at `0x8A000000` (303,771 bytes).
- `CNTFRQ_EL0` reads zero; the board's timer source says 26 MHz.
- The memory-map probe displayed 20 records: five DRAM banks, 11 fixed
  reservations and four size-only dynamic pools.
- DRAM banks observed:
  - `0x80000000 + 0x3C800000`
  - `0xC0000000 + 0x20000000`
  - `0xE1900000 + 0x1E700000`
  - `0x880000000 + 0x80000000`
  - `0x900000000 + 0x80000000`
- Combined DRAM size reported by the probe: 6,063 MiB (~5.92 GiB).

The photographs do not provide legible initrd start/end values. The full
reserved/dynamic map must be reconciled with the framebuffer, FDT, initrd,
firmware allocations and loader allocations before assigning RAM to XNU.
Do not infer a free range merely because it appears inside a DRAM bank.

## Important upstream/reference lead

The Scion/HakiOS project now has a public Exynos 9810 XNU port for the sibling
Galaxy S9 (`starlte`), with device files in
[Scion-Project/scion-starlte](https://github.com/Scion-Project/scion-starlte)
and the kernel/loader in
[Scion-Project/scion-kernel](https://github.com/Scion-Project/scion-kernel).

The S9 reference uses a different boot architecture:

`S-Boot -> BootShim -> Silicium UEFI -> Carbon EL2/PSCI -> xnuloader -> XNU`

Its loader chooses a free UEFI memory-map window, creates Apple boot arguments
and an Apple device tree, then exits boot services and enters XNU. Carbon
provides PSCI handling that S-Boot does not provide correctly for the M3
cluster. This is a strong research reference, but it is not a drop-in patch for
this repository's existing direct bare-metal loader.

Important limitation: the public `xnuloader/platforms/star2lte.c` entry in
Scion's tree explicitly says it is **written, not seen** on an S9+ or Note9.
It describes the shared board/SoC layout and sets `extra_ram_off=1`; only the
S9 `starlte` port is described as device-tested. Therefore its star2lte
configuration must be treated as a hypothesis until tested on this handset.
The reference project's own device page says S9+ and Note9 are untested.

Relevant source files to compare (read-only references; no code copied in
this plan):

- `scion-kernel/xnuloader/platforms/star2lte.c`
- `scion-kernel/xnuloader/platforms/starlte.c`
- `scion-kernel/xnuloader/loader.c`
- `scion-starlte/uefi/files/Platforms/Samsung/starltePkg/Library/MemoryMapLib/MemoryMapLib.c`
- `scion-starlte/configs/starlte.config`

The Scion repository declares its loader/device layer MIT-licensed and its
XNU modifications APSL-2.0. Preserve those notices and review compatibility
before adapting any implementation. This plan contains no copied source.

## Architecture choice

There are two viable paths, and they must not be mixed accidentally:

1. **Direct S-Boot -> bare-metal loader -> XNU:** closest to this repository's
   current probe and packaging. It must independently handle the observed EL1
   entry state, FDT, memory ownership, cache maintenance, boot_args/ADT,
   exception vectors and CPU bring-up. Do not reuse the HEROLTE loader's
   hard-coded RAM window or log address.
2. **S-Boot -> BootShim/UEFI -> EL2 PSCI service -> XNU loader -> XNU:** closer
   to the tested S9 reference. This requires a STAR2LTE-capable UEFI build,
   a verified Carbon/PSCI path, and careful reservations for UEFI, payload,
   CP, ABOX and framebuffer. It is a larger boot-chain project.

Start by keeping the current direct-loader path isolated and diagnostic. Do
not add EL2, PSCI cluster writes or a new UEFI image to the phone until each
stage has a separate build and read-only validation plan. Re-evaluate the
UEFI/Carbon path after the first loader milestone and compare effort against
the direct path.

## Implementation milestones

### 1. Finish memory ownership accounting

- Improve the memory-map display/test so initrd bounds are legible.
- Record every fixed reservation and dynamic pool with full 64-bit addresses.
- Reconcile the 11 fixed and four dynamic records with the exact-board DTS and
  the live S-Boot FDT; do not treat dynamic size-only pools as fixed addresses.
- Determine where the framebuffer and FDT reside and whether firmware continues
  using them after handoff.
- Produce a machine-readable memory map and a checker that rejects overlaps.
- Only then select a loader log page and XNU physical window.

### 2. Add an isolated STAR2LTE XNU build target

- Add a STAR2LTE machine/platform configuration patch in the XNU patch series.
- Keep the initial build single-core and conservative: 4 KiB pages, no SMP,
  no storage driver, no display driver, and no M3-specific code.
- Add an explicit `STAR2LTE` choice to the XNU workflow; do not change the
  existing HEROLTE default or claim its build covers STAR2LTE.
- Confirm the exact XNU entry ABI and required `boot_args`/ADT properties
  against this repository's XNU revision before jumping.

### 3. Build a diagnostic loader milestone

- Validate the incoming FDT pointer, EL1 state, image bounds and memory map.
- Establish a private stack only in memory proven free by the live map.
- Print a loader banner and numbered checkpoints using the already-tested
  framebuffer path.
- Keep interrupts masked and CPU0 only; do not touch UFS, PMU, watchdog,
  cluster power registers or other unverified MMIO.
- On a failed check, show an error and use only the already-tested recovery
  reset path if the reset sequence has been independently verified.

### 4. First XNU handoff

- Construct the minimum valid boot_args and Apple device tree.
- Set up the required page tables and cache maintenance.
- Enter the XNU entry only after the physical window and all reserved ranges
  are proven non-overlapping.
- First success criterion: stable XNU early-boot output on CPU0. A compiled
  kernel or a loader banner alone is not a kernel boot.

### 5. Drivers and SMP

Only after CPU0 early boot: GIC-400/timer, PSCI/secondary cores, UFS 2.1,
DECON_F/DSIM, touch, PMIC and other board devices. Treat M3 startup as a
separate high-risk task; the S9 reference reports extra PMU preparation and
EL2 PSCI support, but those writes must not be copied without board-specific
verification.

## Safety and acceptance rules

- Never flash an experimental XNU image as a replacement for the only recovery
  path. Keep a known-good stock AP/BOOT package available.
- No PIT, repartition, EFS, modem, USERDATA or /data operations in kernel tests.
- Separate static analysis, host tests, CI builds, image validation and
  physical-device results in every report.
- A successful STAR2LTE cross-compile is not evidence of a successful boot.
- Do not mark STAR2LTE or iOS as working until the actual SM-G965F has reached
  and retained the claimed milestone.
