# STAR2LTE external hardware references

These are source references for the Exynos 9810 and the SM-G965F/DS board.
They are research inputs, not substitutes for the physical phone's S-Boot
handoff DTB. Never select an XNU RAM window from these static DTS files.

## Exact board source: star2lte

The LineageOS 24.0 Exynos 9810 kernel tree contains:

- [exynos9810-star2lte_eur_open_26.dts](https://github.com/ExyHyperBrick/android_kernel_samsung_exynos9810/blob/lineage-24.0/arch/arm64/boot/dts/exynos/exynos9810-star2lte_eur_open_26.dts)
- [star2lte device tree repository](https://github.com/ExyHyperBrick/android_device_samsung_star2lte/tree/lineage-24.0)
- [star2lte common kernel repository](https://github.com/ExyHyperBrick/android_kernel_samsung_exynos9810/tree/lineage-24.0)

The board DTS identifies "Samsung Star2 rev26", uses root
`#address-cells = <2>` and `#size-cells = <1>`, and defines
`reserved-memory` with the same cell widths. This is why the parser now
has a dedicated one-size-cell test fixture in
`test_star2lte_reserved.dts`.

Fixed reservations in that source include:

| Node | Base | Size | Note |
| --- | ---: | ---: | --- |
| `modem_if` | `0xF0000000` | `0x09200000` | modem shared memory |
| `cp_ram_logging` | `0xF9200000` | `0x02000000` | CP logging |
| `abox_rmem` | `0xFB200000` | `0x02800000` | audio shared memory |
| `seclog_mem` | `0xC0000000` | `0x00080000` | secure log |
| `secure_camera` | `0xE1900000` | `0x01E00000` | camera memory |
| `exynos_ss` | `0xFDA00000` | `0x01C08000` | Samsung crash/log region |
| `fimc_is_lib` | `0xFF800000` | `0x00800000` | camera firmware region |

The source also declares size-only pools such as `video_stream`,
`crypto`, `camera`, and `camera_contig`. Their base is allocated
dynamically; do not invent one from `alloc-ranges`.

Two entries, `tima_region` and `rkp_region`, are explicitly marked
`status = "disabled"` in this board DTS. A parser must not report them as
active fixed reservations. The host test covers disabled-node filtering.
Nested children of `exynos_ss` are descriptive subranges, not additional
top-level reservations; the parent region is the reservation.

**Critical limitation:** this vendor board DTS does not provide a root
`memory` node. The DRAM banks are supplied by boot firmware / another
DTB layer. The static reserved ranges above are cross-checks only, not a
live RAM map and not proof that every region is active in the S-Boot DTB.

## Exynos 9810 SoC and the S9 sibling

Upstream Linux sources:

- [exynos9810.dtsi](https://github.com/torvalds/linux/blob/master/arch/arm64/boot/dts/exynos/exynos9810.dtsi)
- [exynos9810-starlte.dts](https://github.com/torvalds/linux/blob/master/arch/arm64/boot/dts/exynos/exynos9810-starlte.dts)
- [Linux mailing-list bring-up series](https://lists.openwall.net/linux-kernel/2024/10/23/1813)

The SoC DTS confirms the A55 MPIDRs 0-3, Mongoose M3 MPIDRs
0x100-0x103, GIC-400, PSCI SMC interface, and the 26 MHz architectural
timer. It explicitly documents that the stock Samsung bootloader does
not configure `CNTFRQ_EL0`, matching the physical probe result.

The upstream board DTS is for Galaxy S9 `starlte` (SM-G960F), not the
S9+ `star2lte`. Treat its panel, GPIO, and board-level settings as sibling
references only.

## uniLoader

- [uniLoader repository](https://github.com/ivoszbg/uniLoader)
- [starlte board file](https://github.com/ivoszbg/uniLoader/blob/master/board/samsung/board-starlte.c)
- [Exynos 9810 SoC initialization](https://github.com/ivoszbg/uniLoader/blob/master/soc/exynos/exynos9810.c)

The starlte board code writes `0x1281` to DECON_F + `0x70`
(`0x16030070`) to enable framebuffer refresh and describes a 1440x2960
ARGB8888 framebuffer at `0xCC000000`. This independently corroborates
the existing DECON/framebuffer research. It is a starlte board hook, not
a drop-in star2lte bootloader or XNU driver.

The Linux bring-up series explains why a shim loader is useful: Samsung
S-Boot leaves the DECON trigger control unset, preventing the simple
framebuffer from refreshing. This is useful architectural context, but
does not authorize changing the current probe's reset/boot sequence.

## Apple device tree / XNU notes

Apple ADT is not the same binary format as the firmware FDT. A Linux DTS
can provide hardware facts, but it cannot be copied as an Apple ADT.
The [UniXNU device-tree guide](https://unixnu.com/porting/device-tree)
describes a DTS/FDT-to-ADT conversion workflow and stresses matching
properties and node names to the actual XNU drivers. Our experimental
`mkadt_star2lte.py` still needs a dedicated STAR2LTE XNU machine
configuration and driver match validation.


## Existing XNU port on the Exynos 9810 sibling

A public HakiOS device repository documents iOS 16.5 running on XNU on the Galaxy S9 `starlte` (SM-G960F), which shares the Exynos 9810 but is not the S9+ board. Its boot chain is S-Boot -> BootShim -> Project Silicium UEFI -> Carbon Hypervisor at EL2 (PSCI) -> `xnuloader` -> XNU. The device README documents the S-Boot parked-core MPIDR quirk and the Exynos cluster-1 PMU preparation needed before Mongoose M3 CPU_ON. It is a valuable architecture and bring-up reference, not a drop-in S9+ port. The README also says the S9+ and Note9 had not yet been tested with its extra-RAM path.

- [HakiOS S9 device repository](https://github.com/lky005/hakios-starlte) (fork of the Scion-Project device repository; BSD/MIT device components and prebuilt UEFI have their own notices)
- [HakiOS porting record](https://github.com/lky005/hakios-starlte/blob/main/porting.toml)
- [HakiOS Exynos 9810 ecosystem survey](https://github.com/lky005/hakios-starlte/blob/main/docs/9810-ecosystem.md)
- [HakiOS S9 memory-map implementation](https://github.com/lky005/hakios-starlte/blob/main/uefi/files/Platforms/Samsung/starltePkg/Library/MemoryMapLib/MemoryMapLib.c)

The common HakiOS kernel repository is referenced by the device README as a separate project. Its XNU code is APSL 2.0; do not copy code unless the exact source and licence are checked. The public device repository is enough to establish the boot architecture and point to the relevant kernel features, but not to transplant the full kernel implementation here.

## Exact S9+ mainline Linux bring-up

SheroAbi's public Linux 7.1 project is for the exact Galaxy S9+ `star2lte` / SM-G965F, not merely the S9 sibling:

- [S9+ mainline Linux repository](https://github.com/SheroAbi/galaxy-s9plus-mainline-linux)
- [Exact S9+ DTS](https://github.com/SheroAbi/galaxy-s9plus-mainline-linux/blob/main/src/mainline-dts/exynos9810-star2lte.dts)
- [Hardware map](https://github.com/SheroAbi/galaxy-s9plus-mainline-linux/blob/main/docs/01-hardware.md)
- [DECON driver](https://github.com/SheroAbi/galaxy-s9plus-mainline-linux/blob/main/src/gpu/drm/s9p/s9p-decon.c)
- [Exynos 9810 UFS driver](https://github.com/SheroAbi/galaxy-s9plus-mainline-linux/blob/main/src/ufs/ufs-exynos9810.c)
- [CPU frequency driver](https://github.com/SheroAbi/galaxy-s9plus-mainline-linux/blob/main/src/cpufreq/s9p-cpufreq.c)
- [Performance notes](https://github.com/SheroAbi/galaxy-s9plus-mainline-linux/blob/main/docs/05-performance.md)

Its DTS reports 6 GiB RAM as low banks `0x80000000+0x3c800000`, `0xc0000000+0x20000000`, `0xe1900000+0x1e700000` and high banks `0x880000000+0x80000000`, `0x900000000+0x80000000`. The source comments say these are addresses the device bootloader reports through `update_memory_node()`. It also carries fixed S-Boot reservations and notes that display scanout must remain below 4 GiB because DECON uses 32-bit addresses. These are Linux/DT cross-checks; XNU needs its own memory-window and DMA design.

The Linux source is GPL-2.0; use it for hardware facts and implementation study, not as code to paste into an APSL XNU tree. The DTS itself is dual-licensed GPL-2.0 OR BSD-3-Clause, as its SPDX header states.


## Exact S9+ Windows-on-ARM reference (permissive source)

[ntdevlabs/exynos9810-woa](https://github.com/ntdevlabs/exynos9810-woa) targets the exact SM-G965F / star2lte and contains a complete `Star2LtePkg` platform description:

- [ACPI MADT](https://github.com/ntdevlabs/exynos9810-woa/blob/main/firmware/Platform/Star2LtePkg/AcpiTables/Madt.aslc): GICv2 GICD/GICC/GICH/GICV windows and all eight MPIDRs.
- [ACPI PPTT](https://github.com/ntdevlabs/exynos9810-woa/blob/main/firmware/Platform/Star2LtePkg/AcpiTables/Pptt.aslc): package, A55 cluster and Mongoose-M3 cluster topology.
- [ACPI GTDT](https://github.com/ntdevlabs/exynos9810-woa/blob/main/firmware/Platform/Star2LtePkg/AcpiTables/Gtdt.aslc): timer PPIs and a documented workaround for `CNTFRQ_EL0 == 0` using a synthetic counter frame and the Samsung MCT.
- [UEFI memory map](https://github.com/ntdevlabs/exynos9810-woa/blob/main/firmware/Platform/Star2LtePkg/Library/PlatformMemoryMapLib/PlatformMemoryMapLib.c): DRAM, framebuffer, reservations and MMIO map.
- [UFS driver](https://github.com/ntdevlabs/exynos9810-woa/tree/main/drivers/Exynos9810Ufs): exact-board UFSHCI implementation.
- [HSI2C driver](https://github.com/ntdevlabs/exynos9810-woa/tree/main/drivers/Exynos9810Hsi2c): exact-board USI03/HSI2C controller implementation.

The repository root is BSD-2-Clause-Patent. The HSI2C folder explicitly marks its hardware engine as BSD-2-Clause-Patent but some Microsoft WDF/SpbCx sample-derived scaffolding as MS-PL; retain those notices if adapting any part. These are Windows driver sources, not XNU drivers, so the Storport/KMDF APIs cannot be pasted into XNU. They are nevertheless the most promising permissively licensed source for register sequencing and controller behavior. Compare the UFS calibration against the S9+ vendor DTS and the current Linux implementation before writing an XNU driver.

## Porting implications

The S9 HakiOS port shows that the Exynos 9810 can run XNU/iOS with a dedicated UEFI + EL2/PSCI + loader stack. The exact S9+ Linux and Windows-on-ARM projects provide board-specific DTS/ACPI, RAM banks, UFS and peripheral resources. The practical path is therefore not to transplant a whole Android ROM: reuse the exact S9+ hardware descriptions, use permissively licensed UEFI/Windows code where appropriate, use GPL Linux code only as a factual reference unless a compatible licensing decision is made, and write/validate the XNU-specific machine and drivers separately.
