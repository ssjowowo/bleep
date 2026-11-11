# The board's HAL (this directory) and everything in app/, as in the emulator
set(FW ${CMAKE_CURRENT_LIST_DIR}/../..)
file(GLOB APP_SRCS ${FW}/app/*.c ${FW}/app/ui/*.c ${FW}/app/ui/fonts/*.c)
# the Home Assistant client's portable core (the emulator links its demo house instead)
file(GLOB HA_SRCS ${FW}/ha/*.c)

idf_component_register(
    SRCS main.c lv_mem_psram.c i2c.c display.c touch.c keys.c sensors.c system.c storage.c wifi.c ble.c ir.c ota.c ha_client.c usb_update.c
         ${APP_SRCS} ${HA_SRCS}
    INCLUDE_DIRS . ${FW}/app ${FW}/hal ${FW}/ha
    REQUIRES lvgl json esp_lcd esp_driver_i2c esp_driver_gpio esp_driver_ledc esp_driver_rmt esp_timer
             nvs_flash esp_wifi esp_netif esp_event bt esp_hid esp_http_client esp_https_ota app_update
             mbedtls esp_psram esp_pm littlefs esp_websocket_client mdns esp_driver_usb_serial_jtag spi_flash)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-parameter -Wno-missing-field-initializers)
# LVGL calls lv_mem_psram.c's allocator but main itself doesn't: keep it linked
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u lv_mem_init" "-u lv_malloc_core")
