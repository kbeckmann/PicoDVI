/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */

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

// Enable to print debug/diagnostics
// #define DIAGNOSTICS
// #define DIAGNOSTICS_JOYBUS

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

audio_sample_t      last_audio_sample;
audio_sample_t      audio_buffer[AUDIO_BUFFER_SIZE];

static void core1_main(void)
{
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    dvi_start(&dvi0);
    dvi_scanbuf_main_16bpp(&dvi0);
    __builtin_unreachable();
}

static void core1_scanline_callback(uint)
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

static void set_audio_dvi_parameters(sample_rate_hz_t samplerate, bool setup)
{
    uint32_t cts;
    uint32_t n;

    switch (samplerate) {
    case SAMPLE_RATE_96000_HZ:
        cts = 25200;
        n = 6144 * 2;
        break;
    case SAMPLE_RATE_48000_HZ:
        cts = 25200;
        n = 6144;
        break;
    case SAMPLE_RATE_44100_HZ:
        cts = 28000;
        n = 6272;
        break;
    case SAMPLE_RATE_32000_HZ:
        cts = 25200;
        n = 4096;
        break;
    default:
        // Assume a freq. close to 32000, so let's use that
        cts = 25200;
        n = (128 * samplerate * cts) / 25200000;
        break;
    }

    if (setup) {
        dvi_set_audio_freq(&dvi0, samplerate, cts, n);
    } else {
        dvi_update_audio_freq(&dvi0, samplerate, cts, n);
    }
}

static void set_audio_sampling_parameters(sample_rate_hz_t samplerate)
{
    uint16_t numerator;
    uint16_t denominator;

    switch (samplerate) {
    case SAMPLE_RATE_96000_HZ:
        numerator = 1;
        denominator = 2625; // No error
        break;
    case SAMPLE_RATE_48000_HZ:
        numerator = 1;
        denominator = 5250; // No error
        break;
    case SAMPLE_RATE_44100_HZ:
        numerator = 4;
        denominator = 22857; // Actual freq. 44100.28 Hz
        break;
    case SAMPLE_RATE_32000_HZ:
        numerator = 1;
        denominator = 7875; // No error
        break;
    default:
        numerator = 1;
        denominator = 252000000 / samplerate; // There might be rounding errors
        break;
    }

    dma_timer_set_fraction(0, numerator, denominator);
}

int main(void)
{
    config_init();
    config_load();

    vreg_set_voltage(VREG_VSEL);
    sleep_ms(10);

    // Run system at TMDS bit clock (252.000 MHz)
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

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

    set_audio_dvi_parameters(g_config.audio_out_sample_rate, true);

    printf("Core 1 start\n");
    multicore_launch_core1(core1_main);

    printf("Start rendering\n");

    for (int i = PIN_VIDEO_D0; i <= PIN_AUDIO_BCLK; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);

        // Enable weak internal pull downs to reduce noise when n64 is turned off
        gpio_set_pulls(i, false, true);
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


    // Audio test data generators
#if 0
    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        audio_buffer[i].channels[0] = rand();
        audio_buffer[i].channels[1] = rand();

        // audio_buffer_b[i].channels[0] = rand();
        // audio_buffer_b[i].channels[1] = rand();
    }
#elif 0
    // Generate sine
    // #define SAMPLE_RATE 96000
    #define SAMPLE_RATE 48000
    #define FREQUENCY1 375
    #define FREQUENCY2 (375 * 1.5)
    #define M_PI 3.1415926535

    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        double time = (double)i / SAMPLE_RATE;
        int16_t sample1 = (int16_t)(16384.0 * sin(2.0 * M_PI * FREQUENCY1 * time));
        int16_t sample2 = (int16_t)(16384.0 * sin(2.0 * M_PI * FREQUENCY2 * time));

        audio_buffer[i].channels[0] = sample1;
        audio_buffer[i].channels[1] = sample2;
    }
#endif

    // DMA for Audio

    // Set up data + ctrl loop DMA jobs that read
    // from the audio PIO and write to a single 32bit word.
    // This is done, so we have a location in RAM where we always
    // have the _latest_ valid audio sample. This is needed because the FIFO filling speed.
    // A workaround is to push multiple times in the PIO, but that's ugly and breaks for low sample rates.
    uint dma_ch_audio_pio_data = dma_claim_unused_channel(true);
    uint dma_ch_audio_pio_ctrl = dma_claim_unused_channel(true);
    printf("dma_ch_audio_pio_data=%d\n", dma_ch_audio_pio_data);
    printf("dma_ch_audio_pio_ctrl=%d\n", dma_ch_audio_pio_ctrl);

    dma_channel_config c_audio_pio_data = dma_channel_get_default_config(dma_ch_audio_pio_data);
    channel_config_set_read_increment(&c_audio_pio_data, false);
    channel_config_set_dreq(&c_audio_pio_data, pio_get_dreq(pio, sm_audio, false));
    channel_config_set_chain_to(&c_audio_pio_data, dma_ch_audio_pio_ctrl);
    channel_config_set_irq_quiet(&c_audio_pio_data, true);

    const volatile void* ptr_audio_pio_rxf = &pio->rxf[sm_audio];
    dma_channel_configure(dma_ch_audio_pio_data, &c_audio_pio_data,
        &last_audio_sample,   // Write to last_audio_sample
        ptr_audio_pio_rxf,    // Read from RX FIFO
        1,                    // Sample one full buffer
        false                 // Do not start immediately
    );

    // Set up the control DMA to make dma_ch_audio_pio_data loop
    dma_channel_config c_audio_pio_ctrl = dma_channel_get_default_config(dma_ch_audio_pio_ctrl);
    channel_config_set_read_increment(&c_audio_pio_ctrl, false);
    channel_config_set_irq_quiet(&c_audio_pio_ctrl, true);
    dma_channel_configure(dma_ch_audio_pio_ctrl, &c_audio_pio_ctrl,
        &dma_hw->ch[dma_ch_audio_pio_data].al3_read_addr_trig, // Write to data DMA's read-address trigger register
        &ptr_audio_pio_rxf, // Read from ptr to RX FIFO
        1,                  // A single transfer is needed to restart data DMA
        true                // Start immediately
    );

    // Now there is a dma job running that reads the Audio PIO rx fifo, and puts it in last_audio_sample.
    // Set up data + ctrl loop DMA jobs that reads continuously with 96kHz from last_audio_sample
    // and write the result in a ringbuffer, audio_buffer.
    uint dma_ch_audio_buffer_data = dma_claim_unused_channel(true);
    uint dma_ch_audio_buffer_ctrl = dma_claim_unused_channel(true);
    printf("dma_ch_audio_buffer_data=%d\n", dma_ch_audio_buffer_data);
    printf("dma_ch_audio_buffer_ctrl=%d\n", dma_ch_audio_buffer_ctrl);

    // Chan A
    dma_channel_config c_audio_buffer_data = dma_channel_get_default_config(dma_ch_audio_buffer_data);
    channel_config_set_read_increment(&c_audio_buffer_data, false); // Read from the same place
    channel_config_set_write_increment(&c_audio_buffer_data, true); // Write the whole buffer
    channel_config_set_transfer_data_size(&c_audio_buffer_data, DMA_SIZE_32);
    channel_config_set_chain_to(&c_audio_buffer_data, dma_ch_audio_buffer_ctrl);
    channel_config_set_irq_quiet(&c_audio_buffer_data, true);

    // Configure the DMA timer that pulls the latest sample at a constant frequency
    dma_timer_claim(0);

    set_audio_sampling_parameters(g_config.audio_out_sample_rate);

    channel_config_set_dreq(&c_audio_buffer_data, DREQ_DMA_TIMER0);

    volatile void* ptr = &last_audio_sample;
    volatile void* ptr2 = &audio_buffer[0];

    dma_channel_configure(dma_ch_audio_buffer_data, &c_audio_buffer_data,
        ptr2,               // Destination pointer
        ptr,                // Source pointer
        AUDIO_BUFFER_SIZE,  // Sample the whole buffer
        false               // Do not start immediately
    );

    // The control DMA triggers the data DMA continuously via ping-pong chain
    dma_channel_config c_audio_buffer_ctrl = dma_channel_get_default_config(dma_ch_audio_buffer_ctrl);
    channel_config_set_read_increment(&c_audio_buffer_ctrl, false); // Always read one word
    channel_config_set_write_increment(&c_audio_buffer_ctrl, false); // Always write to data DMA's read-address trigger register
    channel_config_set_irq_quiet(&c_audio_buffer_ctrl, true);
    dma_channel_configure(dma_ch_audio_buffer_ctrl, &c_audio_buffer_ctrl,
        &dma_hw->ch[dma_ch_audio_buffer_data].al2_write_addr_trig,  // Destination pointer is data DMA's read-address trigger register
        &ptr2,               // Source pointer is the address of RX FIFO
        1,                   // A single transfer is needed to restart data DMA
        true                 // Start immediately
    );

    // Write to the beginning of the buffer, read from the middle
    set_read_offset(&dvi0.audio_ring, (AUDIO_BUFFER_SIZE) / 2);

    // Let the dvi code know which dma channel we use so it can query the write pointer
    dvi_audio_sample_dma_set_chan(&dvi0, dma_ch_audio_buffer_data, audio_buffer, 0, 0, AUDIO_BUFFER_SIZE);

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

            // This code is duplucated for performance reasons - 555 and 565 respectively

            if (g_config.dvi_color_mode == DVI_RGB_555) {
                do {
                    // 3.3 Convert to RGB555
                    g_framebuf[count++] = (
                        ((BGRS <<  1) & 0xf800) |
                        ((BGRS >> 12) & 0x07e0) |
                        ((BGRS >> 26) & 0x001f)
                        // | 0x1f // Uncomment to tint everything with blue
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

                    // 3.5 Count number of pixels processed on this row
                    column += 2; // Fuse two increments into one instruction

                    // Fetch new pixel in the end, so the loop logic can react to it first
                    BGRS = pio_sm_get_blocking(pio, sm_video);
                } while (1);
            } else if (g_config.dvi_color_mode == DVI_RGB_565) {
                do {
                    // 3.3 Convert to RGB565
                    g_framebuf[count++] = (
                        ((BGRS <<  1) & 0xf800) |
                        ((BGRS >> 12) & 0x07c0) | // Mask so only 5 bits for green are used
                        ((BGRS >> 26) & 0x001f)
                        // | 0x1f // Uncomment to tint everything with blue
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

                    // 3.5 Count number of pixels processed on this row
                    column += 2; // Fuse two increments into one instruction

                    // Fetch new pixel in the end, so the loop logic can react to it first
                    BGRS = pio_sm_get_blocking(pio, sm_video);
                } while (1);
            } else {
                // Panic
            }
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

    __builtin_unreachable();
}
