# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

# Add custom u-blox boards
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../boards/short_range/zephyr/)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../boards/cellular/)

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(ubxlib)
set(UBXLIB_BASE $ENV{UBXLIB_BASE})
set(ZEPHYR_PORT_BASE ..)

#nrf5340 test runner application
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/app/u_main.c)

# examples
target_sources(app PRIVATE ${UBXLIB_BASE}/example/sockets/main.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/sockets/main_tls.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/sockets/credentials_tls.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/security/e2e/e2e_main.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/security/psk/psk_main.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/security/c2c/c2c_main.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/mqtt_client/mqtt_main.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/location/main_loc_gnss.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/location/main_loc_gnss_cell.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/location/main_loc_cell_locate.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/cell/lte_cfg/lte_cfg_main.c)
