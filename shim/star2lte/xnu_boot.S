/* SPDX-License-Identifier: GPL-2.0-only
 * star2lte (SM-G965F/DS, Exynos 9810) S-Boot Image wrapper that hands off
 * to 4 KiB XNU. Adapted from shim/herolte/xnu_boot.S. No storage accesses.
 * Errors are shown on screen, then the Exynos9810 PMU resets the phone.
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
    mov x19, x0              /* FDT from S-Boot (0x8a000000) */
    mrs x20, CurrentEL
    mrs x21, sctlr_el1
    /* WTCON of both Exynos9810 watchdogs, from the star2lte DTB. */
    ldr x14, =0x10050000
    str wzr, [x14]
    ldr x14, =0x10060000
    str wzr, [x14]
    dsb sy
    /* Log page 0x8ffff000, below the XNU RAM window; XNU marks +0x100. */
    ldr x13, =0x8ffff000
    mov x14, x13
    add x15, x13, #4096
inval_log:
    dc ivac, x14
    add x14, x14, #64
    cmp x14, x15
    b.lo inval_log
    dsb sy
    mov x14, x13
zero_log:
    stp xzr, xzr, [x14], #16
    cmp x14, x15
    b.lo zero_log
    ldr x14, =0x313030554e583253 /* "S2XNU001" */
    str x14, [x13]
    str x20, [x13, #8]
    str x21, [x13, #16]
    str x19, [x13, #24]
    adr x14, _start
    str x14, [x13, #32]
    adr x14, star2lte_boot_stack_top
    mov sp, x14
    mov x0, x19
    mov x1, x13
    mov x2, x20
    mov x3, x21
    bl star2lte_prepare_xnu
    ldr x13, =0x8ffff000
    cbz x0, fail
    mov x16, x0
    ldr x0, [x13, #136]       /* boot_args physical address (log[17]) */
    /* C cleaned the copied ranges and the log. XNU's entry needs MMU off. */
    mrs x14, sctlr_el1
    bic x14, x14, #1
    bic x14, x14, #4
    bic x14, x14, #0x1000
    msr sctlr_el1, x14
    isb
    /* Distinct loader marker at 0x8ffff100, the same address where XNU
     * writes its own marker once it starts (so a later read can tell the
     * two apart). XNU overwrites this word. */
    ldr x14, =0x5354584e
    str x14, [x13, #256]
    dsb sy
    isb
    br x16
fail:
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
star2lte_boot_stack:
    .space 8192
star2lte_boot_stack_top:
