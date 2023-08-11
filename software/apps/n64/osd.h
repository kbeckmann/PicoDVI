#pragma once

#include "joybus.h"

// Key combination to enter the OSD menu (L + R + C-DOWN + DPAD-DOWN)
#define OSD_SHORTCUT(__keys__) ( \
    TL_BUTTON(__keys__) && \
    TR_BUTTON(__keys__) && \
    CD_BUTTON(__keys__) && \
    DD_BUTTON(__keys__)    \
)

void osd_run(void);
