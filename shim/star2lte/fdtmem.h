/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>

/* Read-only walker that lists the DRAM banks and every reserved range in
 * the FDT S-Boot hands over (X0 = 0x8A000000 on star2lte). Needs only
 * aligned 32-bit and byte reads, so it is safe with the MMU off. */
#define FDTMEM_NAME 13

enum {
    FDTMEM_RAM = 'M',     /* /memory*@... reg                              */
    FDTMEM_RESV = 'R',    /* /reserved-memory/<node> reg                   */
    FDTMEM_DYN = 'D',     /* /reserved-memory/<node> with size only (no base) */
    FDTMEM_RSVMAP = 'V'   /* header memory-reservation block               */
};

struct fdtmem_rec {
    char name[FDTMEM_NAME];   /* node name without the @unit-address */
    char kind;
    uint64_t base, size;
};

struct fdtmem_stat {
    uint32_t totalsize;
    uint32_t count;           /* records stored */
    uint32_t dropped;         /* records that did not fit */
    uint64_t initrd_start, initrd_end;   /* /chosen, 0 if absent */
};

/* Returns 0 on success, negative on a malformed or implausible FDT. */
int fdtmem_scan(uintptr_t fdt, struct fdtmem_rec *rec, unsigned max,
                struct fdtmem_stat *st);
