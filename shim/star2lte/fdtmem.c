/* SPDX-License-Identifier: GPL-2.0-only
 * Bounded, read-only FDT walker for the S-Boot handoff.
 * Every 32-bit load is aligned: the stage-0 images run with the MMU off.
 */
#include "fdtmem.h"

#define FDT_MAGIC 0xd00dfeedU
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE 2U
#define FDT_PROP 3U
#define FDT_NOP 4U
#define FDT_END 9U
#define MAX_DEPTH 4
#define NODE_NAME 32
#define MAX_FDT 0x400000U

static uint32_t be32(uintptr_t a)
{
    return __builtin_bswap32(*(volatile const uint32_t *)a);
}

static uint64_t be64(uintptr_t a)
{
    return ((uint64_t)be32(a) << 32) | be32(a + 4);
}

static unsigned bounded_len(const char *s, unsigned limit)
{
    unsigned n;
    for (n = 0; n < limit && s[n]; ++n)
        ;
    return n;
}

static int str_eq(const char *s, unsigned len, const char *lit)
{
    unsigned i = 0;
    while (lit[i]) {
        if (i >= len || s[i] != lit[i])
            return 0;
        ++i;
    }
    return i < len && s[i] == 0;
}

static int str_starts(const char *s, unsigned len, const char *prefix)
{
    unsigned i = 0;
    while (prefix[i]) {
        if (i >= len || s[i] != prefix[i])
            return 0;
        ++i;
    }
    return 1;
}

static void copy_name(char *dst, const char *src, unsigned len, unsigned limit)
{
    unsigned n = 0;
    while (n < len && src[n] && src[n] != '@' && n < limit - 1) {
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

static void add(struct ctx *c, const char *name, unsigned name_len, char kind,
                uint64_t base, uint64_t size)
{
    if (c->st->count >= c->max) {
        ++c->st->dropped;
        return;
    }
    struct fdtmem_rec *r = &c->rec[c->st->count++];
    copy_name(r->name, name, name_len, FDTMEM_NAME);
    r->kind = kind;
    r->base = base;
    r->size = size;
}

static uint64_t cells(uintptr_t a, uint32_t n)
{
    return n == 1 ? be32(a) : be64(a);
}

/* A reg property: (address, size) pairs sized by the parent's cell counts. */
static int add_reg(struct ctx *c, const char *name, unsigned name_len, char kind,
                   uintptr_t data, uint32_t len, uint32_t ac, uint32_t sc)
{
    if (ac < 1 || ac > 2 || sc < 1 || sc > 2)
        return -1;
    uint32_t stride = (ac + sc) * 4U;
    if (len % stride)
        return -1;
    for (uint32_t off = 0; off < len; off += stride)
        add(c, name, name_len, kind, cells(data + off, ac),
            cells(data + off + ac * 4U, sc));
    return 0;
}

int fdtmem_scan(uintptr_t fdt, struct fdtmem_rec *rec, unsigned max,
                struct fdtmem_stat *st)
{
    struct ctx c = { rec, max, st };
    st->totalsize = 0;
    st->count = 0;
    st->dropped = 0;
    st->initrd_start = 0;
    st->initrd_end = 0;

    if (!fdt || (fdt & 7U) || fdt > UINTPTR_MAX - MAX_FDT ||
        be32(fdt) != FDT_MAGIC)
        return -1;

    uint32_t total = be32(fdt + 4);
    if (total < 40 || total > MAX_FDT || fdt > UINTPTR_MAX - total)
        return -2;

    uint32_t off_struct = be32(fdt + 8);
    uint32_t off_str = be32(fdt + 12);
    uint32_t off_rsv = be32(fdt + 16);
    uint32_t version = be32(fdt + 20);
    uint32_t last_comp = be32(fdt + 24);
    uint32_t str_size = be32(fdt + 32);
    uint32_t struct_size = be32(fdt + 36);

    if (version < 16 || last_comp > version ||
        (off_struct & 3U) || (off_rsv & 7U) ||
        off_struct < 40 || off_struct > total ||
        struct_size > total - off_struct ||
        off_str < 40 || off_str > total ||
        str_size > total - off_str ||
        off_rsv < 40 || off_rsv > total || total - off_rsv < 16 ||
        !struct_size || !str_size)
        return -2;

    st->totalsize = total;

    /* Require a complete, aligned reservation map terminator before using it. */
    uint32_t rsv_end = off_rsv;
    int rsv_terminated = 0;
    while (rsv_end <= total - 16U) {
        uint64_t base = be64(fdt + rsv_end);
        uint64_t size = be64(fdt + rsv_end + 8U);
        rsv_end += 16U;
        if (!base && !size) {
            rsv_terminated = 1;
            break;
        }
    }
    if (!rsv_terminated)
        return -8;
    if (!(rsv_end <= off_struct || off_rsv >= off_struct + struct_size) ||
        !(rsv_end <= off_str || off_rsv >= off_str + str_size))
        return -8;

    for (uint32_t off = off_rsv; off < rsv_end - 16U; off += 16U)
        add(&c, "memreserve", 10, FDTMEM_RSVMAP,
            be64(fdt + off), be64(fdt + off + 8U));

    char names[MAX_DEPTH + 1][NODE_NAME];
    unsigned name_lens[MAX_DEPTH + 1];
    uint32_t ac[MAX_DEPTH + 1], sc[MAX_DEPTH + 1];
    int reg_seen[MAX_DEPTH + 1];
    uint64_t pending_size[MAX_DEPTH + 1];
    int depth = 0, root_seen = 0, ended = 0;
    uint32_t p = off_struct, end = off_struct + struct_size;

    while (p <= end - 4U) {
        uint32_t tok = be32(fdt + p);
        p += 4U;

        if (tok == FDT_BEGIN_NODE) {
            uint32_t remain = end - p;
            unsigned len = bounded_len((const char *)(fdt + p), remain);
            if (len == remain)
                return -3;
            uint32_t padded = (len + 1U + 3U) & ~3U;
            if (padded > remain)
                return -3;
            if (depth == 0) {
                if (root_seen || len != 0)
                    return -3;
                root_seen = 1;
            }
            ++depth;
            if (depth <= MAX_DEPTH) {
                const char *name = (const char *)(fdt + p);
                copy_name(names[depth], name, len, NODE_NAME);
                name_lens[depth] = bounded_len(names[depth], NODE_NAME);
                ac[depth] = 2;
                sc[depth] = 1;
                reg_seen[depth] = 0;
                pending_size[depth] = 0;
            }
            p += padded;
        } else if (tok == FDT_END_NODE) {
            if (depth <= 0)
                return -4;
            if (depth == 3 && str_starts(names[2], name_lens[2], "reserved-memory") &&
                !reg_seen[3] && pending_size[3])
                add(&c, names[3], name_lens[3], FDTMEM_DYN, 0, pending_size[3]);
            --depth;
        } else if (tok == FDT_PROP) {
            if (p > end - 8U)
                return -5;
            uint32_t len = be32(fdt + p);
            uint32_t nameoff = be32(fdt + p + 4U);
            uint32_t data_off = p + 8U;
            if (len > end - data_off || nameoff >= str_size)
                return -5;
            uint32_t padded_len = (len + 3U) & ~3U;
            if (padded_len < len || padded_len > end - data_off)
                return -5;
            const char *pname = (const char *)(fdt + off_str + nameoff);
            unsigned pname_len = bounded_len(pname, str_size - nameoff);
            if (pname_len == str_size - nameoff)
                return -5;
            p = data_off + padded_len;

            if (depth < 1 || depth > MAX_DEPTH)
                continue;
            uintptr_t data = fdt + data_off;
            if (str_eq(pname, pname_len + 1U, "#address-cells") && len == 4) {
                ac[depth] = be32(data);
            } else if (str_eq(pname, pname_len + 1U, "#size-cells") && len == 4) {
                sc[depth] = be32(data);
            } else if (depth == 2 &&
                       str_starts(names[2], name_lens[2], "memory") &&
                       str_eq(pname, pname_len + 1U, "reg")) {
                if (add_reg(&c, names[2], name_lens[2], FDTMEM_RAM,
                            data, len, ac[1], sc[1]))
                    return -6;
            } else if (depth == 3 &&
                       str_starts(names[2], name_lens[2], "reserved-memory")) {
                if (str_eq(pname, pname_len + 1U, "reg")) {
                    reg_seen[3] = 1;
                    if (add_reg(&c, names[3], name_lens[3], FDTMEM_RESV,
                                data, len, ac[2], sc[2]))
                        return -6;
                } else if (str_eq(pname, pname_len + 1U, "size") &&
                           (len == 4 || len == 8)) {
                    pending_size[3] = cells(data, len / 4U);
                }
            } else if (depth == 2 &&
                       str_eq(names[2], name_lens[2] + 1U, "chosen") &&
                       (len == 4 || len == 8)) {
                if (str_eq(pname, pname_len + 1U, "linux,initrd-start"))
                    st->initrd_start = cells(data, len / 4U);
                else if (str_eq(pname, pname_len + 1U, "linux,initrd-end"))
                    st->initrd_end = cells(data, len / 4U);
            }
        } else if (tok == FDT_END) {
            if (!root_seen || depth != 0)
                return -7;
            ended = 1;
            break;
        } else if (tok != FDT_NOP) {
            return -7;
        }
    }
    return ended ? 0 : -7;
}
