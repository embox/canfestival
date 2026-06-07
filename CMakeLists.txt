cmake_minimum_required(VERSION 3.16)
project(CanFestival VERSION 3.0 LANGUAGES C)

# ---------------------------------------------------------------------------
#  Build options
# ---------------------------------------------------------------------------
set(CF_TARGET "unix" CACHE STRING "Target platform (unix, xeno, windows, none)")
set(CF_CAN_DRIVER "virtual" CACHE STRING "CAN driver(s) to build; semicolon-separated list, e.g. \"virtual;socket\"")

option(CF_ENABLE_LSS "Enable LSS services" OFF)
option(CF_ENABLE_LSS_FS "Enable LSS FastScan" OFF)
option(CF_ENABLE_DLL_DRIVERS "Build CAN drivers as shared libraries" ON)
option(CF_BUILD_EXAMPLES "Build example applications" OFF)
option(CF_SDO_DYNAMIC_BUFFER_ALLOCATION "Use dynamic (heap) buffers for large SDO transfers" OFF)

# Stack constants
set(CF_MAX_CAN_BUS_ID 1 CACHE STRING "Number of CAN buses")
set(CF_SDO_MAX_LENGTH_TRANSFER 32 CACHE STRING "Max SDO transfer bytes")
set(CF_SDO_BLOCK_SIZE 16 CACHE STRING "SDO block transfer size")
set(CF_SDO_MAX_SIMULTANEOUS_TRANSFERS 4 CACHE STRING "Concurrent SDO transfers")
set(CF_NMT_MAX_NODE_ID 128 CACHE STRING "Max NMT node ID")
set(CF_SDO_TIMEOUT_MS 3000 CACHE STRING "SDO timeout in ms")
set(CF_MAX_NB_TIMER 32 CACHE STRING "Max timers")
set(CF_EMCY_MAX_ERRORS 8 CACHE STRING "Max emergency errors")
set(CF_LSS_TIMEOUT_MS 1000 CACHE STRING "LSS timeout in ms")
set(CF_LSS_FS_TIMEOUT_MS 100 CACHE STRING "LSS FastScan timeout in ms")
set(CF_SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE 512 CACHE STRING "Max dynamic SDO buffer size in bytes")

# Dynamic (heap) SDO buffers. The variable names match the config.h.in
# cmakedefine guards; left unset (feature off) otherwise.
if(CF_SDO_DYNAMIC_BUFFER_ALLOCATION)
    set(SDO_DYNAMIC_BUFFER_ALLOCATION 1)
    set(SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE ${CF_SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE})
endif()

# ---------------------------------------------------------------------------
#  Endianness detection
# ---------------------------------------------------------------------------
include(TestBigEndian)
test_big_endian(CF_BIG_ENDIAN)
if(CF_BIG_ENDIAN)
    set(CANOPEN_BIG_ENDIAN 1)
endif()

# ---------------------------------------------------------------------------
#  Generate REPEAT_*_TIMES macros
# ---------------------------------------------------------------------------
function(generate_repeat_macro VAR_NAME COUNT OUTPUT)
    set(result "#define REPEAT_${VAR_NAME}_TIMES(repeat)")
    set(body "")
    math(EXPR limit "${COUNT} - 1")
    foreach(i RANGE 0 ${limit})
        string(APPEND body " repeat")
    endforeach()
    set(result "${result}\\\n${body}\n")
    set(${OUTPUT} "${result}" PARENT_SCOPE)
endfunction()

generate_repeat_macro("SDO_MAX_SIMULTANEOUS_TRANSFERS" ${CF_SDO_MAX_SIMULTANEOUS_TRANSFERS} REPEAT_SDO)
generate_repeat_macro("NMT_MAX_NODE_ID" ${CF_NMT_MAX_NODE_ID} REPEAT_NMT)
generate_repeat_macro("EMCY_MAX_ERRORS" ${CF_EMCY_MAX_ERRORS} REPEAT_EMCY)
set(CF_REPEAT_MACROS "${REPEAT_SDO}\n${REPEAT_NMT}\n${REPEAT_EMCY}")

# ---------------------------------------------------------------------------
#  Generate config.h
# ---------------------------------------------------------------------------
configure_file(include/config.h.in ${CMAKE_BINARY_DIR}/include/config.h @ONLY)

# ---------------------------------------------------------------------------
#  Platform-specific flags
# ---------------------------------------------------------------------------
if(CF_TARGET STREQUAL "xeno")
    find_program(XENO_CONFIG xeno-config)
    if(XENO_CONFIG)
        execute_process(COMMAND ${XENO_CONFIG} --skin=alchemy --skin=rtdm --cflags
                        OUTPUT_VARIABLE XENO_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${XENO_CONFIG} --skin=alchemy --skin=rtdm --ldflags
                        OUTPUT_VARIABLE XENO_LDFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    set(RTCAN_SOCKET 1)
endif()

# ---------------------------------------------------------------------------
#  Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(drivers)

if(CF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ---------------------------------------------------------------------------
#  Install targets
# ---------------------------------------------------------------------------
install(DIRECTORY include/ DESTINATION include/canfestival
        FILES_MATCHING PATTERN "*.h"
        PATTERN "config.h.in" EXCLUDE)
install(FILES ${CMAKE_BINARY_DIR}/include/config.h
        DESTINATION include/canfestival)
install(DIRECTORY objdictgen/ DESTINATION share/canfestival/objdictgen
        FILES_MATCHING
        PATTERN "*.py"
        PATTERN "*.prf"
        PATTERN "*.ico"
        PATTERN "__pycache__" EXCLUDE
        PATTERN "gnosis*" EXCLUDE)
