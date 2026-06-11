# CanFestival CANopen stack - Zephyr module build
#
# Written by Edouard TISSERANT
# SPDX-License-Identifier: LGPL-2.1-or-later

if(CONFIG_CANFESTIVAL)

  set(CF_ROOT ${ZEPHYR_CURRENT_MODULE_DIR})

  # ---------------------------------------------------------------------------
  #  Stack constants (mirror the defaults of the top-level CMakeLists.txt)
  # ---------------------------------------------------------------------------
  # The number of CAN ports is NOT configured here: drivers/zephyr/zephyr.c and
  # drivers/can_zephyr/can_zephyr.c size their pools directly from the
  # "canfestival,interfaces" devicetree node (DT_PROP_LEN), which is the single
  # source of truth (CMake's dt_prop() cannot enumerate a phandle-array).
  # Remaining stack constants are tunable through Kconfig.
  set(CF_SDO_MAX_LENGTH_TRANSFER ${CONFIG_CANFESTIVAL_SDO_MAX_LENGTH_TRANSFER})
  set(CF_SDO_BLOCK_SIZE ${CONFIG_CANFESTIVAL_SDO_BLOCK_SIZE})
  set(CF_SDO_MAX_SIMULTANEOUS_TRANSFERS ${CONFIG_CANFESTIVAL_SDO_MAX_SIMULTANEOUS_TRANSFERS})
  set(CF_NMT_MAX_NODE_ID ${CONFIG_CANFESTIVAL_NMT_MAX_NODE_ID})
  set(CF_SDO_TIMEOUT_MS ${CONFIG_CANFESTIVAL_SDO_TIMEOUT_MS})
  set(CF_MAX_NB_TIMER ${CONFIG_CANFESTIVAL_MAX_NB_TIMER})
  set(CF_EMCY_MAX_ERRORS ${CONFIG_CANFESTIVAL_EMCY_MAX_ERRORS})
  # LSS timeouts only exist as Kconfig symbols when LSS is enabled; the values
  # still feed canfestival_config.h unconditionally, so fall back to defaults otherwise.
  if(CONFIG_CANFESTIVAL_ENABLE_LSS)
    set(CF_LSS_TIMEOUT_MS ${CONFIG_CANFESTIVAL_LSS_TIMEOUT_MS})
    set(CF_LSS_FS_TIMEOUT_MS ${CONFIG_CANFESTIVAL_LSS_FS_TIMEOUT_MS})
  else()
    set(CF_LSS_TIMEOUT_MS 1000)
    set(CF_LSS_FS_TIMEOUT_MS 100)
  endif()
  # Dynamic (heap) SDO buffers. The variable names match the canfestival_config.h.in
  # cmakedefine guards; left unset (feature off) otherwise.
  if(CONFIG_CANFESTIVAL_SDO_DYNAMIC_BUFFER_ALLOCATION)
    set(SDO_DYNAMIC_BUFFER_ALLOCATION 1)
    set(SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE ${CONFIG_CANFESTIVAL_SDO_DYNAMIC_BUFFER_ALLOCATION_SIZE})
  endif()

  # TIMEVAL / TIMEVAL_MAX are provided by include/zephyr/applicfg.h, so they
  # are not part of the generated canfestival_config.h at all.

  # ---------------------------------------------------------------------------
  #  Endianness detection
  # ---------------------------------------------------------------------------
  include(TestBigEndian)
  test_big_endian(CF_BIG_ENDIAN)
  if(CF_BIG_ENDIAN)
    set(CANOPEN_BIG_ENDIAN 1)
  endif()

  # ---------------------------------------------------------------------------
  #  Generate REPEAT_*_TIMES macros (same helper as the top-level CMakeLists)
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
  #  Generate canfestival_config.h from the shared template
  # ---------------------------------------------------------------------------
  configure_file(${CF_ROOT}/include/canfestival_config.h.in
                 ${CMAKE_CURRENT_BINARY_DIR}/include/canfestival_config.h @ONLY)

  # ---------------------------------------------------------------------------
  #  CanFestival library
  # ---------------------------------------------------------------------------
  zephyr_library_named(canfestival)

  zephyr_library_sources(
    # CANopen core stack (all portable)
    ${CF_ROOT}/src/dcf.c
    ${CF_ROOT}/src/emcy.c
    ${CF_ROOT}/src/lifegrd.c
    ${CF_ROOT}/src/nmtMaster.c
    ${CF_ROOT}/src/nmtSlave.c
    ${CF_ROOT}/src/objacces.c
    ${CF_ROOT}/src/pdo.c
    ${CF_ROOT}/src/sdo.c
    ${CF_ROOT}/src/states.c
    ${CF_ROOT}/src/sync.c
    ${CF_ROOT}/src/timer.c
    # Zephyr platform wrapper (incl. timer driver) + CAN driver, linked
    # directly (no dynamic loading)
    ${CF_ROOT}/drivers/zephyr/zephyr.c
    ${CF_ROOT}/drivers/can_zephyr/can_zephyr.c
  )
  zephyr_library_sources_ifdef(CONFIG_CANFESTIVAL_ENABLE_LSS ${CF_ROOT}/src/lss.c)

  zephyr_library_compile_definitions(NOT_USE_DYNAMIC_LOADING)
  zephyr_library_compile_definitions_ifdef(CONFIG_CANFESTIVAL_ENABLE_LSS CO_ENABLE_LSS)

  set(CF_INCLUDE_DIRS
    ${CF_ROOT}/include
    ${CF_ROOT}/include/zephyr          # applicfg.h, stackcfg.h
    ${CF_ROOT}/drivers/zephyr
    ${CMAKE_CURRENT_BINARY_DIR}/include # generated canfestival_config.h
  )

  # Make the headers visible to the CanFestival sources and to the application
  # (so PLC/runtime glue can call canOpen/setState/... against the stack).
  zephyr_library_include_directories(${CF_INCLUDE_DIRS})
  zephyr_include_directories(${CF_INCLUDE_DIRS})

endif()
