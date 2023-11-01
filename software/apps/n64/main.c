// #pragma GCC optimize("Os")
// #pragma GCC optimize("O2")
#pragma GCC optimize("O3")


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"

#include "dvi.h"
#include "dvi_serialiser.h"
#include "common_dvi_pin_configs.h"
#include "sprite.h"

#include "joybus.h"

#include "joybus.pio.h"
#include "n64.pio.h"

#include "gfx.h"
#include "osd.h"

// Pinout reference
#define PIN_VIDEO_D0     0
#define PIN_VIDEO_D1     1
#define PIN_VIDEO_D2     2
#define PIN_VIDEO_D3     3
#define PIN_VIDEO_D4     4
#define PIN_VIDEO_D5     5
#define PIN_VIDEO_D6     6
#define PIN_VIDEO_DSYNC  7
#define PIN_VIDEO_CLK    8
#define PIN_AUDIO_LRCLK  9
#define PIN_AUDIO_SDAT  10
#define PIN_AUDIO_BCLK  11
#define PIN_JOYBUS_P1   20

// Crop configuration for PAL vs NTSC
#define IN_RANGE(__x, __low, __high) (((__x) >= (__low)) && ((__x) <= (__high)))
#define IN_TOLERANCE(__x, __value, __tolerance) IN_RANGE(__x, (__value - __tolerance), (__value + __tolerance))

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

const PIO pio_joybus = DVI_DEFAULT_SERIAL_CONFIG.pio; // usually pio0
const uint sm_joybus = 3; // last free sm in pio0 unless sm_tmds is set to something unusual

const PIO pio = (DVI_DEFAULT_SERIAL_CONFIG.pio == pio0) ? pio1 : pio0;
const uint sm_video = 0;
const uint sm_audio = 1;
struct dvi_inst dvi0;

// __no_inline_not_in_flash_func
// __time_critical_func
// __not_in_flash_func


//Audio Related
#if AUDIO_ENABLED
audio_sample_t      audio_buffer_a[AUDIO_BUFFER_SIZE];
// audio_sample_t      audio_buffer_b[AUDIO_BUFFER_SIZE];
#endif


void core1_main(void)
{
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    dvi_start(&dvi0);
    dvi_scanbuf_main_16bpp(&dvi0);
    __builtin_unreachable();
}

void core1_scanline_callback(uint)
{
    // Discard any scanline pointers passed back
    uint16_t *bufptr;
    while (queue_try_remove_u32(&dvi0.q_colour_free, &bufptr))
        ;
    // Note first two scanlines are pushed before DVI start
    static uint scanline = 2;
    bufptr = &g_framebuf[FRAME_WIDTH * scanline];
    queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);
    scanline = (scanline + 1) % FRAME_HEIGHT;
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

    gfx_init();
    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
    dvi0.scanline_callback = core1_scanline_callback;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    // Once we've given core 1 the g_framebuffer, it will just keep on displaying
    // it without any intervention from core 0

#ifdef DIAGNOSTICS
    // Fill with red
    sprite_fill16(g_framebuf, RGB888_TO_RGB565(0xFF, 0x00, 0x00), FRAME_WIDTH * FRAME_HEIGHT);
#else
    // Fill with black
    sprite_fill16(g_framebuf, RGB888_TO_RGB565(0x00, 0x00, 0x00), FRAME_WIDTH * FRAME_HEIGHT);
#endif

    uint16_t *bufptr = g_framebuf;
    queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);
    bufptr += FRAME_WIDTH;
    queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);


     // HDMI Audio related
    dvi_get_blank_settings(&dvi0)->top    = 4 * 0;
    dvi_get_blank_settings(&dvi0)->bottom = 4 * 0;
    // dvi_audio_sample_buffer_set(&dvi0, audio_buffer, AUDIO_BUFFER_SIZE);
    // void dvi_audio_sample_dma_set_chan(struct dvi_inst *inst, int chan_a, audio_sample_t *buf_a, int chan_b, audio_sample_t *buf_b, int size) {
    // dvi_set_audio_freq(&dvi0, 44100, 28000, 6272);
    // dvi_set_audio_freq(&dvi0, 48000, 25200, 6144);
    dvi_set_audio_freq(&dvi0, 32000, 25200, 4096);


    printf("Core 1 start\n");
    multicore_launch_core1(core1_main);

    printf("Start rendering\n");

    for (int i = PIN_VIDEO_D0; i <= PIN_AUDIO_BCLK; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_set_pulls(i, false, false);
    }

    gpio_init(PIN_JOYBUS_P1);
    gpio_set_dir(PIN_JOYBUS_P1, GPIO_IN);
    gpio_set_pulls(PIN_JOYBUS_P1, false, false);

    // Video
    uint offset = pio_add_program(pio, &n64_program);
    n64_video_program_init(pio, sm_video, offset);
    pio_sm_set_enabled(pio, sm_video, true);

    // Audio
    n64_audio_program_init(pio, sm_audio, offset);
    pio_sm_set_enabled(pio, sm_audio, true);

    // Joybus RX
    uint offset_joybus = pio_add_program(pio_joybus, &joybus_program);
    joybus_rx_program_init(pio_joybus, sm_joybus, offset_joybus, PIN_JOYBUS_P1);
    pio_sm_set_enabled(pio_joybus, sm_joybus, true);

    // TODO: Move all stuff into this function
    joybus_rx_init(pio_joybus, sm_joybus);

    // set_write_offset(&dvi0.audio_ring, 0);
    // set_read_offset(&dvi0.audio_ring, (AUDIO_BUFFER_SIZE) / 2);
    // set_read_offset(&dvi0.audio_ring, 0);

#if 0
    // Audio only test
    while (true) {
        // uint32_t sample = pio_sm_get_blocking(pio, sm_audio) >> 16;
        // uint32_t sample = (pio_sm_get_blocking(pio, sm_audio) >> 16) & 0xffff;
        // uint32_t sample = (pio_sm_get_blocking(pio, sm_audio)) & 0xffff;
        uint32_t sample = pio_sm_get_blocking(pio, sm_audio);
        // sample = ((sample << 8) | (sample >> 8));
        // audio_sample_u_t *audio_ptr = get_write_pointer(&dvi0.audio_ring);
        // audio_ptr->channels[0] = sample;
        // audio_ptr->channels[1] = 0;

        uint32_t *audio_ptr = (uint32_t *) get_write_pointer(&dvi0.audio_ring);
        *audio_ptr = sample;

        increase_write_pointer(&dvi0.audio_ring, 1);

        // Just copy the sample
        // audio_ptr = get_write_pointer(&dvi0.audio_ring);
        // *audio_ptr = sample;

        // increase_write_pointer(&dvi0.audio_ring, 1);


    }

#elif 1


#if 0
    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        audio_buffer_a[i].channels[0] = rand();
        audio_buffer_a[i].channels[1] = rand();

        audio_buffer_b[i].channels[0] = rand();
        audio_buffer_b[i].channels[1] = rand();
    }
#endif

    // DMA
    uint dma_chan_a = dma_claim_unused_channel(true);
    // uint dma_chan_b = dma_claim_unused_channel(true);
    // uint dma_chan_b = dma_chan_a;

    // Chan A
    dma_channel_config config_a = dma_channel_get_default_config(dma_chan_a);
    channel_config_set_read_increment(&config_a, false);
    channel_config_set_write_increment(&config_a, true);
    channel_config_set_transfer_data_size(&config_a, DMA_SIZE_32);
    // channel_config_set_chain_to(&config_a, dma_chan_b);
    channel_config_set_irq_quiet(&config_a, true);
    channel_config_set_dreq(&config_a, pio_get_dreq(pio, sm_audio, false));

#if AUDIO_ENABLED
    dma_channel_configure(dma_chan_a, &config_a,
        audio_buffer_a,        // Destination pointer
        &pio->rxf[sm_audio],      // Source pointer
        AUDIO_BUFFER_SIZE, // Number of transfers
        false                // not Start immediately
    );
#endif

    // Chan B
    // dma_channel_config config_b = dma_channel_get_default_config(dma_chan_b);
    // channel_config_set_read_increment(&config_b, false);
    // channel_config_set_write_increment(&config_b, true);
    // channel_config_set_transfer_data_size(&config_b, DMA_SIZE_32);
    // // channel_config_set_chain_to(&config_b, dma_chan_a);
    // channel_config_set_irq_quiet(&config_b, true);
    // channel_config_set_dreq(&config_b, pio_get_dreq(pio, sm_audio, false));

    // dma_channel_configure(dma_chan_b, &config_b,
    //     audio_buffer_b,        // Destination pointer
    //     &pio->rxf[sm_audio],      // Source pointer
    //     AUDIO_BUFFER_SIZE, // Number of transfers
    //     false                // Do not start immediately
    // );

#if AUDIO_ENABLED
    // dvi_audio_sample_dma_set_chan(&dvi0, dma_chan_a, audio_buffer_a, dma_chan_b, audio_buffer_b, AUDIO_BUFFER_SIZE);
    dvi_audio_sample_dma_set_chan(&dvi0, dma_chan_a, audio_buffer_a, 0, 0, AUDIO_BUFFER_SIZE);
    // dvi_audio_sample_buffer_set(&dvi0, audio_buffer_a, AUDIO_BUFFER_SIZE);
#endif


#endif

#ifdef DIAGNOSTICS_JOYBUS

    uint32_t transfer = 0;
    uint32_t y = 0;

    gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "hello");
    gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "offset_joybus = %d", offset_joybus);

    while (true) {

        // The following code prints the raw PIO data
#if 0
        uint32_t value[8];
        for (int i = 0; i < 4; i++) {
            value[i] = pio_sm_get_blocking(pio_joybus, sm_joybus);
        }

        transfer++;

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: %08X %08X %08X %08X",
            transfer % 100, 
            value[0],
            value[1], value[2], value[3]);

        if (y > 24) {
            y = 0;
            sleep_ms(2000);
        }

#else
        // Use helper functions to decode the last controller state
        uint32_t value = joybus_rx_get_latest();

        y = 0;
        transfer++;

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: A=%d B=%d Z=%d Start=%d",
            transfer, 
            !!A_BUTTON(value), 
            !!B_BUTTON(value), 
            !!Z_BUTTON(value), 
            !!START_BUTTON(value));

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: DU=%d DD=%d DL=%d DR=%d",
            transfer, 
            !!DU_BUTTON(value), 
            !!DD_BUTTON(value), 
            !!DL_BUTTON(value), 
            !!DR_BUTTON(value));

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: Reset=%d",
            transfer, 
            !!RESET_BUTTON(value));

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: TL=%d TR=%d",
            transfer, 
            !!TL_BUTTON(value), 
            !!TR_BUTTON(value));

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: CU=%d CD=%d CL=%d CR=%d",
            transfer, 
            !!CU_BUTTON(value), 
            !!CD_BUTTON(value), 
            !!CL_BUTTON(value), 
            !!CR_BUTTON(value));

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: X=%04d Y=%04d",
            transfer, 
            X_STICK(value), 
            Y_STICK(value));
#endif

    }


#endif


#if 1
    // Video

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
    uint32_t crop_x = DEFAULT_CROP_X_PAL;
    uint32_t crop_y = DEFAULT_CROP_Y_PAL;
#ifdef DIAGNOSTICS
    const volatile uint32_t *pGetTime = &timer_hw->timerawl;
    uint32_t t0 = 0;
    uint32_t t1 = 0;
#endif

    while (1) {
        // printf("START\n");

        // Let the OSD code run
        osd_run();

        // 1. Find posedge VSYNC
        do {
            BGRS = pio_sm_get_blocking(pio, sm_video);
        } while (!(BGRS & VSYNCB_MASK));

        // printf("VSYNC\n");

        int active_row = 0;
        for (row = 0; ; row++) {

            int skip_row = (
                (row % 2 != 0) ||            // Skip every second line (TODO: Add blend option later)
                (row < crop_y) ||            // crop_y, number of rows to skip vertically from the top
                (active_row >= FRAME_HEIGHT) // Never attempt to write more rows than the g_framebuffer
            );

            // 2. Find posedge HSYNC
            do {
                BGRS = pio_sm_get_blocking(pio, sm_video);

                if ((BGRS & VSYNCB_MASK) == 0) {
                    // VSYNC found, time to quit
                    goto end_of_line;
                }

            } while ((BGRS & ACTIVE_PIXEL_MASK) != ACTIVE_PIXEL_MASK);

            if (skip_row) {
                // Skip rows based on logic above
                do {
                    BGRS = pio_sm_get_blocking(pio, sm_video);

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
            for (int left_ctr = 0; left_ctr < crop_x; left_ctr++) {
                BGRS = pio_sm_get_blocking(pio, sm_video);
            };

            // 3.2 Capture active pixels
            BGRS = pio_sm_get_blocking(pio, sm_video);
            do {
                // 3.3 Convert to RGB565 or 555
                g_framebuf[count++] = (
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
                        BGRS = pio_sm_get_blocking(pio, sm_video);
                    } while ((BGRS & ACTIVE_PIXEL_MASK) == ACTIVE_PIXEL_MASK);
                    break;
                }

                // 3.4 Skip every second pixel
                BGRS = pio_sm_get_blocking(pio, sm_video);
                column++;

                // Skip one extra pixel, for debugging
                // BGRS = pio_sm_get_blocking(pio, sm_video);
                column++;

                // Fetch new pixel in the end, so the loop logic can react to it first
                BGRS = pio_sm_get_blocking(pio, sm_video);

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

            gfx_puttextf(0, ++y * 8, 0xffff, 0x0000, "Delta %d", (t1 - t0));
            gfx_puttextf(0, ++y * 8, 0xffff, 0x0000, "row %d", row);
            gfx_puttextf(0, ++y * 8, 0xffff, 0x0000, "column %d", column);
            gfx_puttextf(0, ++y * 8, 0xffff, 0x0000, "count %d", count);


            sleep_ms(2000);
            t0 = *pGetTime;
        }
#endif

        // Perform NTSC / PAL detection based on number of rows
        if (IN_TOLERANCE(row, ROWS_PAL, ROWS_TOLERANCE)) {
            crop_x = DEFAULT_CROP_X_PAL;
            crop_y = DEFAULT_CROP_Y_PAL;
        } else {
            // In case the mode can't be detected, default to NTSC as it crops fewer rows
            crop_x = DEFAULT_CROP_X_NTSC;
            crop_y = DEFAULT_CROP_Y_NTSC;
        }

        // Trigger DMA
#if AUDIO_ENABLED
        if (!dma_channel_is_busy(dma_chan_a)) {
            dma_channel_configure(dma_chan_a, &config_a,
                audio_buffer_a,        // Destination pointer
                &pio->rxf[sm_audio],      // Source pointer
                AUDIO_BUFFER_SIZE, // Number of transfers
                true                // not Start immediately
            );
        }
#endif


#ifdef DIAGNOSTICS_JOYBUS
    {
        // Use helper functions to decode the last controller state
        uint32_t value = joybus_rx_get_latest();

        uint32_t y = 20;

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: A=%d B=%d Z=%d Start=%d",
            frame, 
            !!A_BUTTON(value), 
            !!B_BUTTON(value), 
            !!Z_BUTTON(value), 
            !!START_BUTTON(value));

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: DU=%d DD=%d DL=%d DR=%d",
            frame, 
            !!DU_BUTTON(value), 
            !!DD_BUTTON(value), 
            !!DL_BUTTON(value), 
            !!DR_BUTTON(value));

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: Reset=%d",
            frame, 
            !!RESET_BUTTON(value));

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: TL=%d TR=%d",
            frame, 
            !!TL_BUTTON(value), 
            !!TR_BUTTON(value));

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: CU=%d CD=%d CL=%d CR=%d",
            frame, 
            !!CU_BUTTON(value), 
            !!CD_BUTTON(value), 
            !!CL_BUTTON(value), 
            !!CR_BUTTON(value));

        gfx_puttextf(0, y++ * 8, 0xffff, 0x0000, "%02d: X=%04d Y=%04d",
            frame, 
            X_STICK(value), 
            Y_STICK(value));
    }
#endif


        frame++;
    }

#endif

    __builtin_unreachable();
}

