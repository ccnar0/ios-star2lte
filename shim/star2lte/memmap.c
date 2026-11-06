/* SPDX-License-Identifier: GPL-2.0-only
 * star2lte (SM-G965F/DS, Exynos 9810) stage-0 memory-map probe: walk the
 * FDT S-Boot passes in X0 (0x8A000000 in the v2 run) and print every DRAM
 * bank and reserved range on the firmware framebuffer, paged, so the XNU
 * RAM window and the payload addresses can be picked from evidence instead
 * of the mainline starlte DTS. Reuses probe.S (no storage, EFS, MMU or
 * bootloader writes; PMU recovery reboot at the end).
 */
#include <stdint.h>
#include "fbtext.h"
#include "fdtmem.h"

#define MAX_RECS 160U
#define PER_PAGE 16U
#define PAGE_S 20U
#define HOLD_S 300U
#define FDT_DEFAULT 0x8A000000UL   /* X0 in the v2 on-device run */

static struct fdtmem_rec recs[MAX_RECS];
static struct fdtmem_stat stat;

static uint64_t counter(void)
{
    uint64_t value;
    __asm__ volatile("isb; mrs %0, cntpct_el0" : "=r" (value));
    return value;
}

/* Compact upper-case hex, no 0x, returns the new write position. */
static unsigned put_hex(char *dst, unsigned n, uint64_t v)
{
    unsigned digits = 1;
    while (digits < 16 && (v >> (4 * digits)))
        ++digits;
    for (unsigned i = digits; i-- > 0;) {
        unsigned d = (unsigned)(v >> (4 * i)) & 15U;
        dst[n++] = (char)(d < 10 ? '0' + d : 'A' + d - 10);
    }
    return n;
}

static unsigned put_dec(char *dst, unsigned n, unsigned v)
{
    char tmp[10];
    unsigned t = 0;
    do {
        tmp[t++] = (char)('0' + v % 10U);
        v /= 10U;
    } while (v);
    while (t)
        dst[n++] = tmp[--t];
    return n;
}

static void show_rec(unsigned row, const struct fdtmem_rec *r)
{
    char line[96];
    unsigned n = 0;
    line[n++] = r->kind;
    line[n++] = ' ';
    for (unsigned i = 0; r->name[i] && i < FDTMEM_NAME - 1U; ++i)
        line[n++] = r->name[i];
    line[n++] = ' ';
    n = put_hex(line, n, r->base);
    line[n++] = ' ';
    n = put_hex(line, n, r->size);
    if (r->kind == FDTMEM_DYN) {
        line[n++] = ' ';
        line[n++] = 'A';
        n = put_hex(line, n, r->alignment);
        if (r->alloc_range_count) {
            line[n++] = ' ';
            line[n++] = 'R';
            n = put_hex(line, n, r->alloc_base);
            line[n++] = '+';
            n = put_hex(line, n, r->alloc_size);
            if (r->alloc_range_count > 1) {
                line[n++] = ' ';
                line[n++] = 'N';
                n = put_dec(line, n, r->alloc_range_count);
            }
        }
        if (r->flags) {
            line[n++] = ' ';
            line[n++] = 'F';
            n = put_hex(line, n, r->flags);
        }
    }
    line[n] = 0;
    star2lte_fb_text(row, line);
}

static void show_page(unsigned page, unsigned pages)
{
    char line[40];
    unsigned n = 0, i;
    line[n++] = 'P';
    line[n++] = ' ';
    n = put_dec(line, n, page + 1U);
    line[n++] = '/';
    n = put_dec(line, n, pages);
    line[n++] = ' ';
    line[n++] = 'N';
    line[n++] = ' ';
    n = put_dec(line, n, stat.count);
    if (stat.dropped) {
        line[n++] = ' ';
        line[n++] = 'D';
        n = put_dec(line, n, stat.dropped);
    }
    line[n] = 0;
    star2lte_fb_text(2, line);
    for (i = 0; i < PER_PAGE; ++i) {
        unsigned idx = page * PER_PAGE + i;
        if (idx < stat.count)
            show_rec(3 + i, &recs[idx]);
        else
            star2lte_fb_text(3 + i, "");
    }
}

void star2lte_probe_main(uintptr_t x0, uintptr_t x1, uintptr_t x2, uintptr_t x3,
                         uint64_t el, uint64_t sctlr, uint64_t image)
{
    (void)x1; (void)x2; (void)x3; (void)el; (void)sctlr; (void)image;
    uint64_t freq;
    uintptr_t fdt = x0 ? x0 : FDT_DEFAULT;
    star2lte_fb_enable();
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r" (freq));
    if (!freq)
        freq = 26000000U;   /* CNTFRQ_EL0 is 0 on this S-Boot (v2 run) */

    star2lte_fb_text(0, "STAR2LTE MEMMAP 1");
    star2lte_fb_hex(1, "FDT", fdt);
    int rc = fdtmem_scan(fdt, recs, MAX_RECS, &stat);
    if (rc) {
        star2lte_fb_hex(2, "SCAN FAIL", (uint64_t)(int64_t)rc);
        star2lte_fb_hex(3, "TOTALSIZE", stat.totalsize);
    } else {
        unsigned pages = (stat.count + PER_PAGE - 1U) / PER_PAGE;
        if (!pages)
            pages = 1;
        if (stat.initrd_end > stat.initrd_start) {
            star2lte_fb_hex(20, "INITRD S", stat.initrd_start);
            star2lte_fb_hex(21, "INITRD E", stat.initrd_end);
        }
        uint64_t t0 = counter();
        unsigned shown = ~0U;
        for (;;) {
            uint64_t elapsed = (counter() - t0) / freq;
            if (elapsed >= HOLD_S)
                return;
            unsigned page = (unsigned)(elapsed / PAGE_S) % pages;
            if (page != shown) {
                show_page(page, pages);
                shown = page;
            }
        }
    }
    uint64_t t0 = counter();
    while ((counter() - t0) / freq < 60U)
        ;
}
