cmake_minimum_required(VERSION 3.16)
project(bleep_sim C CXX)

# PC emulator for the Bleep remote's firmware: the same app/ and UI code as
# the ESP32-S3 build, with sim/hal_sim.c standing in for the hardware.
#
#   cmake -S firmware/sim -B build/sim && cmake --build build/sim -j
#   ./build/sim/bleep_sim

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BLEEP_SDL "Open SDL windows (needs libsdl2-dev). OFF builds the headless test runner only." ON)
set(LVGL_DIR "" CACHE PATH "Use a local LVGL checkout instead of fetching v9.6.0")

set(FW ${CMAKE_CURRENT_LIST_DIR}/..)

# ---- LVGL ----
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/lv_conf.h CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(LV_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
if(LVGL_DIR)
  add_subdirectory(${LVGL_DIR} lvgl EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.6.0
    GIT_SHALLOW TRUE)
  FetchContent_GetProperties(lvgl)
  if(NOT lvgl_POPULATED)
    FetchContent_Populate(lvgl)
    add_subdirectory(${lvgl_SOURCE_DIR} ${lvgl_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
endif()

if(EMSCRIPTEN AND NOT BLEEP_SDL)
  # WebAssembly test runner (sim/test.sh --wasm): the headless scripts under node
  target_compile_definitions(lvgl PUBLIC BLEEP_SDL=0)
elseif(EMSCRIPTEN)
  # Browser build: emcmake cmake -S firmware/sim -B build/web (see README)
  target_compile_definitions(lvgl PUBLIC BLEEP_SDL=1)
  target_compile_options(lvgl PUBLIC -sUSE_SDL=2)
elseif(BLEEP_SDL)
  find_package(SDL2 REQUIRED)
  target_compile_definitions(lvgl PUBLIC BLEEP_SDL=1)
  target_include_directories(lvgl PRIVATE ${SDL2_INCLUDE_DIRS})
else()
  target_compile_definitions(lvgl PUBLIC BLEEP_SDL=0)
endif()

# ---- cJSON (the remote gets it from ESP-IDF's json component) ----
include(FetchContent)
FetchContent_Declare(cjson
  GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
  GIT_TAG v1.7.18
  GIT_SHALLOW TRUE)
FetchContent_GetProperties(cjson)
if(NOT cjson_POPULATED)
  FetchContent_Populate(cjson)
endif()

# ---- app ----
file(GLOB APP_SOURCES CONFIGURE_DEPENDS ${FW}/app/*.c ${FW}/app/ui/*.c ${FW}/app/ui/fonts/*.c)

add_executable(bleep_sim
  ${APP_SOURCES}
  main.c
  boot.c
  hal_sim.c
  demo.c
  store.c
  ${cjson_SOURCE_DIR}/cJSON.c
  ha_mock.c
  ir_library_demo.c
  panel.c
  script.c
  png.c
  web.c)
target_include_directories(bleep_sim PRIVATE ${FW}/app ${FW}/hal ${CMAKE_CURRENT_LIST_DIR} ${cjson_SOURCE_DIR})
target_compile_definitions(bleep_sim PRIVATE BLEEP_SIM=1)
target_compile_options(bleep_sim PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
target_link_libraries(bleep_sim PRIVATE lvgl m)
if(EMSCRIPTEN AND NOT BLEEP_SDL)
  target_link_options(bleep_sim PRIVATE -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=33554432 -sEXIT_RUNTIME=1)
  set_target_properties(bleep_sim PROPERTIES SUFFIX ".js")
elseif(EMSCRIPTEN)
  target_link_options(bleep_sim PRIVATE -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=33554432
    -sEXPORTED_FUNCTIONS=_main,_malloc,_free,_web_save,_web_key,_web_key_down,_web_key_up,_web_lift,_web_battery,_web_usb,_web_lux,_web_skip_idle,_web_status
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,lengthBytesUTF8)
  set_target_properties(bleep_sim PROPERTIES OUTPUT_NAME bleep SUFFIX ".js")
  configure_file(${CMAKE_CURRENT_LIST_DIR}/web/index.html ${CMAKE_CURRENT_BINARY_DIR}/index.html COPYONLY)
elseif(BLEEP_SDL)
  target_link_libraries(bleep_sim PRIVATE ${SDL2_LIBRARIES})
  target_include_directories(bleep_sim PRIVATE ${SDL2_INCLUDE_DIRS})
endif()
