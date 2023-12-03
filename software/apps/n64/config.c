/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */

#include "config.h"
#include <string.h>

config_t g_config;

static config_t default_config = {
    .magic1 = CONFIG_MAGIC1,

    .audio_out_sample_rate = 96000,
    .dvi_color_mode = DVI_RGB_555,

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
