#include "ble_hid.h"

#include <stdatomic.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_check.h"
#include "esp_hidd.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "ble_hid";

#define DEVICE_NAME        "DialDeck"
#define REPORT_ID_MOUSE    1

static const uint8_t s_report_map[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, REPORT_ID_MOUSE, //   Report ID
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (1)
    0x29, 0x03,        //     Usage Maximum (3)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x03,        //     Report Count (3)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x05,        //     Report Size (5)
    0x81, 0x03,        //     Input (Const)
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x06,        //     Input (Data,Var,Rel)
    0xC0,              //   End Collection
    0xC0,              // End Collection
};

static esp_hid_raw_report_map_t s_report_maps[] = {
    { .data = s_report_map, .len = sizeof(s_report_map) },
};

static const esp_hid_device_config_t s_hid_config = {
    .vendor_id = 0x16C0,
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = DEVICE_NAME,
    .manufacturer_name = "DialDeck",
    .serial_number = "0001",
    .report_maps = s_report_maps,
    .report_maps_len = 1,
};

static esp_hidd_dev_t *s_hid_dev;
static atomic_int s_state = BLE_HID_STATE_STARTING;

static int gap_event(struct ble_gap_event *event, void *arg);

static void advertise(void)
{
    struct ble_hs_adv_fields fields = {
        .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
        .appearance = ESP_HID_APPEARANCE_MOUSE,
        .appearance_is_present = 1,
        .name = (uint8_t *)DEVICE_NAME,
        .name_len = strlen(DEVICE_NAME),
        .name_is_complete = 1,
        .uuids16 = (ble_uuid16_t[]){ BLE_UUID16_INIT(0x1812) },   // HID service
        .num_uuids16 = 1,
        .uuids16_is_complete = 1,
    };
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields failed: %d", rc);
        return;
    }

    const struct ble_gap_adv_params params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .itvl_min = BLE_GAP_ADV_ITVL_MS(30),
        .itvl_max = BLE_GAP_ADV_ITVL_MS(50),
    };
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start failed: %d", rc);
        return;
    }
    atomic_store(&s_state, BLE_HID_STATE_ADVERTISING);
    ESP_LOGI(TAG, "advertising");
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connect status=%d", event->connect.status);
        if (event->connect.status != 0) {
            advertise();
        } else {
            // Ask for encryption right away so a bonded host resumes without user action.
            ble_gap_security_initiate(event->connect.conn_handle);
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect reason=%d", event->disconnect.reason);
        advertise();
        return 0;
    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "encryption status=%d", event->enc_change.status);
        if (event->enc_change.status == 0) {
            atomic_store(&s_state, BLE_HID_STATE_CONNECTED);
        }
        return 0;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        // The host lost its bond (e.g. "Forget this device"); drop ours and pair again.
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    default:
        return 0;
    }
}

static void hidd_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    switch ((esp_hidd_event_t)id) {
    case ESP_HIDD_START_EVENT:
        advertise();
        break;
    default:
        break;
    }
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_store_config_init(void);

void ble_hid_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_nimble_init());

    // Just Works pairing with bonding: the device has no way to show or enter a passkey yet.
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ESP_ERROR_CHECK(esp_hidd_dev_init(&s_hid_config, ESP_HID_TRANSPORT_BLE, hidd_event, &s_hid_dev));
    ble_svc_gap_device_name_set(DEVICE_NAME);
    ble_svc_gap_device_appearance_set(ESP_HID_APPEARANCE_MOUSE);

    ble_store_config_init();
    nimble_port_freertos_init(host_task);
}

ble_hid_state_t ble_hid_state(void)
{
    return atomic_load(&s_state);
}

static int take_step(int *value)
{
    const int step = *value > 127 ? 127 : (*value < -127 ? -127 : *value);
    *value -= step;
    return step;
}

void ble_hid_mouse(int dx, int dy, int wheel, uint8_t buttons)
{
    if (ble_hid_state() != BLE_HID_STATE_CONNECTED) {
        return;
    }
    do {
        uint8_t report[4] = {
            buttons,
            (uint8_t)(int8_t)take_step(&dx),
            (uint8_t)(int8_t)take_step(&dy),
            (uint8_t)(int8_t)take_step(&wheel),
        };
        esp_hidd_dev_input_set(s_hid_dev, 0, REPORT_ID_MOUSE, report, sizeof(report));
    } while (dx != 0 || dy != 0 || wheel != 0);
}
