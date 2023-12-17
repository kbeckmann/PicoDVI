/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */

#pragma once

#include "joybus.h"

/**
 * @brief Key combination to enter the OSD menu.
 *
 * This macro checks if the specified key combination is pressed.
 * The key combination for the OSD menu is L + R + C-DOWN + DPAD-DOWN.
 *
 * @param __keys__ The current state of the keys.
 * @return True if the OSD shortcut keys are pressed, false otherwise.
 */
#define OSD_SHORTCUT(__keys__) ( \
    TL_BUTTON(__keys__) && \
    TR_BUTTON(__keys__) && \
    CD_BUTTON(__keys__) && \
    DD_BUTTON(__keys__)    \
)

/**
 * @brief Run the OSD.
 *
 * This function runs the OSD. It should be called in your main loop.
 */
void osd_run(void);
