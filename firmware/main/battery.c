#include "battery.h"

#include <stdatomic.h>

#include "board.h"
#include "driver/usb_serial_jtag.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "battery";

// The charger's ripple reaches the ADC unfiltered, so each reading averages many samples and the
// readings are smoothed again over time.
#define SAMPLES_PER_READING  64
#define READING_PERIOD_MS    1000
#define SMOOTHING            0.2f   // weight of a new reading in the moving average

// A Li-ion cell never reads above ~4.2 V on its own; on USB the pin reads the charger output,
// measured at ~4.45 V. Above this the device is on external power even without a USB host
// (a plain charger).
#define EXTERNAL_POWER_MV    4300

// Open-circuit voltage to charge for a single Li-ion cell under the light load of this device.
// On battery right after a full charge the device measured 4.16 V.
static const struct {
    int mv;
    int percent;
} s_curve[] = {
    { 4150, 100 }, { 4050, 90 }, { 3970, 80 }, { 3900, 70 }, { 3840, 60 }, { 3790, 50 },
    { 3750, 40 }, { 3710, 30 }, { 3680, 20 }, { 3620, 10 }, { 3500, 5 }, { 3300, 0 },
};
#define CURVE_LEN  (sizeof(s_curve) / sizeof(s_curve[0]))

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static adc_channel_t s_channel;
static atomic_bool s_charging;
static atomic_int s_percent = -1;

static int read_pin_mv(void)
{
    int sum = 0;
    for (int i = 0; i < SAMPLES_PER_READING; i++) {
        int raw = 0, mv = 0;
        adc_oneshot_read(s_adc, s_channel, &raw);
        adc_cali_raw_to_voltage(s_cali, raw, &mv);
        sum += mv;
    }
    return sum / SAMPLES_PER_READING;
}

static int percent_for(int mv)
{
    if (mv >= s_curve[0].mv) {
        return 100;
    }
    for (int i = 1; i < CURVE_LEN; i++) {
        if (mv >= s_curve[i].mv) {
            const int span = s_curve[i - 1].mv - s_curve[i].mv;
            const int gain = s_curve[i - 1].percent - s_curve[i].percent;
            return s_curve[i].percent + (mv - s_curve[i].mv) * gain / span;
        }
    }
    return 0;
}

static void battery_task(void *arg)
{
    float smoothed = 0;
    bool was_charging = false;
    while (1) {
        const int mv = read_pin_mv() * BOARD_BATTERY_DIVIDER;
        const bool charging = usb_serial_jtag_is_connected() || mv >= EXTERNAL_POWER_MV;
        // Start over when external power goes away, so the charger's voltage does not linger
        // in the average.
        if (smoothed == 0 || charging != was_charging) {
            smoothed = mv;
        } else {
            smoothed += SMOOTHING * (mv - smoothed);
        }
        was_charging = charging;
        atomic_store(&s_charging, charging);
        if (!charging) {
            atomic_store(&s_percent, percent_for((int)smoothed));
        }
        ESP_LOGD(TAG, "%d mV (smoothed %d), charging %d, %d%%", mv, (int)smoothed, charging,
                 atomic_load(&s_percent));
        vTaskDelay(pdMS_TO_TICKS(READING_PERIOD_MS));
    }
}

void battery_init(void)
{
    adc_unit_t unit;
    ESP_ERROR_CHECK(adc_oneshot_io_to_channel(BOARD_BATTERY_ADC, &unit, &s_channel));
    const adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = unit };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));
    const adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,   // up to ~3.1 V at the pin, enough for the halved charger output
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, s_channel, &chan_cfg));
    const adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = unit,
        .chan = s_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali));

    xTaskCreate(battery_task, "battery", 3072, NULL, 2, NULL);
}

bool battery_charging(void)
{
    return atomic_load(&s_charging);
}

int battery_percent(void)
{
    return atomic_load(&s_percent);
}
