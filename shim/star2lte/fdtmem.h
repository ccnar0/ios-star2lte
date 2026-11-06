/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>

/* Read-only walker that lists the DRAM banks and every reserved range in
 * the FDT S-Boot hands over (X0 = 0x8A000000 on star2lte). Needs only
 * aligned 32-bit and byte reads, so it is safe with the MMU off. */
#define FDTMEM_NAME 16

enum {
    FDTMEM_RAM = 'M',     /* /memory*@... reg                              */
    FDTMEM_RESV = 'R',    /* /reserved-memory/<node> reg                   */
    FDTMEM_DYN = 'D',     /* /reserved-memory/<node> with size only (no base) */
    FDTMEM_RSVMAP = 'V'   /* header memory-reservation block               */
};

enum {
    FDTMEM_F_NO_MAP = 1U << 0,
    FDTMEM_F_REUSABLE = 1U << 1
};

struct fdtmem_rec {
    char name[FDTMEM_NAME];   /* node name without the @unit-address */
    char kind;
    uint32_t flags;
    uint32_t alloc_range_count; /* zero means no alloc-ranges property */
    uint64_t base, size;
    uint64_t alignment;
    uint64_t alloc_base, alloc_size; /* first alloc-ranges tuple, if present */
};

struct fdtmem_stat {
    uint32_t totalsize;
    uint32_t count;           /* records stored */
    uint32_t dropped;         /* records that did not fit */
    uint64_t initrd_start, initrd_end;   /* /chosen, 0 if absent */
};

/* Returns 0 on success, negative on a malformed or implausible FDT.
 * Disabled nodes are omitted; size-only pools are reported with base 0.
 */
int fdtmem_scan(uintptr_t fdt, struct fdtmem_rec *rec, unsigned max,
                struct fdtmem_stat *st);
