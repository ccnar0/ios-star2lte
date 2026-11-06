/* Host test for fdtmem.c: gcc -O2 -Wall -Wextra host_fdtmem_test.c fdtmem.c
 * dtc -I dts -O dtb test_memmap.dts -o test_memmap.dtb && ./a.out test_memmap.dtb */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fdtmem.h"

static int fails;
#define EXPECT(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static int has(const struct fdtmem_rec *r, unsigned n, char kind, const char *name,
               uint64_t base, uint64_t size)
{
    for (unsigned i = 0; i < n; ++i)
        if (r[i].kind == kind && !strcmp(r[i].name, name) && r[i].base == base && r[i].size == size)
            return 1;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    FILE *f = fopen(argv[1], "rb");
    void *buf = aligned_alloc(8, 1 << 20);
    size_t len = fread(buf, 1, 1 << 20, f);
    fclose(f);
    struct fdtmem_rec rec[64];
    struct fdtmem_stat st;
    int rc = fdtmem_scan((uintptr_t)buf, rec, 64, &st);
    printf("rc=%d total=%u (file %zu) count=%u dropped=%u\n", rc, st.totalsize, len, st.count, st.dropped);
    for (unsigned i = 0; i < st.count; ++i)
        printf("%c %-12s %llx %llx\n", rec[i].kind, rec[i].name,
               (unsigned long long)rec[i].base, (unsigned long long)rec[i].size);
    EXPECT(rc == 0);
    EXPECT(has(rec, st.count, 'M', "memory", 0x80000000ULL, 0x3c800000ULL));
    EXPECT(has(rec, st.count, 'M', "memory", 0x880000000ULL, 0x80000000ULL));
    EXPECT(has(rec, st.count, 'M', "memory", 0xe1900000ULL, 0x1e700000ULL));
    EXPECT(has(rec, st.count, 'R', "ecdump", 0x8e000000ULL, 0x1000000ULL));
    EXPECT(has(rec, st.count, 'R', "tima", 0xb1000000ULL, 0x500000ULL));
    EXPECT(has(rec, st.count, 'D', "pool", 0, 0x400000ULL));
    EXPECT(has(rec, st.count, 'V', "memreserve", 0x80000000ULL, 0x10000ULL));
    EXPECT(!has(rec, st.count, 'D', "tima", 0, 0));   /* reg wins over size */
    EXPECT(st.initrd_start == 0x82000000ULL && st.initrd_end == 0x821fffffULL);
    /* truncated / corrupt input must fail, not crash */
    ((uint32_t *)buf)[0] = 0;
    EXPECT(fdtmem_scan((uintptr_t)buf, rec, 64, &st) < 0);
    /* a tiny record array reports dropped records */
    ((uint32_t *)buf)[0] = __builtin_bswap32(0xd00dfeedU);
    fdtmem_scan((uintptr_t)buf, rec, 2, &st);
    EXPECT(st.count == 2 && st.dropped > 0);
    printf(fails ? "FAILED\n" : "OK\n");
    return fails != 0;
}
