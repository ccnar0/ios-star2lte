/* SPDX-License-Identifier: GPL-2.0-only
 * Samsung star2lte (SM-G965F/DS, Exynos 9810) stage-0 handoff probe.
 *
 * Diagnostic only: no storage, EFS, PMIC-regulator, MMU or bootloader
 * changes. It records the S-Boot handoff on the firmware framebuffer,
 * then asks the Exynos 9810 PMU to reboot into recovery (TWRP).
 *
 * The arm64 Image header fields text_offset/image_size/flags are rewritten
 * by tools/star2lte-pack-boot.py from the stock kernel's header.
 */
.section .text.head, "ax"
.global _start
_start:
    b entry
    .long 0
    .quad 0x80000            /* text_offset: replaced by the packer */
    .quad 0                  /* image_size: replaced by the packer */
    .quad 0                  /* flags: replaced by the packer */
    .quad 0, 0, 0
    .byte 0x41, 0x52, 0x4d, 0x64
    .long 0

entry:
    msr daifset, #15
    mov x19, x0              /* FDT from S-Boot */
    mrs x20, CurrentEL
    mrs x21, sctlr_el1
    adr x22, _start
    adr x14, star2lte_probe_stack_top
    mov sp, x14
    mov x0, x19
    mov x1, x20
    mov x2, x21
    mov x3, x22
    bl star2lte_probe_main   /* returns only to request recovery */
    /* Exynos9810 PMU 0x14060000: SYSIP_DAT0 (0x810) = INFORM_RECOVERY 0xf,
     * then SWRESET (0x400) = 1. Offsets from the LineageOS
     * drivers/soc/samsung/exynos-reboot.c; pmu_base is in /reboot of the
     * stock star2lte DTB. */
    ldr x0, =0x14060000
    mov w14, #0xf
    str w14, [x0, #0x810]
    dsb sy
    mov w14, #1
    str w14, [x0, #0x400]
    dsb sy
halt:
    wfe
    b halt
    .ltorg

.section .data, "aw"
.balign 16
star2lte_probe_stack:
    .space 8192
star2lte_probe_stack_top:
