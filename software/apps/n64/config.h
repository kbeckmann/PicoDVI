/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */

/**
 * @file config.h
 * @brief Configuration management functions and global configuration variable.
 */

#pragma once

#include <stdint.h>

/// Default crop X parameter for PAL.
#define DEFAULT_CROP_X_PAL  (36)

/// Default crop X parameter for NTSC.
#define DEFAULT_CROP_X_NTSC (14)

/// Default crop Y parameter for PAL.
#define DEFAULT_CROP_Y_PAL  (90)

/// Default crop Y parameter for NTSC.
#define DEFAULT_CROP_Y_NTSC (25)

/// Number of rows for PAL.
#define ROWS_PAL            (615)

/// Number of rows for NTSC.
#define ROWS_NTSC           (511)

/// Tolerance for number of rows.
#define ROWS_TOLERANCE      (5)

/// Y offset for OSD.
#define OSD_Y_OFFSET (3)

/// X offset for OSD.
#define OSD_X_OFFSET (5)

/// TMDS bit clock 252 MHz
/// DVDD 1.2V (1.1V seems ok too)
/// Voltage regulator selection.
#define VREG_VSEL VREG_VOLTAGE_1_20

/// DVI timing configuration.
#define DVI_TIMING dvi_timing_640x480p_60hz

/// UART config on the last GPIOs
/// UART transmission pin.
#define UART_TX_PIN (28)

/// UART reception pin (not available on the pico).
#define UART_RX_PIN (29)

/// UART identifier.
#define UART_ID     uart0

/// Baud rate for UART communication.
#define BAUD_RATE   115200

/**
 * @brief The number of bits to shift to get the audio buffer size.
 *
 * This value is used to calculate the size of the audio buffer by shifting
 * 1 to the left by this number of bits. For example, if
 * AUDIO_BUFFER_SIZE_BITS is 12, then the audio buffer size will be
 * 1 << 12, or 4096.
 */
#define AUDIO_BUFFER_SIZE_BITS 12

/**
 * @brief The size of the audio buffer.
 *
 * This is calculated as 1 << AUDIO_BUFFER_SIZE_BITS. So if
 * AUDIO_BUFFER_SIZE_BITS is 12, then AUDIO_BUFFER_SIZE will be 4096.
 */
#define AUDIO_BUFFER_SIZE (1 << AUDIO_BUFFER_SIZE_BITS)

/**
 * @def USE_RGB555
 * @brief A macro to control the color depth of the display.
 *
 * When defined, the display will use RGB555 color depth, which provides
 * 5 bits for each of the red, green, and blue color channels. This results
 * in a total of 32,768 possible colors.
 *
 * RGB555 can sometimes look better than RGB565 because it provides a balanced
 * color depth across all three color channels. This can result in more accurate
 * color reproduction, especially for images that have a balanced mix of colors.
 * However, it does provide fewer total colors than RGB565.
 *
 * If this macro is not defined, and USE_RGB565 is defined instead, the display
 * will use RGB565 color depth. This provides 5 bits for red, 6 bits for green,
 * and 5 bits for blue, resulting in a total of 65,536 possible colors. The
 * extra bit for the green channel is because the human eye is more sensitive
 * to variations in green than in red or blue.
 */
#define USE_RGB555
// #define USE_RGB565

/**
 * @def AUDIO_ENABLED
 * @brief A macro to control the audio input functionality.
 *
 * When set to 1, the audio input functionality is enabled. This means that the
 * application will process and output audio data. However, this will only work
 * if the necessary audio input wires have been correctly soldered to the
 * appropriate pins on the hardware.
 *
 * When set to 0, the audio input functionality is disabled. This can be useful
 * for debugging, or when running on hardware where the audio input wires have
 * not been soldered.
 */
#define AUDIO_ENABLED 1
// #define AUDIO_ENABLED 0

/**
 * @def DIAGNOSTICS
 * @brief A macro to control the display of diagnostic data.
 *
 * When defined, the application will print diagnostic data on the screen.
 * This can be useful for debugging and performance tuning.
 *
 * When not defined, no diagnostic data will be displayed.
 */
// #define DIAGNOSTICS

/**
 * @def DIAGNOSTICS_JOYBUS
 * @brief A macro to control the display of Joybus diagnostic data.
 *
 * When defined, the application will print Joybus-specific diagnostic data
 * on the screen. This can be useful for debugging and performance tuning
 * of the Joybus communication.
 *
 * When not defined, no Joybus diagnostic data will be displayed.
 */
// #define DIAGNOSTICS_JOYBUS

/**
 * @def CONFIG_MAGIC1
 * @brief A magic number used for configuration validation.
 *
 * This magic number is used as part of a mechanism to validate the
 * configuration data. It should be stored in a known location in the
 * configuration data, and checked when the configuration data is read.
 * If it does not match the expected value, this indicates that the
 * configuration data is not valid, and default values should be used instead.
 */
#define CONFIG_MAGIC1 0x12345678

/**
 * @def CONFIG_MAGIC2
 * @brief A second magic number used for configuration validation.
 *
 * This magic number is used as part of a mechanism to validate the
 * configuration data, in conjunction with CONFIG_MAGIC1. It provides an
 * additional check to ensure the integrity of the configuration data.
 */
#define CONFIG_MAGIC2 0xdeadf00d

/**
 * @enum dvi_color_mode
 * @brief Enumerates the supported color modes.
 */
typedef enum dvi_color_mode {
    DVI_RGB_555 = 0, ///< 15-bit color mode (5 bits each for red, green, and blue).
    DVI_RGB_565,     ///< 16-bit color mode (5 bits for red and blue, 6 bits for green).
    DVI_RGB_888,     ///< 24-bit color mode (8 bits each for red, green, and blue).
} dvi_color_mode_t;

/**
 * @enum sample_rate_hz
 * @brief Enumerates the supported output audio sample rates in Hertz.
 */
typedef enum sample_rate_hz {
    SAMPLE_RATE_32000_HZ = 32000, ///< 32,000 Hz sample rate.
    SAMPLE_RATE_44100_HZ = 44100, ///< 44,100 Hz sample rate.
    SAMPLE_RATE_48000_HZ = 48000, ///< 48,000 Hz sample rate.
    SAMPLE_RATE_96000_HZ = 96000, ///< 96,000 Hz sample rate.
} sample_rate_hz_t;

/**
 * @struct config
 * @brief Represents the configuration for the application.
 */
typedef struct config {
    uint32_t magic1;                ///< The first magic number used for configuration validation.
    uint32_t audio_out_sample_rate; ///< The audio output sample rate in Hertz (see @ref sample_rate_hz_t).
    uint32_t dvi_color_mode;        ///< The DVI color mode (see @ref dvi_color_mode_t).
    uint32_t magic2;                ///< The second magic number used for configuration validation.
} config_t;

/**
 * @brief Global configuration variable.
 *
 * This variable holds the current configuration for the application.
 * It can be initialized with default values using config_init(),
 * loaded from flash memory using config_load(), and saved to flash
 * memory using config_save().
 */
extern config_t g_config;

/**
 * @brief Initializes the global configuration with default values.
 *
 * This function sets the fields of g_config to their default values.
 * It should be called when the application starts, before any other
 * configuration management functions are used.
 */
void config_init(void);

/**
 * @brief Loads the configuration from flash memory into g_config.
 *
 * This function reads the configuration data from flash memory and
 * stores it in g_config. If the configuration data in flash memory
 * is not valid (as determined by the magic numbers), this function
 * will leave g_config unchanged.
 */
void config_load(void);

/**
 * @brief Saves the configuration from g_config to flash memory.
 *
 * This function writes the configuration data from g_config to flash
 * memory. This allows the current configuration to be preserved when
 * the application is restarted.
 */
void config_save(void);
