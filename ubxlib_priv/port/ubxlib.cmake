cmake_minimum_required(VERSION 3.13.1)

# Always include base components
list(APPEND UBXLIB_COMPONENTS base)

# Conditionally add one .c file to UBXLIB_SRC
function(u_add_source_file component file)
  if (${component} IN_LIST UBXLIB_COMPONENTS)
    list(APPEND UBXLIB_SRC ${file})
    set(UBXLIB_SRC ${UBXLIB_SRC} PARENT_SCOPE)
  endif()
endfunction()

# Conditionally add all .c files in a directory to UBXLIB_SRC
function(u_add_source_dir component src_dir)
  if (${component} IN_LIST UBXLIB_COMPONENTS)
    file(GLOB SRCS ${src_dir}/*.c)
    list(APPEND UBXLIB_SRC ${SRCS})
    set(UBXLIB_SRC ${UBXLIB_SRC} PARENT_SCOPE)
  endif()
endfunction()

# Conditionally add all .c files in a directory to UBXLIB_TEST_SRC
function(u_add_test_source_dir component src_dir)
  if (${component} IN_LIST UBXLIB_COMPONENTS)
    file(GLOB SRCS ${src_dir}/*.c)
    list(APPEND UBXLIB_TEST_SRC ${SRCS})
    set(UBXLIB_TEST_SRC ${UBXLIB_TEST_SRC} PARENT_SCOPE)
  endif()
endfunction()

# This function will take a component directory and:
# - Add <component_dir>/src/*.c to UBXLIB_SRC
# - Add <component_dir>/src to UBXLIB_PRIVATE_INC
# - Add <component_dir>/api to UBXLIB_INC
# - Add <component_dir>/test/*.c to UBXLIB_TEST_SRC
# - Add <component_dir>/test to UBXLIB_TEST_INC
function(u_add_component_dir component component_dir)
  if (${component} IN_LIST UBXLIB_COMPONENTS)
    u_add_source_dir(${component} ${component_dir}/src)
    u_add_test_source_dir(${component} ${component_dir}/test)
    if(EXISTS ${component_dir}/api)
      list(APPEND UBXLIB_INC ${component_dir}/api)
    endif()
    if(EXISTS ${component_dir}/src)
      list(APPEND UBXLIB_PRIVATE_INC ${component_dir}/src)
    endif()
    if(EXISTS ${component_dir}/test)
      list(APPEND UBXLIB_TEST_INC ${component_dir}/test)
    endif()
    set(UBXLIB_SRC ${UBXLIB_SRC} PARENT_SCOPE)
    set(UBXLIB_INC ${UBXLIB_INC} PARENT_SCOPE)
    set(UBXLIB_PRIVATE_INC ${UBXLIB_PRIVATE_INC} PARENT_SCOPE)
    set(UBXLIB_TEST_SRC ${UBXLIB_TEST_SRC} PARENT_SCOPE)
    set(UBXLIB_TEST_INC ${UBXLIB_TEST_INC} PARENT_SCOPE)
  endif()
endfunction()

# ubxlib base source and includes
list(APPEND UBXLIB_INC
  ${UBXLIB_BASE}/cfg
  ${UBXLIB_BASE}/port/api
)
list(APPEND UBXLIB_PRIVATE_INC
  ${UBXLIB_BASE}/port/platform/common/event_queue
)
u_add_source_dir(base ${UBXLIB_BASE}/port/platform/common/event_queue)
u_add_source_dir(base ${UBXLIB_BASE}/port/platform/common/mbedtls)
u_add_source_dir(base ${UBXLIB_BASE}/port/platform/common/mutex_debug)
u_add_component_dir(base ${UBXLIB_BASE}/common/at_client)
u_add_component_dir(base ${UBXLIB_BASE}/common/error)
u_add_component_dir(base ${UBXLIB_BASE}/common/location)
u_add_component_dir(base ${UBXLIB_BASE}/common/mqtt_client)
u_add_component_dir(base ${UBXLIB_BASE}/common/security)
u_add_component_dir(base ${UBXLIB_BASE}/common/sock)
u_add_component_dir(base ${UBXLIB_BASE}/common/ubx_protocol)
u_add_component_dir(base ${UBXLIB_BASE}/common/utils)
# Network requires special care since it contains stub & optional files
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/network/src/u_network.c)
list(APPEND UBXLIB_INC ${UBXLIB_BASE}/common/network/api)
list(APPEND UBXLIB_PRIVATE_INC ${UBXLIB_BASE}/common/network/src)

# Optional components
# shortrange
u_add_component_dir(shortrange ${UBXLIB_BASE}/common/short_range)
u_add_component_dir(shortrange ${UBXLIB_BASE}/ble)
u_add_component_dir(shortrange ${UBXLIB_BASE}/wifi)
u_add_source_file(shortrange ${UBXLIB_BASE}/common/network/src/u_network_private_ble_extmod.c)
u_add_source_file(shortrange ${UBXLIB_BASE}/common/network/src/u_network_private_ble_intmod.c)
u_add_source_file(shortrange ${UBXLIB_BASE}/common/network/src/u_network_private_wifi.c)
u_add_source_file(shortrange ${UBXLIB_BASE}/common/network/src/u_network_private_short_range.c)
# cell
u_add_component_dir(cell ${UBXLIB_BASE}/cell)
u_add_source_file(shortrange ${UBXLIB_BASE}/common/network/src/u_network_private_cell.c)
# gnss
u_add_component_dir(gnss ${UBXLIB_BASE}/gnss)
u_add_source_file(gnss ${UBXLIB_BASE}/common/network/src/u_network_private_gnss.c)

# Test related files and directories
list(APPEND UBXLIB_TEST_INC
  ${UBXLIB_BASE}/common/network/test
  ${UBXLIB_BASE}/port/platform/common/runner
)
u_add_test_source_dir(base ${UBXLIB_BASE}/port/platform/common/runner)
u_add_test_source_dir(base ${UBXLIB_BASE}/common/network/test)
u_add_test_source_dir(base ${UBXLIB_BASE}/port/platform/common/test)
u_add_test_source_dir(base ${UBXLIB_BASE}/port/test)
