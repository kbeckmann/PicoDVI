/**
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Konrad Beckmann
 */

#include "config.h"

#include <string.h>

#include "gfx.h"
#include "osd.h"
#include "joybus.h"

typedef enum item_type {
    ITEM_TYPE_TEXT = 0,
    ITEM_TYPE_VALUE_RW_U32,
    ITEM_TYPE_VALUE_RW_I32,
    ITEM_TYPE_VALUE_RO_U32,
    ITEM_TYPE_VALUE_RO_I32,
    ITEM_TYPE_MENU,
    ITEM_TYPE_BACK,
    ITEM_TYPE_EXIT,
} item_type_t;

typedef struct menu_item {
    char *text;
    item_type_t type;
    union {
        uint32_t *value_u32;
        int32_t *value_i32;
        void *value_ptr;
    } value;
} menu_item_t;

#define IS_FOCUSABLE(__x__) ( \
    ((__x__) == ITEM_TYPE_VALUE_RW_U32) || \
    ((__x__) == ITEM_TYPE_VALUE_RW_I32) || \
    ((__x__) == ITEM_TYPE_VALUE_RO_U32) || \
    ((__x__) == ITEM_TYPE_VALUE_RO_I32) || \
    ((__x__) == ITEM_TYPE_MENU) || \
    ((__x__) == ITEM_TYPE_BACK) || \
    ((__x__) == ITEM_TYPE_EXIT) \
)

menu_item_t menu_audio[] = {
    {
        .text = "OSD Audio Menu",
    },
    {
        .text = "Back",
        .type = ITEM_TYPE_BACK,
    },
    {
        .text = NULL,
    }
};

menu_item_t menu[] = {
    {
        .text = "PicoDVI-N64 by @kbeckmann",
    },
    {
        .text = "Git rev: <...>",
    },
    {
        .text = "Build date: 2023-xx-xx",
    },
    {
        .text = "",
    },
    {
        .text = "OSD Menu",
    },
    {
        .text = "Second",
    },
    {
        .text = "Third",
    },
    {
        .text = "Sub menu",
        .type = ITEM_TYPE_MENU,
        .value.value_ptr = menu_audio,
    },
    {
        .text = "Exit OSD",
        .type = ITEM_TYPE_EXIT,
    },
    {
        .text = NULL,
    }
};

static struct {
    uint32_t last_buttons;
    menu_item_t *root;
    menu_item_t *current_root;
    menu_item_t *focused_item;
    menu_item_t *stack[8];
    bool open;
} state = {
    .root = menu,
    .stack = {menu},
};

#define BUTTON_PRESSED(__op__) (!__op__(state.last_buttons) && __op__(buttons))


osd_ret_t osd_run(void)
{
    // Get Joybus state to decide if we should show the menu
    uint32_t buttons = joybus_rx_get_latest();
    if (OSD_SHORTCUT(buttons)) {
        state.open = true;
        
        // Reset menu state
        state.last_buttons = buttons;
        state.root = menu;
        state.current_root = menu;
        state.focused_item = NULL;
        memset(state.stack, 0, sizeof(state.stack));
    }

    if (!state.open) {
        return OSD_DONE;
    }

    bool rerender = false;

    // Show OSD menu
    menu_item_t *item = state.current_root;
    uint32_t y = OSD_Y_OFFSET;
    uint32_t x = OSD_X_OFFSET;

    if (state.focused_item == NULL) {
        // Find first focusable item
        menu_item_t *it = item;
        while (it->text) {
            if (IS_FOCUSABLE(it->type)) {
                state.focused_item = it;
                break;
            }
            it++;
        }
    }

    // Render
    while (item && item->text) {
        uint16_t bg_color = (item == state.focused_item) ? (RGB888_TO_RGB565(0xff, 0x00, 0xff)) : (RGB888_TO_RGB565(0x00, 0x00, 0x00));
        uint16_t fg_color = RGB888_TO_RGB565(0xff, 0xff, 0xff);

        gfx_puttextf(x, y++ * 8, bg_color, fg_color, "%s", item->text);

        item++;
    }

    // Handle Input
    buttons = joybus_rx_get_latest();
    if (BUTTON_PRESSED(DD_BUTTON)) {
        if ((state.focused_item + 1)->text != NULL) {
            state.focused_item++;
        }
    }
    else if (BUTTON_PRESSED(DU_BUTTON)) {
        if (state.focused_item != state.current_root) {
            state.focused_item--;
        }
    }
    else if (BUTTON_PRESSED(A_BUTTON)) {
        if (state.focused_item->type == ITEM_TYPE_MENU) {
            menu_item_t *previous_root = state.current_root;
            state.current_root = (menu_item_t *) state.focused_item->value.value_ptr;
            state.current_root->value.value_ptr = previous_root;
            state.focused_item = NULL;
            rerender = true;
        }
        else if (state.focused_item->type == ITEM_TYPE_BACK) {
            state.current_root = (menu_item_t *) state.current_root->value.value_ptr;
            state.focused_item = NULL;
            rerender = true;
        }
        else if (state.focused_item->type == ITEM_TYPE_EXIT) {
            state.open = false;
        }
    }

    state.last_buttons = buttons;
    
    if (rerender) {
        return OSD_SKIP_NEXT_FRAME;
    }

    return OSD_AGAIN;
}
