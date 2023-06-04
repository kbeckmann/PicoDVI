#include "config.h"
#include <string.h>

config_t g_config;

static config_t default_config = {
    .audio_out_sample_rate = 96000,
    .dvi_color_mode = DVI_RGB_555,
};

void config_init(void)
{
    memcpy(&g_config, &default_config, sizeof(config_t));
}

void config_load(void)
{
    // TODO
}

// Saves configuration from g_config to flash
void config_save(void)
{
    // TODO
}
