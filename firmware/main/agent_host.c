#include "agent_host.h"

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "agent_host";

#define NVS_NAMESPACE  "dialdeck"
#define NVS_KEY        "agent_host"

static agent_host_t s_agent_host = AGENT_HOST_HERDR;
static void (*s_on_change)(void);

void agent_host_load(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    uint8_t stored = AGENT_HOST_HERDR;
    if (nvs_get_u8(nvs, NVS_KEY, &stored) == ESP_OK && stored <= AGENT_HOST_ORCA) {
        s_agent_host = (agent_host_t)stored;
    }
    nvs_close(nvs);
}

agent_host_t agent_host_get(void)
{
    return s_agent_host;
}

const char *agent_host_name(void)
{
    return s_agent_host == AGENT_HOST_ORCA ? "Orca" : "herdr";
}

bool agent_host_set(uint8_t value)
{
    if (value > AGENT_HOST_ORCA) {
        return false;
    }
    const agent_host_t next = (agent_host_t)value;
    if (next == s_agent_host) {
        return true;
    }
    s_agent_host = next;

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        esp_err_t err = nvs_set_u8(nvs, NVS_KEY, value);
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
        }
        nvs_close(nvs);
        ESP_LOGI(TAG, "%s: %s", agent_host_name(), esp_err_to_name(err));
    } else {
        ESP_LOGE(TAG, "nvs_open failed");
    }

    if (s_on_change != NULL) {
        s_on_change();
    }
    return true;
}

void agent_host_on_change(void (*callback)(void))
{
    s_on_change = callback;
}
