idf_component_register(SRCS "main.c" "display.c" "knob.c" "ble_hid.c" "touchpad.c"
                            "settings.c" "settings_screen.c" "ui.c" "deck_screen.c"
                            "touchpad_screen.c" "status_bar.c" "widgets.c"
                            "deck_config.c" "config_service.c" "battery.c"
                            "host_slots.c"
                            "fonts/font_ja.c" "fonts/font_ja_16.c"
                            "agents.c" "agents_screen.c" "usage.c" "usage_sources.c"
                       INCLUDE_DIRS "." "fonts"
                       REQUIRES bt esp_hid nvs_flash esp_timer esp_lcd esp_rom
                                esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_driver_spi
                                esp_adc esp_driver_usb_serial_jtag)
