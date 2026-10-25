#include "knob.h"

#include <stdatomic.h>

#include "board.h"
#include "driver/gpio.h"
#include "esp_lvgl_port.h"
#include "esp_rom_sys.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Each detent pulls one pin LOW for at least ~4 ms. Confirm the edge after 2 ms to reject glitches.
#define KNOB_DEBOUNCE_US   2000
#define KNOB_EVENT_COUNT   16

typedef struct {
    int gpio;
    int dir;
    int index;
    atomic_bool pressed;
} knob_pin_t;

static knob_pin_t s_pins[] = {
    { .gpio = BOARD_KNOB_LEFT, .dir = -1, .index = 0 },
    { .gpio = BOARD_KNOB_RIGHT, .dir = 1, .index = 1 },
};

typedef struct {
    int index;
    int level;
} knob_event_t;

static QueueHandle_t s_events;
static atomic_int s_pending;   // detents not yet consumed by LVGL
static atomic_int s_scroll;    // detents not yet sent as scroll

static void knob_isr(void *arg)
{
    const knob_pin_t *pin = arg;
    const knob_event_t event = { .index = pin->index, .level = gpio_get_level(pin->gpio) };
    BaseType_t higher_priority_task_woken = pdFALSE;
    xQueueSendFromISR(s_events, &event, &higher_priority_task_woken);
    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

static void knob_event_task(void *arg)
{
    knob_event_t event;
    while (1) {
        if (xQueueReceive(s_events, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        knob_pin_t *pin = &s_pins[event.index];
        if (event.level) {
            atomic_store(&pin->pressed, false);
            continue;
        }
        if (atomic_load(&pin->pressed)) {
            continue;
        }

        esp_rom_delay_us(KNOB_DEBOUNCE_US);
        if (gpio_get_level(pin->gpio) == 0) {
            atomic_store(&pin->pressed, true);
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

void knob_set_sleep_wakeup(bool enabled)
{
    for (size_t i = 0; i < sizeof(s_pins) / sizeof(s_pins[0]); i++) {
        knob_pin_t *pin = &s_pins[i];
        if (enabled) {
            ESP_ERROR_CHECK(gpio_wakeup_enable(pin->gpio, GPIO_INTR_LOW_LEVEL));
            continue;
        }

        ESP_ERROR_CHECK(gpio_wakeup_disable(pin->gpio));
        ESP_ERROR_CHECK(gpio_set_intr_type(pin->gpio, GPIO_INTR_ANYEDGE));
        atomic_store(&pin->pressed, gpio_get_level(pin->gpio) == 0);
    }
}

lv_indev_t *knob_init(void)
{
    const gpio_config_t io = {
        .pin_bit_mask = (1ULL << BOARD_KNOB_LEFT) | (1ULL << BOARD_KNOB_RIGHT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    s_events = xQueueCreate(KNOB_EVENT_COUNT, sizeof(knob_event_t));
    configASSERT(s_events != NULL);
    // Run before input_task so short (~4 ms) encoder pulses are checked before they end.
    configASSERT(xTaskCreate(knob_event_task, "knob", 2048, NULL, 6, NULL) == pdPASS);
    for (size_t i = 0; i < sizeof(s_pins) / sizeof(s_pins[0]); i++) {
        ESP_ERROR_CHECK(gpio_isr_handler_add(s_pins[i].gpio, knob_isr, &s_pins[i]));
    }
    ESP_ERROR_CHECK(gpio_set_intr_type(BOARD_KNOB_LEFT, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_set_intr_type(BOARD_KNOB_RIGHT, GPIO_INTR_ANYEDGE));

    lvgl_port_lock(0);
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev, knob_read);
    lvgl_port_unlock();
    return indev;
}
