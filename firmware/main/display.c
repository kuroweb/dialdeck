#include "display.h"

#include <stdbool.h>
#include <stdatomic.h>

#include "board.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st77916.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd_init_cmds.h"

static const char *TAG = "display";

#define LCD_HOST          SPI2_HOST
#define DRAW_BUF_LINES    40
#define BL_DUTY_MAX       1023
#define LCD_CMD_SLEEP_IN  0x10
#define LCD_CMD_SLEEP_OUT 0x11
#define LCD_SLEEP_OUT_MS  120

static atomic_int s_brightness = 100;
static atomic_int s_idle_timeout;
static atomic_llong s_last_activity_us;
static atomic_bool s_backlight_on = true;
static atomic_bool s_panel_sleeping;
static atomic_bool s_wake_requested;
static bool s_consume_wake_touch;
static esp_lcd_panel_io_handle_t s_panel_io;
static esp_lcd_panel_handle_t s_panel;

static void set_backlight(bool on)
{
    atomic_store(&s_backlight_on, on);
    const int duty = on ? BL_DUTY_MAX * atomic_load(&s_brightness) / 100 : 0;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void backlight_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    const ledc_channel_config_t channel = {
        .gpio_num = BOARD_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = BL_DUTY_MAX,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

void display_set_brightness(int percent)
{
    // Linear duty: the LED current (the battery load this setting can cut) tracks the
    // percentage. 1% is about 10 counts of 1023, still lit, so the UI stays usable.
    if (percent < 1) {
        percent = 1;
    } else if (percent > 100) {
        percent = 100;
    }
    atomic_store(&s_brightness, percent);
    if (atomic_load(&s_backlight_on)) {
        set_backlight(true);
    }
}

void display_set_idle_timeout(int seconds)
{
    atomic_store(&s_idle_timeout, seconds);
    display_note_activity();
}

void display_note_activity(void)
{
    atomic_store(&s_last_activity_us, esp_timer_get_time());
    if (atomic_load(&s_panel_sleeping)) {
        atomic_store(&s_wake_requested, true);
    } else if (!atomic_load(&s_backlight_on)) {
        set_backlight(true);
    }
}

static void touch_input_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        if (!atomic_load(&s_backlight_on)) {
            display_note_activity();
            s_consume_wake_touch = true;
        } else {
            // A drag can end without LVGL sending CLICKED; release the guard on the next touch.
            s_consume_wake_touch = false;
        }
    }
    if (!s_consume_wake_touch) {
        return;
    }

    lv_indev_stop_processing(lv_event_get_current_target(e));
    if (code == LV_EVENT_CLICKED) {
        s_consume_wake_touch = false;
    }
}

void display_idle_check(void)
{
    if (atomic_exchange(&s_wake_requested, false) && atomic_load(&s_panel_sleeping)) {
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_panel_io, LCD_CMD_SLEEP_OUT, NULL, 0));
        vTaskDelay(pdMS_TO_TICKS(LCD_SLEEP_OUT_MS));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
        atomic_store(&s_panel_sleeping, false);
        set_backlight(true);
    }

    const int timeout = atomic_load(&s_idle_timeout);
    if (timeout > 0 && !atomic_load(&s_panel_sleeping) &&
        esp_timer_get_time() - atomic_load(&s_last_activity_us) >= (int64_t)timeout * 1000000) {
        set_backlight(false);
        atomic_store(&s_panel_sleeping, true);
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, false));
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_panel_io, LCD_CMD_SLEEP_IN, NULL, 0));
    }
}

bool display_is_sleeping(void)
{
    return atomic_load(&s_panel_sleeping);
}

static void panel_init(esp_lcd_panel_io_handle_t *ret_io, esp_lcd_panel_handle_t *ret_panel)
{
    const spi_bus_config_t bus = ST77916_PANEL_BUS_QSPI_CONFIG(
        BOARD_LCD_SCK, BOARD_LCD_D0, BOARD_LCD_D1, BOARD_LCD_D2, BOARD_LCD_D3,
        BOARD_LCD_H_RES * DRAW_BUF_LINES * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = ST77916_PANEL_IO_QSPI_CONFIG(BOARD_LCD_CS, NULL, NULL);
    io_config.pclk_hz = BOARD_LCD_PCLK_HZ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, ret_io));

    st77916_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(*ret_io, &panel_config, ret_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*ret_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*ret_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*ret_panel, true));
}

static esp_lcd_touch_handle_t touch_init(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BOARD_TOUCH_SDA,
        .scl_io_num = BOARD_TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(bus, &io_config, &io));

    const esp_lcd_touch_config_t touch_config = {
        .x_max = BOARD_LCD_H_RES,
        .y_max = BOARD_LCD_V_RES,
        .rst_gpio_num = BOARD_TOUCH_RST,
        .int_gpio_num = BOARD_TOUCH_INT,
    };
    esp_lcd_touch_handle_t touch;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(io, &touch_config, &touch));
    return touch;
}

lv_display_t *display_init(void)
{
    panel_init(&s_panel_io, &s_panel);
    esp_lcd_touch_handle_t touch = touch_init();

    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    // Draw buffers in internal RAM: PSRAM buffers are too slow for this QSPI panel.
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_panel_io,
        .panel_handle = s_panel,
        .buffer_size = BOARD_LCD_H_RES * DRAW_BUF_LINES,
        .double_buffer = true,
        .hres = BOARD_LCD_H_RES,
        .vres = BOARD_LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch,
    };
    lv_indev_t *touch_indev = lvgl_port_add_touch(&touch_cfg);
    lv_indev_add_event_cb(touch_indev, touch_input_event, LV_EVENT_ALL, NULL);

    backlight_init();
    ESP_LOGI(TAG, "display ready");
    return disp;
}
