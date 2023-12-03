#include "joybus.h"

static struct {
    PIO pio_instance;
    uint32_t sm_instance;
    uint32_t last_value;
} state;

void joybus_rx_init(PIO pio_instance, uint sm_instance)
{
    state.pio_instance = pio_instance;
    state.sm_instance = sm_instance;
}

uint32_t joybus_rx_get_latest(void)
{
    static uint32_t last_value = 0;

    while (!pio_sm_is_rx_fifo_empty(state.pio_instance, state.sm_instance)) {
        last_value = pio_sm_get_blocking(state.pio_instance, state.sm_instance);
    }

    return last_value;
}
