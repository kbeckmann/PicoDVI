/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */

#pragma once

#include "joybus.h"

// Key combination to enter the OSD menu (L + R + C-DOWN + DPAD-DOWN)
#define OSD_SHORTCUT(__keys__) ( \
    TL_BUTTON(__keys__) && \
    TR_BUTTON(__keys__) && \
    CD_BUTTON(__keys__) && \
    DD_BUTTON(__keys__)    \
)

typedef enum {
    OSD_DONE = 0,
    OSD_SKIP_NEXT_FRAME,
    OSD_AGAIN
} osd_ret_t;

osd_ret_t osd_run(void);
