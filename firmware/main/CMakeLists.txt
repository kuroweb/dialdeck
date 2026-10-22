idf_component_register(SRCS "main.c" "display.c" "knob.c" "ble_hid.c" "touchpad.c"
                            "settings.c" "settings_screen.c" "ui.c" "deck_screen.c"
                            "deck_config.c" "config_service.c"
                       INCLUDE_DIRS "."
                       REQUIRES bt esp_hid nvs_flash esp_timer esp_lcd
                                esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_driver_spi)
