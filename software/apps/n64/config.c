/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */

#include "config.h"
#include <string.h>

config_t g_config;

// Allow for compile-time configuration of default sample rate
#ifndef CONFIG_DEFAULT_SAMPLE_RATE_HZ
#define CONFIG_DEFAULT_SAMPLE_RATE_HZ SAMPLE_RATE_96000_HZ
#endif

// Allow for compile-time configuration of default color depth
#ifndef CONFIG_DEFAULT_COLOR_DEPTH
#define CONFIG_DEFAULT_COLOR_DEPTH DVI_RGB_555
#endif

static config_t default_config = {
    .magic1 = CONFIG_MAGIC1,

    .audio_out_sample_rate = CONFIG_DEFAULT_SAMPLE_RATE_HZ,
    .dvi_color_mode = CONFIG_DEFAULT_COLOR_DEPTH,

    .magic2 = CONFIG_MAGIC2,
};

void config_init(void)
{
    memcpy(&g_config, &default_config, sizeof(config_t));
}

void config_load(void)
{
    // TODO
}

void config_save(void)
{
    // TODO
}
