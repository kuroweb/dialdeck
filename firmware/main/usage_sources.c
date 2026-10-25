#include "usage_sources.h"

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "usage_sources";

#define NVS_NAMESPACE  "dialdeck"
#define NVS_KEY        "usage_src"

static uint8_t s_mask = USAGE_SOURCE_DEFAULT;
static void (*s_on_change)(void);

void usage_sources_load(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    uint8_t stored = s_mask;
    if (nvs_get_u8(nvs, NVS_KEY, &stored) == ESP_OK) {
        s_mask = stored & (USAGE_SOURCE_CLAUDE | USAGE_SOURCE_CODEX | USAGE_SOURCE_CURSOR);
    }
    nvs_close(nvs);
}

uint8_t usage_sources_mask(void)
{
    return s_mask;
}

static void save_mask(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return;
    }
    const esp_err_t err = nvs_set_u8(nvs, NVS_KEY, s_mask);
    if (err == ESP_OK) {
        nvs_commit(nvs);
    }
    nvs_close(nvs);
    ESP_LOGI(TAG, "mask 0x%02x: %s", s_mask, esp_err_to_name(err));
}

bool usage_sources_set_mask(uint8_t mask)
{
    const uint8_t value = mask & (USAGE_SOURCE_CLAUDE | USAGE_SOURCE_CODEX | USAGE_SOURCE_CURSOR);
    if (value == s_mask) {
        return false;
    }
    s_mask = value;
    save_mask();
    if (s_on_change != NULL) {
        s_on_change();
    }
    return true;
}

bool usage_sources_enabled(int index)
{
    if (index < 0 || index >= USAGE_SOURCE_COUNT) {
        return false;
    }
    return (s_mask & (1 << index)) != 0;
}

void usage_sources_toggle(int index)
{
    if (index < 0 || index >= USAGE_SOURCE_COUNT) {
        return;
    }
    s_mask ^= (uint8_t)(1 << index);
    save_mask();
    if (s_on_change != NULL) {
        s_on_change();
    }
}

void usage_sources_on_change(void (*callback)(void))
{
    s_on_change = callback;
}
