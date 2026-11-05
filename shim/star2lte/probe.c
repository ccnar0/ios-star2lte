/* SPDX-License-Identifier: GPL-2.0-only
 * star2lte (SM-G965F/DS, Exynos 9810) stage-0 probe: print the S-Boot
 * handoff on the firmware framebuffer, then return so probe.S can request
 * recovery. No storage, EFS, PMIC-regulator, MMU or bootloader writes.
 */
#include <stdint.h>
#include "../fdt.h"
#include "fbtext.h"

#define RECOVERY_DELAY_S 90U
#define FDT_MAGIC 0xd00dfeedU

static uint64_t counter(void)
{
    uint64_t value;
    __asm__ volatile("isb; mrs %0, cntpct_el0" : "=r" (value));
    return value;
}

static uint32_t be32(uintptr_t a)
{
    return __builtin_bswap32(*(volatile uint32_t *)a);
}

/* Only dereference X0 if it looks like the S-Boot FDT pointer, so a bad
 * handoff cannot make the probe hang on an unbacked physical address. */
static int fdt_looks_valid(uintptr_t fdt)
{
    return fdt >= 0x80000000UL && fdt < 0x200000000UL && !(fdt & 7U);
}

void star2lte_probe_main(uintptr_t fdt, uint64_t el, uint64_t sctlr, uint64_t image)
{
    uint64_t midr, mpidr, mmfr0, pfr0, isar0, freq, start = 0, end = 0;
    uint32_t trig_before = star2lte_fb_enable();
    int fdt_ok = fdt_looks_valid(fdt) && be32(fdt) == FDT_MAGIC;

    __asm__ volatile("mrs %0, midr_el1" : "=r" (midr));
    __asm__ volatile("mrs %0, mpidr_el1" : "=r" (mpidr));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r" (mmfr0));
    __asm__ volatile("mrs %0, id_aa64pfr0_el1" : "=r" (pfr0));
    __asm__ volatile("mrs %0, id_aa64isar0_el1" : "=r" (isar0));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r" (freq));

    unsigned row = 0;
    star2lte_fb_text(row++, "STAR2LTE PROBE 1");
    star2lte_fb_hex(row++, "EL", el >> 2);
    star2lte_fb_hex(row++, "SCTLR", sctlr);
    star2lte_fb_hex(row++, "FDT", fdt);
    star2lte_fb_hex(row++, "IMAGE", image);
    star2lte_fb_hex(row++, "MIDR", midr);
    star2lte_fb_hex(row++, "MPIDR", mpidr);
    star2lte_fb_hex(row++, "MMFR0", mmfr0);
    star2lte_fb_hex(row++, "PFR0", pfr0);
    star2lte_fb_hex(row++, "ISAR0", isar0);
    star2lte_fb_hex(row++, "CNTFRQ", freq);
    star2lte_fb_hex(row++, "TRIG", trig_before);
    if (fdt_ok) {
        star2lte_fb_hex(row++, "FDT SIZE", be32(fdt + 4));
        if (fdt_chosen_u64(fdt, "linux,initrd-start", &start) &&
            fdt_chosen_u64(fdt, "linux,initrd-end", &end)) {
            star2lte_fb_hex(row++, "INITRD", start);
            star2lte_fb_hex(row++, "INITRD END", end);
        } else {
            star2lte_fb_text(row++, "INITRD ?");
        }
    } else {
        star2lte_fb_text(row++, "FDT BAD");
        star2lte_fb_text(row++, "INITRD ?");
    }
    /* MMFR0[23:20] TGran16: 0 means no 16 KiB pages (needs the 4 KiB XNU). */
    star2lte_fb_text(row++, ((mmfr0 >> 20) & 15U) ? "16K PAGES: YES" : "16K PAGES: NO");

    if (!freq)
        freq = 26000000U;   /* exynos9810 oscclk, mainline starlte DTS */
    uint64_t t0 = counter();
    unsigned shown = ~0U;
    for (;;) {
        uint64_t elapsed = (counter() - t0) / freq;
        if (elapsed >= RECOVERY_DELAY_S)
            return;
        unsigned left = RECOVERY_DELAY_S - (unsigned)elapsed;
        if (left != shown) {
            char line[] = "TWRP EN 000 S";
            line[8] = (char)('0' + left / 100);
            line[9] = (char)('0' + (left / 10) % 10);
            line[10] = (char)('0' + left % 10);
            star2lte_fb_text(row + 1, line);
            shown = left;
        }
    }
}
