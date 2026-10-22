#include "config_service.h"

#include <string.h>

#include "deck_config.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "settings.h"

static const char *TAG = "config_service";

#define PROTOCOL_VERSION  2

// 98d3462c-2eb8-4022-ba87-4f253e72a24b and characteristics sharing its base.
// NimBLE takes 128-bit UUIDs least significant byte first.
#define CONFIG_UUID(n) BLE_UUID128_INIT(0x4b, 0xa2, 0x72, 0x3e, 0x25, 0x4f, 0x87, 0xba, \
                                        0x22, 0x40, 0xb8, 0x2e, (n), 0x46, 0xd3, 0x98)

static const ble_uuid128_t s_service_uuid = CONFIG_UUID(0x2c);
static const ble_uuid128_t s_info_uuid = CONFIG_UUID(0x2d);
static const ble_uuid128_t s_deck_uuid = CONFIG_UUID(0x2e);
static const ble_uuid128_t s_settings_uuid = CONFIG_UUID(0x2f);

// Info: [protocol version, slot count, label length].
static int access_info(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const uint8_t info[] = { PROTOCOL_VERSION, DECK_SLOT_COUNT, DECK_LABEL_LEN };
    return os_mbuf_append(ctxt->om, info, sizeof(info)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// Deck: read returns every slot back to back; write takes [slot index] + one slot.
static int access_deck(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        for (int i = 0; i < DECK_SLOT_COUNT; i++) {
            if (os_mbuf_append(ctxt->om, deck_config_slot(i), sizeof(deck_slot_t)) != 0) {
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }
        }
        return 0;
    }

    uint8_t buf[1 + sizeof(deck_slot_t)];
    uint16_t len = 0;
    if (OS_MBUF_PKTLEN(ctxt->om) != sizeof(buf) ||
        ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &len) != 0) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    deck_slot_t slot;
    memcpy(&slot, buf + 1, sizeof(slot));
    if (!deck_config_set(buf[0], &slot)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }
    ESP_LOGI(TAG, "slot %d set to \"%s\"", buf[0], slot.label);
    return 0;
}

// Settings: read returns settings_t; write replaces it. Out-of-range values are refused.
static int access_settings(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return os_mbuf_append(ctxt->om, settings(), sizeof(settings_t)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    settings_t value;
    uint16_t len = 0;
    if (OS_MBUF_PKTLEN(ctxt->om) != sizeof(value) ||
        ble_hs_mbuf_to_flat(ctxt->om, &value, sizeof(value), &len) != 0) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (!settings_replace(&value)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }
    ESP_LOGI(TAG, "settings replaced");
    return 0;
}

static const struct ble_gatt_svc_def s_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &s_info_uuid.u,
                .access_cb = access_info,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Only a bonded, encrypted link may read or change assignments.
                .uuid = &s_deck_uuid.u,
                .access_cb = access_deck,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                         BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
            },
            {
                .uuid = &s_settings_uuid.u,
                .access_cb = access_settings,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                         BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
            },
            { 0 },
        },
    },
    { 0 },
};

void config_service_register(void)
{
    ESP_ERROR_CHECK(ble_gatts_count_cfg(s_services));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(s_services));
}
