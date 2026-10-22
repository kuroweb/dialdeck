#include "knob.h"

#include <stdatomic.h>

#include "board.h"
#include "driver/gpio.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"

// Each detent pulls one pin LOW for at least ~4 ms. Edge interrupts also catch sub-ms glitches
// on the other pin, so sample instead and count a pulse only once it has stayed LOW this long.
#define KNOB_POLL_US       1000
#define KNOB_STABLE_POLLS  2

typedef struct {
    int gpio;
    int dir;
    int low_polls;
    bool pressed;
} knob_pin_t;

static knob_pin_t s_pins[] = {
    { .gpio = BOARD_KNOB_LEFT, .dir = -1 },
    { .gpio = BOARD_KNOB_RIGHT, .dir = 1 },
};

static atomic_int s_pending;   // detents not yet consumed by LVGL
static atomic_int s_scroll;    // detents not yet sent as scroll

static void knob_poll(void *arg)
{
    for (size_t i = 0; i < sizeof(s_pins) / sizeof(s_pins[0]); i++) {
        knob_pin_t *pin = &s_pins[i];
        if (gpio_get_level(pin->gpio)) {
            pin->low_polls = 0;
            pin->pressed = false;
            continue;
        }
        if (!pin->pressed && ++pin->low_polls >= KNOB_STABLE_POLLS) {
            pin->pressed = true;
            atomic_fetch_add(&s_pending, pin->dir);
            atomic_fetch_add(&s_scroll, pin->dir);
        }
    }
}

static void knob_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    data->enc_diff = atomic_exchange(&s_pending, 0);
    data->state = LV_INDEV_STATE_RELEASED;
}

int knob_take_scroll(void)
{
    return atomic_exchange(&s_scroll, 0);
}

lv_indev_t *knob_init(void)
{
    const gpio_config_t io = {
        .pin_bit_mask = (1ULL << BOARD_KNOB_LEFT) | (1ULL << BOARD_KNOB_RIGHT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    const esp_timer_create_args_t timer_args = {
        .callback = knob_poll,
        .name = "knob",
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, KNOB_POLL_US));

    lvgl_port_lock(0);
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev, knob_read);
    lvgl_port_unlock();
    return indev;
}
