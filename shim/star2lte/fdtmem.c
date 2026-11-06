/* SPDX-License-Identifier: GPL-2.0-only
 * See fdtmem.h. Every load is a byte or an aligned 32-bit word: the
 * stage-0 images run with the MMU off, where unaligned accesses fault.
 */
#include "fdtmem.h"

#define FDT_MAGIC 0xd00dfeedU
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE 2U
#define FDT_PROP 3U
#define FDT_NOP 4U
#define FDT_END 9U
#define MAX_DEPTH 4         /* root=1, /reserved-memory/<node>=3, +1 spare */
#define NODE_NAME 32
#define MAX_FDT 0x400000U   /* S-Boot's own DTB is ~0x4a000 */

static uint32_t be32(uintptr_t a)
{
    return __builtin_bswap32(*(volatile const uint32_t *)a);
}

static uint64_t be64(uintptr_t a)
{
    return ((uint64_t)be32(a) << 32) | be32(a + 4);
}

static unsigned cstrlen(const char *s)
{
    unsigned n = 0;
    while (s[n])
        ++n;
    return n;
}

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static int starts(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++)
            return 0;
    }
    return 1;
}

/* Copy up to limit-1 characters of a node name, stopping at the @unit. */
static void copy_name(char *dst, const char *src, unsigned limit)
{
    unsigned n = 0;
    while (src[n] && src[n] != '@' && n < limit - 1) {
        dst[n] = src[n];
        ++n;
    }
    dst[n] = 0;
}

struct ctx {
    struct fdtmem_rec *rec;
    unsigned max;
    struct fdtmem_stat *st;
};

static void add(struct ctx *c, const char *name, char kind, uint64_t base, uint64_t size)
{
    if (c->st->count >= c->max) {
        ++c->st->dropped;
        return;
    }
    struct fdtmem_rec *r = &c->rec[c->st->count++];
    copy_name(r->name, name, FDTMEM_NAME);
    r->kind = kind;
    r->base = base;
    r->size = size;
}

static uint64_t cells(uintptr_t a, uint32_t n)
{
    return n == 1 ? be32(a) : be64(a);
}

/* A reg property: (address, size) pairs sized by the parent's cell counts. */
static int add_reg(struct ctx *c, const char *name, char kind, uintptr_t data,
                   uint32_t len, uint32_t ac, uint32_t sc)
{
    if (ac < 1 || ac > 2 || sc < 1 || sc > 2)
        return -1;
    uint32_t stride = (ac + sc) * 4U;
    for (uint32_t off = 0; off + stride <= len; off += stride)
        add(c, name, kind, cells(data + off, ac), cells(data + off + ac * 4U, sc));
    return 0;
}

int fdtmem_scan(uintptr_t fdt, struct fdtmem_rec *rec, unsigned max, struct fdtmem_stat *st)
{
    struct ctx c = { rec, max, st };
    st->totalsize = 0;
    st->count = 0;
    st->dropped = 0;
    st->initrd_start = 0;
    st->initrd_end = 0;
    if ((fdt & 3U) || be32(fdt) != FDT_MAGIC)
        return -1;
    uint32_t total = be32(fdt + 4), off_struct = be32(fdt + 8), off_str = be32(fdt + 12);
    uint32_t off_rsv = be32(fdt + 16), str_size = be32(fdt + 32), struct_size = be32(fdt + 36);
    st->totalsize = total;
    if (total < 40 || total > MAX_FDT || (off_struct & 3U) || off_struct >= total ||
        struct_size > total - off_struct || off_str >= total || str_size > total - off_str ||
        (off_rsv & 3U) || off_rsv >= total)
        return -2;

    for (uintptr_t a = fdt + off_rsv; a + 16 <= fdt + total; a += 16) {
        uint64_t base = be64(a), size = be64(a + 8);
        if (!base && !size)
            break;
        add(&c, "memreserve", FDTMEM_RSVMAP, base, size);
    }

    char names[MAX_DEPTH + 1][NODE_NAME];   /* full names: "reserved-memory" is 15 */
    uint32_t ac[MAX_DEPTH + 1], sc[MAX_DEPTH + 1];
    int reg_seen[MAX_DEPTH + 1];
    uint64_t pending_size[MAX_DEPTH + 1];
    int depth = 0;
    uintptr_t p = fdt + off_struct, end = p + struct_size;
    while (p + 4 <= end) {
        uint32_t tok = be32(p);
        p += 4;
        if (tok == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            uint32_t len = cstrlen(name) + 1U;
            if (len > end - p)
                return -3;
            p = (p + len + 3U) & ~(uintptr_t)3U;
            ++depth;
            if (depth <= MAX_DEPTH) {
                copy_name(names[depth], name, NODE_NAME);
                ac[depth] = 2;   /* DT defaults, used only if the node omits them */
                sc[depth] = 1;
                reg_seen[depth] = 0;
                pending_size[depth] = 0;
            }
        } else if (tok == FDT_END_NODE) {
            if (depth == 3 && starts(names[2], "reserved-memory") &&
                !reg_seen[3] && pending_size[3])
                add(&c, names[3], FDTMEM_DYN, 0, pending_size[3]);
            if (--depth < 0)
                return -4;
        } else if (tok == FDT_PROP) {
            if (p + 8 > end)
                return -5;
            uint32_t len = be32(p), nameoff = be32(p + 4);
            uintptr_t data = p + 8;
            if (len > end - data || nameoff >= str_size)
                return -5;
            p = (data + len + 3U) & ~(uintptr_t)3U;
            const char *pname = (const char *)(fdt + off_str + nameoff);
            if (depth < 1 || depth > MAX_DEPTH)
                continue;
            if (streq(pname, "#address-cells") && len == 4)
                ac[depth] = be32(data);
            else if (streq(pname, "#size-cells") && len == 4)
                sc[depth] = be32(data);
            else if (depth == 2 && starts(names[2], "memory") && streq(pname, "reg")) {
                if (add_reg(&c, names[2], FDTMEM_RAM, data, len, ac[1], sc[1]))
                    return -6;
            } else if (depth == 3 && starts(names[2], "reserved-memory")) {
                if (streq(pname, "reg")) {
                    reg_seen[3] = 1;
                    if (add_reg(&c, names[3], FDTMEM_RESV, data, len, ac[2], sc[2]))
                        return -6;
                } else if (streq(pname, "size") && (len == 4 || len == 8)) {
                    pending_size[3] = cells(data, len / 4U);
                }
            } else if (depth == 2 && streq(names[2], "chosen") && (len == 4 || len == 8)) {
                if (streq(pname, "linux,initrd-start"))
                    st->initrd_start = cells(data, len / 4U);
                else if (streq(pname, "linux,initrd-end"))
                    st->initrd_end = cells(data, len / 4U);
            }
        } else if (tok == FDT_END) {
            break;
        } else if (tok != FDT_NOP) {
            return -7;
        }
    }
    return 0;
}
