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
        if (r[i].kind == kind && !strcmp(r[i].name, name) &&
            r[i].base == base && r[i].size == size)
            return 1;
    return 0;
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
    printf("star2lte-style rc=%d total=%u file=%zu count=%u dropped=%u\n",
           rc, st.totalsize, len, st.count, st.dropped);
    EXPECT(rc == 0);
    EXPECT(st.totalsize == len);
    EXPECT(has(rec, st.count, 'M', "memory", 0x80000000ULL, 0x40000000ULL));
    EXPECT(has(rec, st.count, 'M', "memory", 0x900000000ULL, 0x80000000ULL));
    EXPECT(has(rec, st.count, 'R', "modem_if", 0xf0000000ULL, 0x09200000ULL));
    EXPECT(has(rec, st.count, 'R', "secure_camera", 0xe1900000ULL, 0x01e00000ULL));
    EXPECT(has(rec, st.count, 'R', "exynos_ss", 0xfda00000ULL, 0x01c08000ULL));
    EXPECT(has(rec, st.count, 'D', "video_stream", 0, 0x06c00000ULL));
    EXPECT(!has(rec, st.count, 'R', "tima_region", 0xb8000000ULL, 0x200000ULL));
    EXPECT(!has(rec, st.count, 'R', "header", 0xfda00000ULL, 0x10000ULL));

    free(buf);
    printf(fails ? "FAILED (%d)\n" : "OK\n", fails);
    return fails != 0;
}
