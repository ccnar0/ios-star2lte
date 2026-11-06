/* SPDX-License-Identifier: GPL-2.0-only
 * STAR2LTE read-only XNU loader/memory-ownership audit.
 * Not an allocator or XNU loader: it validates S-Boot handoff and refuses
 * to choose RAM while size-only reserved pools have no physical base.
 * No memory allocation/overwrite, MMU, PSCI, UFS, PMU or other-core writes.
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
static uint64_t read_counter(void) {
    uint64_t v; __asm__ volatile("isb; mrs %0, cntpct_el0" : "=r"(v)); return v;
}
static uint32_t be32(uintptr_t p) { return __builtin_bswap32(*(volatile const uint32_t *)p); }
static int range_end(uint64_t b,uint64_t s,uint64_t *e) {
    if (!s || b > UINT64_MAX-s) return 0; *e=b+s; return 1;
}
static int inside_ram(uint64_t b,uint64_t s) {
    uint64_t e; if (!range_end(b,s,&e)) return 0;
    for (unsigned i=0;i<map.count;i++) { uint64_t re;
        if (recs[i].kind==FDTMEM_RAM && range_end(recs[i].base,recs[i].size,&re) &&
            b>=recs[i].base && e<=re) return 1;
    } return 0;
}
static int overlaps(uint64_t a,uint64_t as,uint64_t b,uint64_t bs) {
    uint64_t ae,be; if (!range_end(a,as,&ae)||!range_end(b,bs,&be)) return 1;
    return a<be && b<ae;
}
static void result(unsigned row,const char *name,int ok) {
    char line[32]; unsigned n=0;
    while(name[n]&&n<19){line[n]=name[n];++n;} line[n++]=' ';
    const char *s=ok?"PASS":"FAIL"; for(unsigned i=0;i<4;i++)line[n++]=s[i];
    line[n]=0; star2lte_fb_text(row,line);
}
void star2lte_probe_main(uintptr_t x0,uintptr_t x1,uintptr_t x2,uintptr_t x3,
                         uint64_t el,uint64_t sctlr,uint64_t image) {
    (void)x1;(void)x2;(void)x3;
    uint64_t mpidr,mmfr0,freq;
    __asm__ volatile("mrs %0, mpidr_el1":"=r"(mpidr));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1":"=r"(mmfr0));
    __asm__ volatile("mrs %0, cntfrq_el0":"=r"(freq));
    star2lte_fb_enable(); star2lte_fb_text(0,"STAR2LTE LOADER AUDIT");
    int cpu0=(mpidr&0xffU)==0, el1=el==4, mmu_off=(sctlr&1U)==0;
    int granule4k=((mmfr0>>28)&0xfU)==0;
    result(1,"CPU0",cpu0); result(2,"EL1",el1);
    result(3,"MMU OFF",mmu_off); result(4,"4K GRANULE",granule4k);
    int fdt_safe=x0>=0x80000000UL&&x0<0xbc800000UL;
    int fdt_ok=fdt_safe&&!(x0&7U)&&be32(x0)==FDT_MAGIC;
    int scan_ok=fdt_ok&&fdtmem_scan(x0,recs,160,&map)==0&&map.count>0&&
                map.dropped==0&&map.totalsize>=40;
    uint64_t fs=scan_ok?map.totalsize:0, rs=scan_ok?map.initrd_start:0;
    uint64_t re=scan_ok?map.initrd_end:0, is=(uintptr_t)_image_end-(uintptr_t)_start;
    int fdt_ram=scan_ok&&inside_ram(x0,fs);
    int initrd_ok=scan_ok&&rs&&re>rs&&inside_ram(rs,re-rs);
    int image_ok=image==(uintptr_t)_start&&is&&inside_ram(image,is);
    int separate=scan_ok&&initrd_ok&&image_ok&&
        !overlaps(x0,fs,rs,re-rs)&&!overlaps(image,is,x0,fs)&&
        !overlaps(image,is,rs,re-rs);
    int fb_ok=scan_ok&&inside_ram(FB_BASE,FB_SIZE);
    result(5,"FDT SCAN",scan_ok); result(6,"FDT IN RAM",fdt_ram);
    result(7,"INITRD IN RAM",initrd_ok); result(8,"IMAGE IN RAM",image_ok);
    result(9,"OBJECTS SEPARATE",separate); result(10,"FRAMEBUFFER RAM",fb_ok);
    unsigned ram=0,fixed=0,dynamic=0,rsvmap=0;
    for(unsigned i=0;scan_ok&&i<map.count;i++) switch(recs[i].kind) {
        case FDTMEM_RAM:++ram;break; case FDTMEM_RESV:++fixed;break;
        case FDTMEM_DYN:++dynamic;break; case FDTMEM_RSVMAP:++rsvmap;break;
    }
    star2lte_fb_hex(11,"RAM BANKS",ram);
    star2lte_fb_hex(12,"FIXED RESERVED",fixed);
    star2lte_fb_hex(13,"DYNAMIC POOLS",dynamic);
    star2lte_fb_hex(14,"RSVMAP",rsvmap);
    /* Size-only pools have no physical base. Never guess their placement. */
    star2lte_fb_text(15,dynamic?"RAM PLAN BLOCKED":"RAM PLAN NEEDS REVIEW");
    star2lte_fb_text(16,dynamic?"DYNAMIC POOLS UNPLACED":"AUDIT ONLY - NO ALLOCATION");
    star2lte_fb_text(17,"NO BOOT_ARGS / NO XNU JUMP");
    if(!freq)freq=26000000U;
    uint64_t t0=read_counter(); while((read_counter()-t0)/freq<HOLD_S) { }
    /* Shared entry wrapper returns to the known recovery request. */
}
