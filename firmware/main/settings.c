#include "settings.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

static const char *TAG = "settings";

#define NVS_NAMESPACE   "dialdeck"
#define NVS_KEY         "settings"
#define SAVE_DELAY_US   (1000 * 1000)

static settings_t s_settings = {
    .pointer_gain = 250,
    .pointer_accel = 10,
    .pointer_gain_max = 500,
    .scroll_step = 1,
    .scroll_reverse = 0,
    .brightness = 100,
};

static esp_timer_handle_t s_save_timer;

static void save(void *arg)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return;
    }
    esp_err_t err = nvs_set_blob(nvs, NVS_KEY, &s_settings, sizeof(s_settings));
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

void settings_changed(void)
{
    esp_timer_stop(s_save_timer);
    esp_timer_start_once(s_save_timer, SAVE_DELAY_US);
}
