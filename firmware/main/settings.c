#include "settings.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

static const char *TAG = "settings";

#define NVS_NAMESPACE   "dialdeck"
#define NVS_KEY         "settings"
#define NVS_DISPLAY_TIMEOUT_KEY "display_timeout"
#define SAVE_DELAY_US   (1000 * 1000)
#define DISPLAY_TIMEOUT_DEFAULT 30

static settings_t s_settings = {
    .pointer_gain = 250,
    .pointer_accel = 10,
    .pointer_gain_max = 500,
    .scroll_step = 1,
    .scroll_reverse = 0,
    .brightness = 100,
};
static int32_t s_display_timeout = DISPLAY_TIMEOUT_DEFAULT;

static const struct {
    size_t offset;
    int min;
    int max;
} s_ranges[] = {
    { offsetof(settings_t, pointer_gain),     100, 500 },
    { offsetof(settings_t, pointer_accel),    0,   50 },
    { offsetof(settings_t, pointer_gain_max), 100, 1000 },
    { offsetof(settings_t, scroll_step),      1,   5 },
    { offsetof(settings_t, scroll_reverse),   0,   1 },
    { offsetof(settings_t, brightness),       1,   100 },
};
#define RANGE_COUNT  (sizeof(s_ranges) / sizeof(s_ranges[0]))
_Static_assert(RANGE_COUNT * sizeof(int32_t) == sizeof(settings_t), "every field needs a range");

static esp_timer_handle_t s_save_timer;
static void (*s_on_change)(void);

static void save(void *arg)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return;
    }
    esp_err_t err = nvs_set_blob(nvs, NVS_KEY, &s_settings, sizeof(s_settings));
    if (err == ESP_OK) {
        err = nvs_set_i32(nvs, NVS_DISPLAY_TIMEOUT_KEY, s_display_timeout);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    ESP_LOGI(TAG, "saved: %s", esp_err_to_name(err));
}

void settings_load(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        settings_t stored;
        size_t len = sizeof(stored);
        // A blob of another size comes from an older layout; keep the defaults then.
        if (nvs_get_blob(nvs, NVS_KEY, &stored, &len) == ESP_OK && len == sizeof(stored)) {
            s_settings = stored;
        }
        nvs_get_i32(nvs, NVS_DISPLAY_TIMEOUT_KEY, &s_display_timeout);
        nvs_close(nvs);
    }

    const esp_timer_create_args_t args = {
        .callback = save,
        .name = "settings_save",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_save_timer));
}

settings_t *settings(void)
{
    return &s_settings;
}

int settings_display_timeout(void)
{
    return s_display_timeout;
}

void settings_set_display_timeout(int seconds)
{
    s_display_timeout = seconds;
    settings_changed();
}

void settings_range(size_t offset, int *min, int *max)
{
    for (int i = 0; i < RANGE_COUNT; i++) {
        if (s_ranges[i].offset == offset) {
            *min = s_ranges[i].min;
            *max = s_ranges[i].max;
            return;
        }
    }
    *min = *max = 0;
}

void settings_changed(void)
{
    esp_timer_stop(s_save_timer);
    esp_timer_start_once(s_save_timer, SAVE_DELAY_US);
}

bool settings_replace(const settings_t *value)
{
    for (int i = 0; i < RANGE_COUNT; i++) {
        const int32_t v = *(const int32_t *)((const char *)value + s_ranges[i].offset);
        if (v < s_ranges[i].min || v > s_ranges[i].max) {
            return false;
        }
    }
    s_settings = *value;
    settings_changed();
    if (s_on_change) {
        s_on_change();
    }
    return true;
}

void settings_on_change(void (*callback)(void))
{
    s_on_change = callback;
}
