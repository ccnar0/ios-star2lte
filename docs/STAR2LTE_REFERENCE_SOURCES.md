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
