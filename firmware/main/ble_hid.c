#include "ble_hid.h"

#include <stdatomic.h>
#include <string.h>

#include "config_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host_slots.h"
#include "esp_bt.h"
#include "esp_check.h"
#include "esp_hidd.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "ble_hid";

#define DEVICE_NAME        "DialDeck"
#define REPORT_ID_MOUSE    1
#define REPORT_ID_KEYBOARD 2
#define REPORT_ID_CONSUMER 3

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

    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, REPORT_ID_KEYBOARD, // Report ID
    0x05, 0x07,        //   Usage Page (Keyboard)
    0x19, 0xE0,        //   Usage Minimum (Left Control)
    0x29, 0xE7,        //   Usage Maximum (Right GUI)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data,Var,Abs): modifiers
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Const): reserved
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Keyboard)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data,Array): keys
    0xC0,              // End Collection

    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, REPORT_ID_CONSUMER, // Report ID
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
    0x19, 0x00,        //   Usage Minimum (0)
    0x2A, 0xFF, 0x03,  //   Usage Maximum (1023)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x00,        //   Input (Data,Array)
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

// Link state shared by the NimBLE host task (GAP events) and callers switching hosts.
static SemaphoreHandle_t s_lock;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static int s_pairing_slot = -1;   // slot waiting for a new host, or -1
static bool s_pairing_addr_set;   // random address for this pairing session chosen

static int gap_event(struct ble_gap_event *event, void *arg);

// Advertise for the active slot. A filled slot accepts only its host (allow-list on the identity
// address; a host using a private address is resolved with the IRK it gave when bonding) and is
// not discoverable, so other hosts cannot take the link. An empty slot, or one being re-paired, is
// discoverable and open to new hosts.
// Call with s_lock held.
static void advertise(void)
{
    const int slot = host_slots_active();
    const ble_addr_t *host = host_slots_addr(slot);
    if (host == NULL) {
        s_pairing_slot = slot;
    }
    const bool pairing = s_pairing_slot >= 0;

    struct ble_hs_adv_fields fields = {
        .flags = (pairing ? BLE_HS_ADV_F_DISC_GEN : 0) | BLE_HS_ADV_F_BREDR_UNSUP,
        .appearance = ESP_HID_APPEARANCE_GENERIC,
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
    if (!pairing && (rc = ble_gap_wl_set(host, 1)) != 0) {
        ESP_LOGE(TAG, "wl_set failed: %d", rc);
        return;
    }

    // While pairing, advertise from a fresh random address. Bonded hosts reconnect on their own as
    // soon as they see the device's public address, and would otherwise grab the open link; they
    // do not recognise a random one. The new host still learns the public identity address while
    // bonding, so it reconnects normally afterwards.
    uint8_t own_addr_type = BLE_OWN_ADDR_PUBLIC;
    if (pairing) {
        if (!s_pairing_addr_set) {
            ble_addr_t random;
            if (ble_hs_id_gen_rnd(0, &random) != 0 || ble_hs_id_set_rnd(random.val) != 0) {
                ESP_LOGE(TAG, "random address failed");
                return;
            }
            s_pairing_addr_set = true;
        }
        own_addr_type = BLE_OWN_ADDR_RANDOM;
    }

    const struct ble_gap_adv_params params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = pairing ? BLE_GAP_DISC_MODE_GEN : BLE_GAP_DISC_MODE_NON,
        .filter_policy = pairing ? BLE_HCI_ADV_FILT_NONE : BLE_HCI_ADV_FILT_BOTH,
        .itvl_min = BLE_GAP_ADV_ITVL_MS(30),
        .itvl_max = BLE_GAP_ADV_ITVL_MS(50),
    };
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start failed: %d", rc);
        return;
    }
    atomic_store(&s_state, pairing ? BLE_HID_STATE_PAIRING : BLE_HID_STATE_ADVERTISING);
    ESP_LOGI(TAG, "advertising for host %d%s", slot + 1, pairing ? " (pairing)" : "");
}

// Drop the current link, or the current advertising, and advertise for the active slot.
// Call with s_lock held.
static void restart_link(void)
{
    if (s_conn != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);   // DISCONNECT re-advertises
    } else {
        ble_gap_adv_stop();
        advertise();
    }
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc = 0;

    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connect status=%d", event->connect.status);
        if (event->connect.status != 0) {
            advertise();
            break;
        }
        s_conn = event->connect.conn_handle;
        // Ask for encryption right away so a bonded host resumes without user action.
        ble_gap_security_initiate(s_conn);
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect reason=%d", event->disconnect.reason);
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        advertise();
        break;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        // The host starts encryption itself, so this can arrive before CONNECT: decide here
        // whether the peer belongs to the active slot.
        ESP_LOGI(TAG, "encryption status=%d", event->enc_change.status);
        if (event->enc_change.status != 0 || ble_gap_conn_find(event->enc_change.conn_handle, &desc) != 0) {
            break;
        }
        s_conn = event->enc_change.conn_handle;
        const int owner = host_slots_find(&desc.peer_id_addr);
        const int wanted = s_pairing_slot >= 0 ? s_pairing_slot : host_slots_active();
        if (owner >= 0 && owner != wanted) {
            ESP_LOGI(TAG, "host %d connected while waiting for host %d; refused", owner + 1, wanted + 1);
            ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
            break;
        }
        if (s_pairing_slot >= 0) {
            host_slots_assign(s_pairing_slot, &desc.peer_id_addr);
            host_slots_set_active(s_pairing_slot);
            ESP_LOGI(TAG, "paired host %d", s_pairing_slot + 1);
            s_pairing_slot = -1;
            s_pairing_addr_set = false;
        }
        atomic_store(&s_state, BLE_HID_STATE_CONNECTED);
        break;
    }
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        // The host lost its bond (e.g. "Forget this device"); drop ours and pair again.
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        rc = BLE_GAP_REPEAT_PAIRING_RETRY;
        break;
    default:
        break;
    }
    xSemaphoreGiveRecursive(s_lock);
    return rc;
}

// Before host slots existed the device kept one bond and accepted any host. Give existing
// bonds slots so those hosts keep working.
static void adopt_existing_bonds(void)
{
    for (int i = 0; i < HOST_SLOT_COUNT; i++) {
        if (host_slots_addr(i)) {
            return;
        }
    }
    ble_addr_t peers[HOST_SLOT_COUNT];
    int count = 0;
    if (ble_store_util_bonded_peers(peers, &count, HOST_SLOT_COUNT) != 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        host_slots_assign(i, &peers[i]);
    }
}

static void hidd_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    switch ((esp_hidd_event_t)id) {
    case ESP_HIDD_START_EVENT:
        xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
        adopt_existing_bonds();
        advertise();
        xSemaphoreGiveRecursive(s_lock);
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
    s_lock = xSemaphoreCreateRecursiveMutex();
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
    ble_svc_gap_device_appearance_set(ESP_HID_APPEARANCE_GENERIC);
    config_service_register();

    ble_store_config_init();
    nimble_port_freertos_init(host_task);
}

ble_hid_state_t ble_hid_state(void)
{
    return atomic_load(&s_state);
}

int ble_hid_host(void)
{
    return host_slots_active();
}

bool ble_hid_host_paired(int slot)
{
    return host_slots_addr(slot) != NULL;
}

void ble_hid_select_host(int slot)
{
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    s_pairing_slot = -1;
    s_pairing_addr_set = false;
    host_slots_set_active(slot);
    restart_link();
    xSemaphoreGiveRecursive(s_lock);
}

void ble_hid_pair_host(int slot)
{
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    const ble_addr_t *old = host_slots_addr(slot);
    if (old) {
        const ble_addr_t forgotten = *old;
        host_slots_assign(slot, NULL);
        // Keep the bond if another slot still uses this host.
        if (host_slots_find(&forgotten) < 0) {
            ble_store_util_delete_peer(&forgotten);
        }
    }
    host_slots_set_active(slot);
    s_pairing_slot = slot;
    s_pairing_addr_set = false;
    restart_link();
    xSemaphoreGiveRecursive(s_lock);
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

void ble_hid_key(uint8_t modifiers, uint8_t key)
{
    if (ble_hid_state() != BLE_HID_STATE_CONNECTED) {
        return;
    }
    uint8_t report[8] = { modifiers, 0, key };
    esp_hidd_dev_input_set(s_hid_dev, 0, REPORT_ID_KEYBOARD, report, sizeof(report));
    memset(report, 0, sizeof(report));
    esp_hidd_dev_input_set(s_hid_dev, 0, REPORT_ID_KEYBOARD, report, sizeof(report));
}

void ble_hid_consumer(uint16_t usage)
{
    if (ble_hid_state() != BLE_HID_STATE_CONNECTED) {
        return;
    }
    uint8_t report[2] = { usage & 0xFF, usage >> 8 };
    esp_hidd_dev_input_set(s_hid_dev, 0, REPORT_ID_CONSUMER, report, sizeof(report));
    memset(report, 0, sizeof(report));
    esp_hidd_dev_input_set(s_hid_dev, 0, REPORT_ID_CONSUMER, report, sizeof(report));
}

void ble_hid_set_battery(int percent)
{
    esp_hidd_dev_battery_set(s_hid_dev, (uint8_t)percent);
}
