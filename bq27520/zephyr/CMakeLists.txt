# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BQ27520)
  zephyr_include_directories(.)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_BQ27520 bq27520.c)

endif()