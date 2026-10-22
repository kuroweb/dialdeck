#include "host_slots.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "host_slots";

#define NVS_NAMESPACE  "dialdeck"
#define NVS_KEY        "hosts"

static struct {
    uint8_t used;                        // bit per slot
    uint8_t active;
    ble_addr_t addr[HOST_SLOT_COUNT];
} s_hosts;

static void save(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return;
    }
    esp_err_t err = nvs_set_blob(nvs, NVS_KEY, &s_hosts, sizeof(s_hosts));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    ESP_LOGI(TAG, "saved: %s", esp_err_to_name(err));
}

void host_slots_load(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    size_t len = sizeof(s_hosts);
    if (nvs_get_blob(nvs, NVS_KEY, &s_hosts, &len) != ESP_OK || len != sizeof(s_hosts) ||
        s_hosts.active >= HOST_SLOT_COUNT) {
        memset(&s_hosts, 0, sizeof(s_hosts));
    }
    nvs_close(nvs);

    // An earlier firmware could record one host in two slots; keep only the first.
    for (int i = 0; i < HOST_SLOT_COUNT; i++) {
        if (host_slots_addr(i) && host_slots_find(&s_hosts.addr[i]) != i) {
            ESP_LOGW(TAG, "slot %d duplicates slot %d; cleared", i + 1, host_slots_find(&s_hosts.addr[i]) + 1);
            host_slots_assign(i, NULL);
        }
    }
}

const ble_addr_t *host_slots_addr(int slot)
{
    return (s_hosts.used & (1 << slot)) ? &s_hosts.addr[slot] : NULL;
}

int host_slots_find(const ble_addr_t *addr)
{
    for (int i = 0; i < HOST_SLOT_COUNT; i++) {
        if ((s_hosts.used & (1 << i)) && ble_addr_cmp(&s_hosts.addr[i], addr) == 0) {
            return i;
        }
    }
    return -1;
}

int host_slots_active(void)
{
    return s_hosts.active;
}

void host_slots_set_active(int slot)
{
    if (s_hosts.active != slot) {
        s_hosts.active = slot;
        save();
    }
}

void host_slots_assign(int slot, const ble_addr_t *addr)
{
    if (addr) {
        s_hosts.addr[slot] = *addr;
        s_hosts.used |= 1 << slot;
    } else {
        memset(&s_hosts.addr[slot], 0, sizeof(ble_addr_t));
        s_hosts.used &= ~(1 << slot);
    }
    save();
}
