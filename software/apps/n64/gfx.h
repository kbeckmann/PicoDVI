#pragma once

#include <stdint.h>
#include <stdarg.h>

#define FRAME_WIDTH 320
#define FRAME_HEIGHT 240

#define RGB888_TO_RGB565(_r, _g, _b) \
    (                                \
        (((_r) & 0xf8) <<  8) |      \
        (((_g) & 0xfc) <<  3) |      \
        (((_b))        >>  3)        \
    )

extern uint16_t g_framebuf[FRAME_WIDTH * FRAME_HEIGHT];

static inline void gfx_putpixel(uint32_t x, uint32_t y, uint16_t rgb)
{
    uint32_t idx = x + y * FRAME_WIDTH;
    g_framebuf[idx] = rgb;
}

void gfx_puttext(uint32_t x0, uint32_t y0, uint32_t bgcol, uint32_t fgcol, const char *text);
void gfx_puttextf(uint32_t x0, uint32_t y0, uint32_t bgcol, uint32_t fgcol, const char *fmt, ...);

uint16_t *gfx_get_framebuf(void);
void gfx_init(void);

