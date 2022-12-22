# SPDX-License-Identifier: Apache-2.0

if(CONFIG_ICG20330)
  zephyr_include_directories(.)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ICG20330 icg20330.c)

endif()