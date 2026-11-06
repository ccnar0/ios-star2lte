/* SPDX-License-Identifier: GPL-2.0-only
 * star2lte (SM-G965F/DS, Exynos 9810) stage-0 probe v2: print the raw
 * S-Boot handoff on the firmware framebuffer, then return so probe.S can
 * request recovery. No storage, EFS, PMIC-regulator, MMU or bootloader
 * writes.
 */
#include <stdint.h>
#include "fbtext.h"

#define RECOVERY_DELAY_S 180U
#define FDT_MAGIC 0xd00dfeedU
#define TAGS_ADDR 0x10000100UL

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

/* Dereferencing an unmapped hole with the MMU off aborts, so only touch
 * ranges the star2lte DTB lists as RAM. While the probe runs from the low
 * alias (image < 0x20000000) the 32 MiB the stock kernel occupied there is
 * backed too; when it runs from DRAM the same test never fires. */
static int safe_addr(uintptr_t a, uintptr_t image)
{
    if (a & 7U)
        return 0;
    if (image < 0x20000000UL && a >= 0x10000000UL && a < 0x12000000UL)
        return 1;
    if (a >= 0x80000000UL && a < 0xbc800000UL)
        return 1;
    if (a >= 0xc0000000UL && a < 0xe0000000UL)
        return 1;
    if (a >= 0xe1900000UL && a < 0x100000000UL)
        return 1;
    return 0;
}

static void word_row(unsigned row, const char *label, uintptr_t a, uintptr_t image)
{
    if (safe_addr(a, image)) {
        star2lte_fb_hex(row, label, be32(a));
        return;
    }
    char line[24];
    unsigned n = 0;
    while (label[n] && n < 12) {
        line[n] = label[n];
        ++n;
    }
    line[n++] = ' ';
    line[n++] = '-';
    line[n++] = '-';
    line[n++] = '-';
    line[n++] = '-';
    line[n] = 0;
    star2lte_fb_text(row, line);
}

/* Fallback: if no register points at the DTB, look for its magic in the
 * area just past the kernel, still inside a range proven to be backed. */
static uintptr_t find_fdt(uintptr_t image)
{
    uintptr_t lo, hi;
    if (image < 0x20000000UL) {
        lo = 0x10000000UL;
        hi = 0x11e00000UL;
    } else {
        lo = image & ~0xfffffUL;
        hi = lo + 0x800000UL;
        if (hi > 0xbc800000UL)
            hi = 0xbc800000UL;
    }
    for (uintptr_t a = lo; a + 4 <= hi; a += 8)
        if (be32(a) == FDT_MAGIC)
            return a;
    return 0;
}

void star2lte_probe_main(uintptr_t x0, uintptr_t x1, uintptr_t x2, uintptr_t x3,
                         uint64_t el, uint64_t sctlr, uint64_t image)
{
    uint64_t midr, mpidr, mmfr0, freq;
    uint32_t trig_before = star2lte_fb_enable();

    __asm__ volatile("mrs %0, midr_el1" : "=r" (midr));
    __asm__ volatile("mrs %0, mpidr_el1" : "=r" (mpidr));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r" (mmfr0));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r" (freq));

    uintptr_t fdt;
    if (safe_addr(x0, image) && be32(x0) == FDT_MAGIC)
        fdt = x0;
    else
        fdt = find_fdt(image);

    unsigned row = 0;
    star2lte_fb_text(row++, "STAR2LTE PROBE 2");
    star2lte_fb_hex(row++, "EL", el >> 2);
    star2lte_fb_hex(row++, "SCTLR", sctlr);
    star2lte_fb_hex(row++, "IMAGE", image);
    star2lte_fb_hex(row++, "X0", x0);
    star2lte_fb_hex(row++, "X1", x1);
    star2lte_fb_hex(row++, "X2", x2);
    star2lte_fb_hex(row++, "X3", x3);
    word_row(row++, "TAGS", TAGS_ADDR, image);
    word_row(row++, "W0", x0, image);
    word_row(row++, "W1", x1, image);
    word_row(row++, "W2", x2, image);
    word_row(row++, "W3", x3, image);
    if (fdt) {
        star2lte_fb_hex(row++, "FDTAT", fdt);
        star2lte_fb_hex(row++, "FDTSZ", be32(fdt + 4));
    } else {
        star2lte_fb_text(row++, "FDTAT NONE");
        star2lte_fb_text(row++, "FDTSZ ----");
    }
    star2lte_fb_hex(row++, "CNTFRQ", freq);
    star2lte_fb_hex(row++, "MIDR", midr);
    star2lte_fb_hex(row++, "MPIDR", mpidr);
    /* MMFR0[23:20] TGran16: 0 means no 16 KiB pages (needs the 4 KiB XNU). */
    star2lte_fb_text(row++, ((mmfr0 >> 20) & 15U) ? "16K PAGES YES" : "16K PAGES NO");
    star2lte_fb_hex(row++, "TRIG", trig_before);

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
            char line[] = "HOLD 000 S";
            line[5] = (char)('0' + left / 100);
            line[6] = (char)('0' + (left / 10) % 10);
            line[7] = (char)('0' + left % 10);
            star2lte_fb_text(row + 1, line);
            shown = left;
        }
    }
}
