# Bleep v1 firmware (ESP32-S3-WROOM-1-N8R2): the same app/ and UI code as the
# emulator, with the board's HAL in main/. See README.md.
#
#   idf.py set-target esp32s3 && idf.py build      (ESP-IDF 5.5)
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# LVGL reads our lv_conf.h (main/), not its Kconfig (CONFIG_LV_CONF_SKIP=n in
# sdkconfig.defaults); its IDF build includes it by name (LV_CONF_INCLUDE_SIMPLE)
idf_build_set_property(COMPILE_OPTIONS "-I${CMAKE_CURRENT_LIST_DIR}/main" APPEND)
project(bleep)
