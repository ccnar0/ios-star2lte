/* Host tests for fdtmem.c. Build with dtc and a C11 compiler. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fdtmem.h"

static int fails;
#define EXPECT(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static uint32_t get_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void put_be32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static int has(const struct fdtmem_rec *r, unsigned n, char kind, const char *name,
               uint64_t base, uint64_t size)
{
    for (unsigned i = 0; i < n; ++i)
        if (r[i].kind == kind && !strcmp(r[i].name, name) &&
            r[i].base == base && r[i].size == size)
            return 1;
    return 0;
}

static void expect_bad(const unsigned char *src, size_t len, unsigned mode)
{
    size_t alloc_len = (len + 7U) & ~(size_t)7U;
    unsigned char *copy = aligned_alloc(8, alloc_len);
    struct fdtmem_rec rec[64];
    struct fdtmem_stat st;
    EXPECT(copy != NULL);
    if (!copy)
        return;
    memcpy(copy, src, len);

    uint32_t off_struct = get_be32(copy + 8);
    uint32_t off_str = get_be32(copy + 12);
    uint32_t off_rsv = get_be32(copy + 16);
    uint32_t str_size = get_be32(copy + 32);

    switch (mode) {
    case 0: /* impossible total size */
        put_be32(copy + 4, 0xffffffffU);
        break;
    case 1: /* structure has no complete token stream */
        put_be32(copy + 36, 4);
        break;
    case 2: /* root node must have an empty name */
        copy[off_struct + 4] = 'x';
        break;
    case 3: /* string table must contain a NUL for every referenced name */
        EXPECT(str_size > 0);
        copy[off_str + str_size - 1U] = 0x7f;
        break;
    case 4: /* reservation map must terminate before the end of the blob */
        memset(copy + off_rsv, 0xa5, len - off_rsv);
        break;
    default:
        EXPECT(0);
        break;
    }
    EXPECT(fdtmem_scan((uintptr_t)copy, rec, 64, &st) < 0);
    free(copy);
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    unsigned char *buf = aligned_alloc(8, 1U << 20);
    if (!buf) {
        fclose(f);
        return 2;
    }
    size_t len = fread(buf, 1, 1U << 20, f);
    int io_error = ferror(f);
    fclose(f);
    if (io_error || len < 40) {
        free(buf);
        return 2;
    }

    struct fdtmem_rec rec[64];
    struct fdtmem_stat st;
    int rc = fdtmem_scan((uintptr_t)buf, rec, 64, &st);
    printf("rc=%d total=%u (file %zu) count=%u dropped=%u\n",
           rc, st.totalsize, len, st.count, st.dropped);
    for (unsigned i = 0; i < st.count; ++i)
        printf("%c %-12s %llx %llx\n", rec[i].kind, rec[i].name,
               (unsigned long long)rec[i].base, (unsigned long long)rec[i].size);
    EXPECT(rc == 0);
    EXPECT(st.totalsize == len);
    EXPECT(has(rec, st.count, 'M', "memory", 0x80000000ULL, 0x3c800000ULL));
    EXPECT(has(rec, st.count, 'M', "memory", 0x880000000ULL, 0x80000000ULL));
    EXPECT(has(rec, st.count, 'M', "memory", 0xe1900000ULL, 0x1e700000ULL));
    EXPECT(has(rec, st.count, 'R', "ecdump", 0x8e000000ULL, 0x1000000ULL));
    EXPECT(has(rec, st.count, 'R', "tima", 0xb1000000ULL, 0x500000ULL));
    EXPECT(has(rec, st.count, 'D', "pool", 0, 0x400000ULL));
    EXPECT(has(rec, st.count, 'V', "memreserve", 0x80000000ULL, 0x10000ULL));
    EXPECT(!has(rec, st.count, 'D', "tima", 0, 0));
    EXPECT(st.initrd_start == 0x82000000ULL && st.initrd_end == 0x821fffffULL);

    for (unsigned mode = 0; mode < 5; ++mode)
        expect_bad(buf, len, mode);

    /* A short output array must report, not overwrite, dropped records. */
    rc = fdtmem_scan((uintptr_t)buf, rec, 2, &st);
    EXPECT(rc == 0);
    EXPECT(st.count == 2 && st.dropped > 0);

    free(buf);
    printf(fails ? "FAILED (%d)\n" : "OK\n", fails);
    return fails != 0;
}
