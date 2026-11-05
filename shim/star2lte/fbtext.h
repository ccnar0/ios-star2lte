/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>

/* S-Boot scanout buffer. 1440x2960 ARGB8888, from uniLoader
 * board-starlte.c (the S9 and S9+ share the panel and the Exynos 9810
 * S-Boot framebuffer layout). */
#define STAR2LTE_FB_BASE 0xcc000000UL
#define STAR2LTE_FB_WIDTH 1440U
#define STAR2LTE_FB_HEIGHT 2960U

/* Let CPU writes reach the panel (DECON_F HW trigger), returns old value. */
uint32_t star2lte_fb_enable(void);
void star2lte_fb_text(unsigned row, const char *s);
void star2lte_fb_hex(unsigned row, const char *label, uint64_t value);
