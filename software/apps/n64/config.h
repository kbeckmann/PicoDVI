#pragma once

// Default crop parameters
#define DEFAULT_CROP_X_PAL  (36)
#define DEFAULT_CROP_X_NTSC (14)
#define DEFAULT_CROP_Y_PAL  (90)
#define DEFAULT_CROP_Y_NTSC (25)
#define ROWS_PAL            (615)
#define ROWS_NTSC           (511)
#define ROWS_TOLERANCE      (5)

// TMDS bit clock 252 MHz
// DVDD 1.2V (1.1V seems ok too)
#define VREG_VSEL VREG_VOLTAGE_1_20
#define DVI_TIMING dvi_timing_640x480p_60hz

// UART config on the last GPIOs
#define UART_TX_PIN (28)
#define UART_RX_PIN (29) /* not available on the pico */
#define UART_ID     uart0
#define BAUD_RATE   115200

#define AUDIO_BUFFER_SIZE   (256 * 32)

// RGB555 results in equal color depth for each color channel
#define USE_RGB555
// #define USE_RGB565

// Uncomment to print diagnostic data on the screen
// #define DIAGNOSTICS
// #define DIAGNOSTICS_JOYBUS

