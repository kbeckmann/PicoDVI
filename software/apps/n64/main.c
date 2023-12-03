// #pragma GCC optimize("Os")
// #pragma GCC optimize("O2")
#pragma GCC optimize("O3")


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "dvi.h"
#include "dvi_serialiser.h"
#include "common_dvi_pin_configs.h"
#include "sprite.h"

#include "n64.pio.h"


// Uncomment to print diagnostic data on the screen
// #define DIAGNOSTICS

// Font
#include "font_8x8.h"
#define FONT_CHAR_WIDTH 8
#define FONT_CHAR_HEIGHT 8
#define FONT_N_CHARS 95
#define FONT_FIRST_ASCII 32

// Row configuration for PAL vs NTSC
#define DEFAULT_CROP_Y_PAL  (90)
#define DEFAULT_CROP_Y_NTSC (25)
#define ROWS_PAL            (615)
#define ROWS_NTSC           (511)
#define ROWS_TOLERANCE      (5)
#define IN_RANGE(__x, __low, __high) (((__x) >= (__low)) && ((__x) <= (__high)))
#define IN_TOLERANCE(__x, __value, __tolerance) IN_RANGE(__x, (__value - __tolerance), (__value + __tolerance))

// TMDS bit clock 252 MHz
// DVDD 1.2V (1.1V seems ok too)
#define FRAME_WIDTH 320
#define FRAME_HEIGHT 240
#define VREG_VSEL VREG_VOLTAGE_1_20
#define DVI_TIMING dvi_timing_640x480p_60hz

// UART config on the last GPIOs
#define UART_TX_PIN (28)
#define UART_RX_PIN (29) /* not available on the pico */
#define UART_ID     uart0
#define BAUD_RATE   115200

#define USE_RGB555

#define RGB888_TO_RGB565(_r, _g, _b) \
    (                                \
        (((_r) & 0xf8) <<  8) |      \
        (((_g) & 0xfc) <<  3) |      \
        (((_b))        >>  3)        \
    )

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

const PIO pio = pio1;
const uint sm = 0;
struct dvi_inst dvi0;
uint16_t framebuf[FRAME_WIDTH * FRAME_HEIGHT];

void core1_main(void)
{
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    dvi_start(&dvi0);
    dvi_scanbuf_main_16bpp(&dvi0);
    __builtin_unreachable();
}

void core1_scanline_callback(void)
{
    // Discard any scanline pointers passed back
    uint16_t *bufptr;
    while (queue_try_remove_u32(&dvi0.q_colour_free, &bufptr))
        ;
    // Note first two scanlines are pushed before DVI start
    static uint scanline = 2;
    bufptr = &framebuf[FRAME_WIDTH * scanline];
    queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);
    scanline = (scanline + 1) % FRAME_HEIGHT;
}

static inline void putpixel(uint x, uint y, uint16_t rgb)
{
    uint idx = x + y * FRAME_WIDTH;
    framebuf[idx] = rgb;
}

void puttext(uint x0, uint y0, uint bgcol, uint fgcol, const char *text)
{
    for (int y = y0; y < y0 + 8; ++y) {
        uint xbase = x0;
        const char *ptr = text;
        char c;
        while ((c = *ptr++)) {
            uint8_t font_bits = font_8x8[(c - FONT_FIRST_ASCII) + (y - y0) * FONT_N_CHARS];
            for (int i = 0; i < 8; ++i)
                putpixel(xbase + i, y, font_bits & (1u << i) ? fgcol : bgcol);
            xbase += 8;
        }
    }
}

void puttextf(uint x0, uint y0, uint bgcol, uint fgcol, const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 128, fmt, args);
    puttext(x0, y0, bgcol, fgcol, buf);
    va_end(args);
}

int main(void)
{
    vreg_set_voltage(VREG_VSEL);
    sleep_ms(10);
#ifdef RUN_FROM_CRYSTAL
    set_sys_clock_khz(12000, true);
#else
    // Run system at TMDS bit clock (252.000 MHz)
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);
#endif

    // setup_default_uart();
    stdio_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);

    printf("Configuring DVI\n");

    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
    dvi0.scanline_callback = core1_scanline_callback;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    // Once we've given core 1 the framebuffer, it will just keep on displaying
    // it without any intervention from core 0

#ifdef DIAGNOSTICS
    // Fill with red
    sprite_fill16(framebuf, RGB888_TO_RGB565(0xFF, 0x00, 0x00), FRAME_WIDTH * FRAME_HEIGHT);
#else
    // Fill with black
    sprite_fill16(framebuf, RGB888_TO_RGB565(0x00, 0x00, 0x00), FRAME_WIDTH * FRAME_HEIGHT);
#endif

    uint16_t *bufptr = framebuf;
    queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);
    bufptr += FRAME_WIDTH;
    queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);

    printf("Core 1 start\n");
    multicore_launch_core1(core1_main);

    printf("Start rendering\n");

    for (int i = 0; i <= 8; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_set_pulls(i, false, false);
    }

    // Init PIO before starting the second core
    uint offset = pio_add_program(pio, &n64_program);
    n64_program_init(pio, 0, offset);
    pio_sm_set_enabled(pio, 0, true);

    int count = 0;
    int row = 0;
    int column = 0;

    #define CSYNCB_POS (0)
    #define HSYNCB_POS (1)
    #define CLAMPB_POS (2)
    #define VSYNCB_POS (3)

    #define CSYNCB_MASK (1 << CSYNCB_POS)
    #define HSYNCB_MASK (1 << HSYNCB_POS)
    #define CLAMPB_MASK (1 << CLAMPB_POS)
    #define VSYNCB_MASK (1 << VSYNCB_POS)

    #define ACTIVE_PIXEL_MASK (VSYNCB_MASK | HSYNCB_MASK | CLAMPB_MASK)

    /*
    0      8       10   15    1B  1F
                    v    v     v   v
                    RRRRRGGGGGGBBBBB
   xBBBBBBBxGGGGGGGxRRRRRRRXXXXVLHC
                 BBBBBBBxGGGGGGGxRRRRRRRxXXXXVLHC
                               BBBBBBBxGGGGGGGxRRRRRRRxXXXXVLHC
                 BBBBBBBxGGGGGGGxRRRRRRRxXXXXVLHC
    */

    uint32_t BGRS;
    uint32_t frame = 0;
    uint32_t crop_y = DEFAULT_CROP_Y_PAL;

#ifdef DIAGNOSTICS
    const volatile uint32_t *pGetTime = &timer_hw->timerawl;
    uint32_t t0 = 0;
    uint32_t t1 = 0;
#endif

    while (1) {
        // printf("START\n");

        // 1. Find posedge VSYNC
        do {
            BGRS = pio_sm_get_blocking(pio, sm);
        } while (!(BGRS & VSYNCB_MASK));

        // printf("VSYNC\n");

        int active_row = 0;
        for (row = 0; ; row++) {

            int skip_row = (
                (row % 2 != 0) ||            // Skip every second line (TODO: Add blend option later)
                (row < crop_y) ||            // crop_y, number of rows to skip vertically from the top
                (active_row >= FRAME_HEIGHT) // Never attempt to write more rows than the framebuffer
            );

            // 2. Find posedge HSYNC
            do {
                BGRS = pio_sm_get_blocking(pio, sm);

                if ((BGRS & VSYNCB_MASK) == 0) {
                    // VSYNC found, time to quit
                    goto end_of_line;
                }

            } while ((BGRS & ACTIVE_PIXEL_MASK) != ACTIVE_PIXEL_MASK);

            if (skip_row) {
                // Skip rows based on logic above
                do {
                    BGRS = pio_sm_get_blocking(pio, sm);

                    if ((BGRS & VSYNCB_MASK) == 0) {
                        // VSYNC found, time to quit
                        goto end_of_line;
                    }
                } while ((BGRS & ACTIVE_PIXEL_MASK) == ACTIVE_PIXEL_MASK);

                continue;
            }

            // printf("HSYNC\n");
            count = active_row * FRAME_WIDTH;
            int count_max = count + FRAME_WIDTH;
            active_row++;

            column = 0;

            // 3.  Capture scanline

            // 3.1 Crop left black bar
            // const int left_crop = 42; // LibDragon 320x240 left-aligned
            const int left_crop = 36; // LibDragon 640x240 left-aligned
            for (int left_ctr = 0; left_ctr < left_crop; left_ctr++) {
                BGRS = pio_sm_get_blocking(pio, sm);
            };

            // 3.2 Capture active pixels
            BGRS = pio_sm_get_blocking(pio, sm);
            do {
                // 3.3 Convert to RGB565 or 555
                framebuf[count++] = (
#if defined(USE_RGB565)
                    ((BGRS <<  1) & 0xf800) |
                    ((BGRS >> 12) & 0x07e0) |
                    ((BGRS >> 26) & 0x001f)
                    // | 0x1f // Uncomment to tint everything with blue
#elif defined(USE_RGB555)
                    ((BGRS <<  1) & 0xf800) |
                    ((BGRS >> 12) & 0x07c0) | // Mask so only 5 bits for green are used
                    ((BGRS >> 26) & 0x001f)
                    // | 0x1f // Uncomment to tint everything with blue
#else
#error Define USE_RGB565 or USE_RGB555
#endif
                );

                // Never write more than the line width.
                // Input might be weird and have too many active pixels - discard in those cases.
                if (count >= count_max) {
                    do {
                        // Consume all active pixels
                        BGRS = pio_sm_get_blocking(pio, sm);
                    } while ((BGRS & ACTIVE_PIXEL_MASK) == ACTIVE_PIXEL_MASK);
                    break;
                }

                // 3.4 Skip every second pixel
                BGRS = pio_sm_get_blocking(pio, sm);
                column++;

                // Skip one extra pixel, for debugging
                // BGRS = pio_sm_get_blocking(pio, sm);
                column++;

                // Fetch new pixel in the end, so the loop logic can react to it first
                BGRS = pio_sm_get_blocking(pio, sm);

#if 0
                // Optional code to check for active pixels.
                // Disabled for performance reasons.
                if ((BGRS & ACTIVE_PIXEL_MASK) != ACTIVE_PIXEL_MASK) {
                    break;
                }
#endif
            } while (1);
        }

end_of_line:
        // Show diagnostic information every 100 frames, for 1 second

#ifdef DIAGNOSTICS
        if (frame % (50) == 0) {
            uint32_t y = 0;
            t1 = *pGetTime;

            puttextf(0, ++y * 8, 0xffff, 0x0000, "Delta %d", (t1 - t0));
            puttextf(0, ++y * 8, 0xffff, 0x0000, "row %d", row);
            puttextf(0, ++y * 8, 0xffff, 0x0000, "column %d", column);
            puttextf(0, ++y * 8, 0xffff, 0x0000, "count %d", count);


            sleep_ms(2000);
            t0 = *pGetTime;
        }
#endif

        // Perform NTSC / PAL detection based on number of rows
        if (IN_TOLERANCE(row, ROWS_PAL, ROWS_TOLERANCE)) {
            crop_y = DEFAULT_CROP_Y_PAL;
        } else {
            // In case the mode can't be detected, default to NTSC as it crops fewer rows
            crop_y = DEFAULT_CROP_Y_NTSC;
        }

        frame++;
    }
    __builtin_unreachable();
}

