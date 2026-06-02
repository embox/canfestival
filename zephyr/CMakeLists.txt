# CanFestival CANopen stack - Zephyr module build
#
# Written by Edouard TISSERANT
# SPDX-License-Identifier: LGPL-2.1-or-later

if(CONFIG_CANFESTIVAL)

  set(CF_ROOT ${ZEPHYR_CURRENT_MODULE_DIR})

  # ---------------------------------------------------------------------------
  #  Stack constants (mirror the defaults of the top-level CMakeLists.txt)
  # ---------------------------------------------------------------------------
  set(CF_MAX_CAN_BUS_ID 1)
  set(CF_SDO_MAX_LENGTH_TRANSFER 32)
  set(CF_SDO_BLOCK_SIZE 16)
  set(CF_SDO_MAX_SIMULTANEOUS_TRANSFERS 4)
  set(CF_NMT_MAX_NODE_ID 128)
  set(CF_SDO_TIMEOUT_MS 3000)
  set(CF_MAX_NB_TIMER 32)
  set(CF_EMCY_MAX_ERRORS 8)
  set(CF_LSS_TIMEOUT_MS 1000)
  set(CF_LSS_FS_TIMEOUT_MS 100)

  # TIMEVAL / TIMEVAL_MAX are provided by include/timers_zephyr/timerscfg.h,
  # so they are intentionally left unset here (config.h #undef-s them).

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
  #  Generate config.h from the shared template
  # ---------------------------------------------------------------------------
  configure_file(${CF_ROOT}/include/config.h.in
                 ${CMAKE_CURRENT_BINARY_DIR}/include/config.h @ONLY)

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
    # Zephyr platform wrapper + drivers (linked directly, no dynamic loading)
    ${CF_ROOT}/drivers/zephyr/zephyr.c
    ${CF_ROOT}/drivers/can_zephyr/can_zephyr.c
    ${CF_ROOT}/drivers/timers_zephyr/timers_zephyr.c
  )
  zephyr_library_sources_ifdef(CONFIG_CANFESTIVAL_ENABLE_LSS ${CF_ROOT}/src/lss.c)

  zephyr_library_compile_definitions(NOT_USE_DYNAMIC_LOADING)
  zephyr_library_compile_definitions_ifdef(CONFIG_CANFESTIVAL_ENABLE_LSS CO_ENABLE_LSS)

  set(CF_INCLUDE_DIRS
    ${CF_ROOT}/include
    ${CF_ROOT}/include/zephyr          # applicfg.h, canfestival.h
    ${CF_ROOT}/include/timers_zephyr   # timerscfg.h
    ${CF_ROOT}/drivers/zephyr
    ${CMAKE_CURRENT_BINARY_DIR}/include # generated config.h
  )

  # Make the headers visible to the CanFestival sources and to the application
  # (so PLC/runtime glue can call canOpen/setState/... against the stack).
  zephyr_library_include_directories(${CF_INCLUDE_DIRS})
  zephyr_include_directories(${CF_INCLUDE_DIRS})

endif()
