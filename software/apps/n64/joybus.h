/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */

#pragma once

#include <stdint.h>
#include "hardware/pio.h"

/**
 * @brief Check if the A button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the A button is pressed, zero otherwise.
 */
#define A_BUTTON(a)     ((a) & 0x80000000)

/**
 * @brief Check if the B button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the B button is pressed, zero otherwise.
 */
#define B_BUTTON(a)     ((a) & 0x40000000)

/**
 * @brief Check if the Z button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the Z button is pressed, zero otherwise.
 */
#define Z_BUTTON(a)     ((a) & 0x20000000)

/**
 * @brief Check if the Start button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the Start button is pressed, zero otherwise.
 */
#define START_BUTTON(a) ((a) & 0x10000000)

/**
 * @brief Check if the D-Pad Up button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the D-Pad Up button is pressed, zero otherwise.
 */
#define DU_BUTTON(a)    ((a) & 0x08000000)

/**
 * @brief Check if the D-Pad Down button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the D-Pad Down button is pressed, zero otherwise.
 */
#define DD_BUTTON(a)    ((a) & 0x04000000)

/**
 * @brief Check if the D-Pad Left button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the D-Pad Left button is pressed, zero otherwise.
 */
#define DL_BUTTON(a)    ((a) & 0x02000000)

/**
 * @brief Check if the D-Pad Right button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the D-Pad Right button is pressed, zero otherwise.
 */
#define DR_BUTTON(a)    ((a) & 0x01000000)

/**
 * @brief Check if the Reset button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the Reset button is pressed, zero otherwise.
 */
#define RESET_BUTTON(a) ((a) & 0x00800000)

// 0x00400000 is unused

/**
 * @brief Check if the Left Trigger button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the Left Trigger button is pressed, zero otherwise.
 */
#define TL_BUTTON(a)    ((a) & 0x00200000)

/**
 * @brief Check if the Right Trigger button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the Right Trigger button is pressed, zero otherwise.
 */
#define TR_BUTTON(a)    ((a) & 0x00100000)

/**
 * @brief Check if the C-Up button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the C-Up button is pressed, zero otherwise.
 */
#define CU_BUTTON(a)    ((a) & 0x00080000)

/**
 * @brief Check if the C-Down button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the C-Down button is pressed, zero otherwise.
 */
#define CD_BUTTON(a)    ((a) & 0x00040000)

/**
 * @brief Check if the C-Left button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the C-Left button is pressed, zero otherwise.
 */
#define CL_BUTTON(a)    ((a) & 0x00020000)

/**
 * @brief Check if the C-Right button is pressed.
 * @param a The current state of the buttons.
 * @return Non-zero if the C-Right button is pressed, zero otherwise.
 */
#define CR_BUTTON(a)    ((a) & 0x00010000)

/**
 * @brief Get the X position of the joystick.
 * @param a The current state of the joystick.
 * @return The X position of the joystick as a signed 8-bit integer.
 */
#define X_STICK(a)      ((int8_t) (((a) & 0x0000FF00) >> 8) )

/**
 * @brief Get the Y position of the joystick.
 * @param a The current state of the joystick.
 * @return The Y position of the joystick as a signed 8-bit integer.
 */
#define Y_STICK(a)      ((int8_t) (((a) & 0x000000FF)     ) )

/**
 * @brief Initialize the Joybus receiver.
 * @param pio_instance The PIO instance to use for the receiver.
 * @param sm_instance The state machine instance to use for the receiver.
 */
void joybus_rx_init(PIO pio_instance, uint sm_instance);

/**
 * @brief Get the latest data from the Joybus receiver.
 * @return The latest data from the Joybus receiver as a 32-bit integer.
 */
uint32_t joybus_rx_get_latest(void);
