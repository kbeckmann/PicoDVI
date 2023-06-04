#pragma once

#include <stdint.h>
#include "hardware/pio.h"

// Pad buttons
#define A_BUTTON(a)     ((a) & 0x80000000)
#define B_BUTTON(a)     ((a) & 0x40000000)
#define Z_BUTTON(a)     ((a) & 0x20000000)
#define START_BUTTON(a) ((a) & 0x10000000)

// D-Pad
#define DU_BUTTON(a)    ((a) & 0x08000000)
#define DD_BUTTON(a)    ((a) & 0x04000000)
#define DL_BUTTON(a)    ((a) & 0x02000000)
#define DR_BUTTON(a)    ((a) & 0x01000000)

#define RESET_BUTTON(a) ((a) & 0x00800000)
// 0x00400000 is usnused

// Triggers
#define TL_BUTTON(a)    ((a) & 0x00200000)
#define TR_BUTTON(a)    ((a) & 0x00100000)

// Yellow C buttons
#define CU_BUTTON(a)    ((a) & 0x00080000)
#define CD_BUTTON(a)    ((a) & 0x00040000)
#define CL_BUTTON(a)    ((a) & 0x00020000)
#define CR_BUTTON(a)    ((a) & 0x00010000)

#define X_STICK(a)      ((int8_t) (((a) & 0x0000FF00) >> 8) )
#define Y_STICK(a)      ((int8_t) (((a) & 0x000000FF)     ) )

void joybus_rx_init(PIO pio_instance, uint sm_instance);
uint32_t joybus_rx_get_latest(void);
