/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */

#pragma once

#include <stdint.h>

// Default crop parameters
#define DEFAULT_CROP_X_PAL  (36)
#define DEFAULT_CROP_X_NTSC (14)
#define DEFAULT_CROP_Y_PAL  (90)
#define DEFAULT_CROP_Y_NTSC (25)
#define ROWS_PAL            (615)
#define ROWS_NTSC           (511)
#define ROWS_TOLERANCE      (5)

#define OSD_Y_OFFSET (3)
#define OSD_X_OFFSET (5)

// TMDS bit clock 252 MHz
// DVDD 1.2V (1.1V seems ok too)
#define VREG_VSEL VREG_VOLTAGE_1_20
#define DVI_TIMING dvi_timing_640x480p_60hz

// UART config on the last GPIOs
#define UART_TX_PIN (28)
#define UART_RX_PIN (29) /* not available on the pico */
#define UART_ID     uart0
#define BAUD_RATE   115200

// #define AUDIO_BUFFER_SIZE   (256 * 32)
#define AUDIO_BUFFER_SIZE_BITS 12
#define AUDIO_BUFFER_SIZE (1 << AUDIO_BUFFER_SIZE_BITS)

// RGB555 results in equal color depth for each color channel
#define USE_RGB555
// #define USE_RGB565

#define AUDIO_ENABLED 1
// #define AUDIO_ENABLED 0

// Uncomment to print diagnostic data on the screen
// #define DIAGNOSTICS
// #define DIAGNOSTICS_JOYBUS

#define CONFIG_MAGIC1 0x123456768
#define CONFIG_MAGIC2 0xdeadf000d

typedef enum dvi_color_mode {
    DVI_RGB_555 = 0,
    DVI_RGB_565,
    DVI_RGB_888,
} dvi_color_mode_t;

typedef struct config {
    uint32_t magic1;

    uint32_t audio_out_sample_rate;
    dvi_color_mode_t dvi_color_mode;

    uint32_t magic2;
} config_t;

extern config_t g_config;

// Initializes g_config with default values
void config_init(void);

// Loads configuration from flash into g_config
void config_load(void);

// Saves configuration from g_config to flash
void config_save(void);
