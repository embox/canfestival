# ---------------------------------------------------------------------------
#  Timer driver (OBJECT library, linked into platform driver)
# ---------------------------------------------------------------------------
if(CF_TIMERS_DRIVER)
    set(TIMERS_DIR timers_${CF_TIMERS_DRIVER})
    set(TIMERS_SRC ${TIMERS_DIR}/${TIMERS_DIR}.c)

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TIMERS_SRC})
        add_library(canfestival_timers OBJECT ${TIMERS_SRC})
        target_include_directories(canfestival_timers PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/${CF_TARGET}
            ${CMAKE_SOURCE_DIR}/include/${TIMERS_DIR}
            ${CMAKE_SOURCE_DIR}/drivers/${CF_TARGET}
            ${CMAKE_BINARY_DIR}/include)

        if(CF_TIMERS_DRIVER STREQUAL "xeno" AND XENO_CFLAGS)
            target_compile_options(canfestival_timers PRIVATE ${XENO_CFLAGS})
            target_compile_definitions(canfestival_timers PRIVATE USE_XENO)
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
#  CAN driver(s) (SHARED or OBJECT depending on CF_ENABLE_DLL_DRIVERS)
#
#  CF_CAN_DRIVER may name several drivers (e.g. "virtual;socket"); one shared
#  library is built per driver, so a single build can ship more than one CAN
#  backend. Static (non-DLL) builds link the driver into the platform library
#  and therefore support a single driver only.
# ---------------------------------------------------------------------------
list(LENGTH CF_CAN_DRIVER _can_driver_count)
if(NOT CF_ENABLE_DLL_DRIVERS AND _can_driver_count GREATER 1)
    message(FATAL_ERROR
        "CF_ENABLE_DLL_DRIVERS=OFF supports a single CF_CAN_DRIVER, got: ${CF_CAN_DRIVER}")
endif()

foreach(_can_driver IN LISTS CF_CAN_DRIVER)
    set(CAN_DIR can_${_can_driver})

    # Find the source file (naming varies)
    file(GLOB CAN_SRC_FILES ${CAN_DIR}/*.c)
    # Exclude hub utility if present (can_serial has can_serial_hub.c)
    list(FILTER CAN_SRC_FILES EXCLUDE REGEX "_hub\\.c$")

    if(CAN_SRC_FILES)
        if(CF_ENABLE_DLL_DRIVERS)
            add_library(canfestival_${CAN_DIR} SHARED ${CAN_SRC_FILES})
        else()
            add_library(canfestival_${CAN_DIR} OBJECT ${CAN_SRC_FILES})
        endif()

        target_include_directories(canfestival_${CAN_DIR} PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/${CF_TARGET}
            ${CMAKE_SOURCE_DIR}/include/timers_${CF_TIMERS_DRIVER}
            ${CMAKE_SOURCE_DIR}/drivers/${CF_TARGET}
            ${CMAKE_BINARY_DIR}/include)

        # Driver-specific link libraries
        if(_can_driver STREQUAL "peak_linux")
            target_link_libraries(canfestival_${CAN_DIR} PRIVATE pcan)
        elseif(_can_driver STREQUAL "anagate_linux")
            target_link_libraries(canfestival_${CAN_DIR} PRIVATE AnaGateAPIDLL AnaCommon)
        elseif(_can_driver STREQUAL "kvaser")
            target_link_libraries(canfestival_${CAN_DIR} PRIVATE canlib)
        elseif(_can_driver STREQUAL "socket")
            # socket CAN uses standard socket API, no extra libs needed
        endif()

        if(CF_ENABLE_DLL_DRIVERS)
            install(TARGETS canfestival_${CAN_DIR} LIBRARY DESTINATION lib)
        endif()
    endif()
endforeach()

# ---------------------------------------------------------------------------
#  Platform driver (STATIC library combining platform + timers)
# ---------------------------------------------------------------------------
if(CF_TARGET AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${CF_TARGET}/${CF_TARGET}.c)
    set(PLATFORM_SOURCES ${CF_TARGET}/${CF_TARGET}.c)

    add_library(canfestival_${CF_TARGET} STATIC ${PLATFORM_SOURCES})

    target_include_directories(canfestival_${CF_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/${CF_TARGET}
        ${CMAKE_SOURCE_DIR}/include/timers_${CF_TIMERS_DRIVER}
        ${CMAKE_SOURCE_DIR}/drivers/${CF_TARGET}
        ${CMAKE_BINARY_DIR}/include)

    # Link timers object into platform library
    if(TARGET canfestival_timers)
        target_sources(canfestival_${CF_TARGET} PRIVATE $<TARGET_OBJECTS:canfestival_timers>)
    endif()

    # Link CAN driver object into platform library when not using DLL
    if(NOT CF_ENABLE_DLL_DRIVERS AND TARGET canfestival_${CAN_DIR})
        target_sources(canfestival_${CF_TARGET} PRIVATE $<TARGET_OBJECTS:canfestival_${CAN_DIR}>)
    endif()

    # Platform-specific link flags
    if(CF_TARGET STREQUAL "unix")
        find_package(Threads REQUIRED)
        target_link_libraries(canfestival_${CF_TARGET} PUBLIC Threads::Threads)
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "CYGWIN")
            target_link_libraries(canfestival_${CF_TARGET} PUBLIC rt)
        endif()
        if(CF_ENABLE_DLL_DRIVERS)
            target_link_libraries(canfestival_${CF_TARGET} PUBLIC dl)
        endif()
    endif()

    if(CF_TIMERS_DRIVER STREQUAL "xeno" AND XENO_LDFLAGS)
        target_link_options(canfestival_${CF_TARGET} PUBLIC ${XENO_LDFLAGS})
    endif()

    install(TARGETS canfestival_${CF_TARGET} ARCHIVE DESTINATION lib)
endif()
