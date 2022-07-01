# SPDX-License-Identifier: Apache-2.0

# This build is dependent on:
#  - ZEPHYR_BASE
# Please add the path locations in your $ENV settings



cmake_minimum_required(VERSION 3.20.0)

set(NRF_SUPPORTED_BOARDS nrf5340dk_nrf5340_cpuapp)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/ltr303)
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/ubxlib_priv)
#list(APPEND ZEPHYR_EXTRA_MODULES $ENV{UBXLIB_BASE})


find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(c210_SensorAggregation)


target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/ublox_modules)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/ublox_modules/cell)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/ublox_modules/wifi)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/ublox_modules/position)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/sensors)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/data_handle)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/buttons_leds)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/system)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/shell_cmd)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

FILE(GLOB app_sources src/*.c)
FILE(GLOB head_sources src/*.h)
FILE(GLOB sensor_sources src/sensors/*.c)
FILE(GLOB ublox_modules_sources src/ublox_modules/*.c)
FILE(GLOB ublox_modules_cell_sources src/ublox_modules/cell/*.c)
FILE(GLOB ublox_modules_wifi_sources src/ublox_modules/wifi/*.c)
FILE(GLOB ublox_modules_position_sources src/ublox_modules/position/*.c)
FILE(GLOB data_handle_sources src/data_handle/*.c)
FILE(GLOB buttons_leds_sources src/buttons_leds/*.c)
FILE(GLOB system_sources src/system/*.c)
FILE(GLOB shell_cmd_sources src/shell_cmd/*.c)

target_sources(app PRIVATE ${app_sources})
target_sources(app PRIVATE ${head_sources})
target_sources(app PRIVATE ${sensor_sources})
target_sources(app PRIVATE ${ublox_modules_sources})
target_sources(app PRIVATE ${ublox_modules_cell_sources})
target_sources(app PRIVATE ${ublox_modules_wifi_sources})
target_sources(app PRIVATE ${ublox_modules_position_sources})
target_sources(app PRIVATE ${data_handle_sources})
target_sources(app PRIVATE ${buttons_leds_sources})
target_sources(app PRIVATE ${system_sources})
target_sources(app PRIVATE ${shell_cmd_sources})


