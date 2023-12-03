/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */

#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

// Font
#include "font_8x8.h"
#define FONT_CHAR_WIDTH 8
#define FONT_CHAR_HEIGHT 8
#define FONT_N_CHARS 95
#define FONT_FIRST_ASCII 32


uint16_t g_framebuf[FRAME_WIDTH * FRAME_HEIGHT];

void gfx_puttext(uint32_t x0, uint32_t y0, uint32_t bgcol, uint32_t fgcol, const char *text)
{
    for (int y = y0; y < y0 + 8; ++y) {
        uint32_t xbase = x0;
        const char *ptr = text;
        char c;
        while ((c = *ptr++)) {
            uint8_t font_bits = font_8x8[(c - FONT_FIRST_ASCII) + (y - y0) * FONT_N_CHARS];
            for (int i = 0; i < 8; ++i)
                gfx_putpixel(xbase + i, y, font_bits & (1u << i) ? fgcol : bgcol);
            xbase += 8;
        }
    }
}

void gfx_puttextf(uint32_t x0, uint32_t y0, uint32_t bgcol, uint32_t fgcol, const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 128, fmt, args);
    gfx_puttext(x0, y0, bgcol, fgcol, buf);
    va_end(args);
}


void gfx_init(void)
{
    
}
