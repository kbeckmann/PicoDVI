/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */
#pragma once

#include <stdint.h>
#include <stdarg.h>

#define FRAME_WIDTH 320 ///< Width of the frame in pixels.
#define FRAME_HEIGHT 240 ///< Height of the frame in pixels.

/**
 * @brief Convert a color from RGB888 format to RGB565 format.
 * @param _r The red component of the color.
 * @param _g The green component of the color.
 * @param _b The blue component of the color.
 * @return The color in RGB565 format.
 */
#define RGB888_TO_RGB565(_r, _g, _b) \
    (                                \
        (((_r) & 0xf8) <<  8) |      \
        (((_g) & 0xfc) <<  3) |      \
        (((_b))        >>  3)        \
    )

extern uint16_t g_framebuf[FRAME_WIDTH * FRAME_HEIGHT]; ///< Frame buffer.

/**
 * @brief Draw a pixel on the screen.
 * @param x The x-coordinate of the pixel.
 * @param y The y-coordinate of the pixel.
 * @param rgb The color of the pixel in RGB565 format.
 */
static inline void gfx_putpixel(uint32_t x, uint32_t y, uint16_t rgb)
{
    uint32_t idx = x + y * FRAME_WIDTH;
    g_framebuf[idx] = rgb;
}

/**
 * @brief Draw text on the screen.
 * @param x0 The x-coordinate of the top-left corner of the text.
 * @param y0 The y-coordinate of the top-left corner of the text.
 * @param bgcol The background color of the text in RGB565 format.
 * @param fgcol The foreground color of the text in RGB565 format.
 * @param text The text to draw.
 */
void gfx_puttext(uint32_t x0, uint32_t y0, uint32_t bgcol, uint32_t fgcol, const char *text);

/**
 * @brief Draw formatted text on the screen.
 * @param x0 The x-coordinate of the top-left corner of the text.
 * @param y0 The y-coordinate of the top-left corner of the text.
 * @param bgcol The background color of the text in RGB565 format.
 * @param fgcol The foreground color of the text in RGB565 format.
 * @param fmt The format string for the text.
 * @param ... The values to substitute into the format string.
 */
void gfx_puttextf(uint32_t x0, uint32_t y0, uint32_t bgcol, uint32_t fgcol, const char *fmt, ...);

/**
 * @brief Get the frame buffer.
 * @return A pointer to the frame buffer.
 */
uint16_t *gfx_get_framebuf(void);

/**
 * @brief Initialize the graphics system.
 */
void gfx_init(void);
