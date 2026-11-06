/* SPDX-License-Identifier: GPL-2.0-only
 * STAR2LTE XNU preflight: CPU0/S-Boot/FDT/physical-range checks only.
 *
 * This is an executable bring-up test, NOT XNU. It deliberately leaves the
 * MMU disabled and never touches storage, PMU, watchdog, UFS or other cores.
 * A successful screen means the known handoff prerequisites passed.
 */
#include <stdint.h>
#include "fbtext.h"
#include "fdtmem.h"

#define HOLD_S 180U
#define FDT_MAGIC 0xd00dfeedU
#define FB_BASE 0xcc000000ULL
#define FB_SIZE (1440ULL * 2960ULL * 4ULL)

extern char _start[], _image_end[];

static struct fdtmem_rec recs[160];
static struct fdtmem_stat map;

static uint64_t read_counter(void)
{
    uint64_t v;
    __asm__ volatile("isb; mrs %0, cntpct_el0" : "=r"(v));
    return v;
}
static uint64_t read_reg(const char *which)
{
    (void)which;
    return 0;
}
static uint32_t be32(uintptr_t p)
{
    return __builtin_bswap32(*(volatile const uint32_t *)p);
}
static int range_end(uint64_t base, uint64_t size, uint64_t *end)
{
    if (!size || base > UINT64_MAX - size) return 0;
    *end = base + size;
    return 1;
}
static int inside_ram(uint64_t base, uint64_t size)
{
    uint64_t end;
    if (!range_end(base, size, &end)) return 0;
    for (unsigned i = 0; i < map.count; ++i) {
        uint64_t re;
        if (recs[i].kind == FDTMEM_RAM &&
            range_end(recs[i].base, recs[i].size, &re) &&
            base >= recs[i].base && end <= re)
            return 1;
    }
    return 0;
}
static int overlaps(uint64_t a, uint64_t as, uint64_t b, uint64_t bs)
{
    uint64_t ae, be;
    if (!range_end(a, as, &ae) || !range_end(b, bs, &be)) return 1;
    return a < be && b < ae;
}
static void result(unsigned row, const char *name, int ok)
{
    char line[32];
    unsigned n = 0;
    while (name[n] && n < 19) { line[n] = name[n]; ++n; }
    line[n++] = ' ';
    const char *s = ok ? "PASS" : "FAIL";
    for (unsigned i = 0; i < 4; ++i) line[n++] = s[i];
    line[n] = 0;
    star2lte_fb_text(row, line);
}

void star2lte_probe_main(uintptr_t x0, uintptr_t x1, uintptr_t x2, uintptr_t x3,
                         uint64_t el, uint64_t sctlr, uint64_t image)
{
    (void)x1; (void)x2; (void)x3;
    uint64_t mpidr, mmfr0, freq;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    star2lte_fb_enable();
    star2lte_fb_text(0, "STAR2LTE XNU PREFLIGHT");
    int cpu0 = (mpidr & 0xffU) == 0;
    int el1 = el == 4;
    int mmu_off = (sctlr & 1U) == 0;
    int granule4k = ((mmfr0 >> 28) & 0xfU) == 0;
    result(1, "CPU0", cpu0);
    result(2, "EL1", el1);
    result(3, "MMU OFF", mmu_off);
    result(4, "4K GRANULE", granule4k);

    int fdt_ok = x0 && (x0 & 7U) == 0 && be32(x0) == FDT_MAGIC;
    int scan_ok = fdt_ok && fdtmem_scan(x0, recs, 160, &map) == 0 &&
                  map.count > 0 && map.dropped == 0 && map.totalsize >= 40;
    result(5, "FDT SCAN", scan_ok);
    uint64_t fdt_size = scan_ok ? map.totalsize : 0;
    result(6, "FDT IN RAM", scan_ok && inside_ram(x0, fdt_size));

    uint64_t rd_s = scan_ok ? map.initrd_start : 0;
    uint64_t rd_e = scan_ok ? map.initrd_end : 0;
    int initrd_ok = scan_ok && rd_s && rd_e > rd_s &&
                    inside_ram(rd_s, rd_e - rd_s);
    result(7, "INITRD IN RAM", initrd_ok);
    result(8, "FDT/INITRD SEP", scan_ok && initrd_ok &&
           !overlaps(x0, fdt_size, rd_s, rd_e - rd_s));

    uint64_t image_size = (uintptr_t)_image_end - (uintptr_t)_start;
    int image_ok = image == (uintptr_t)_start && image_size &&
                   inside_ram(image, image_size);
    result(9, "IMAGE IN RAM", image_ok);
    result(10, "IMAGE SEPARATE", scan_ok && initrd_ok && image_ok &&
           !overlaps(image, image_size, x0, fdt_size) &&
           !overlaps(image, image_size, rd_s, rd_e - rd_s));
    result(11, "FRAMEBUFFER RAM", scan_ok && inside_ram(FB_BASE, FB_SIZE));
    star2lte_fb_hex(12, "MAP RECORDS", scan_ok ? map.count : 0);
    star2lte_fb_hex(13, "INITRD START", rd_s);
    star2lte_fb_hex(14, "INITRD END", rd_e);
    star2lte_fb_hex(15, "FDT", x0);

    int pass = cpu0 && el1 && mmu_off && granule4k && scan_ok &&
               inside_ram(x0, fdt_size) && initrd_ok &&
               !overlaps(x0, fdt_size, rd_s, rd_e - rd_s) &&
               image_ok && !overlaps(image, image_size, x0, fdt_size) &&
               !overlaps(image, image_size, rd_s, rd_e - rd_s) &&
               inside_ram(FB_BASE, FB_SIZE);
    star2lte_fb_text(16, pass ? "PREFLIGHT PASS - NO XNU YET" :
                                      "PREFLIGHT FAIL - DO NOT CONTINUE");
    if (!freq) freq = 26000000U;
    uint64_t t0 = read_counter();
    while ((read_counter() - t0) / freq < HOLD_S) { }
    /* Return to the already-tested probe.S recovery request. */
}
