set(CF_CORE_SOURCES
    dcf.c
    emcy.c
    lifegrd.c
    nmtMaster.c
    nmtSlave.c
    objacces.c
    pdo.c
    sdo.c
    states.c
    sync.c
    timer.c
)

if(CF_ENABLE_LSS)
    list(APPEND CF_CORE_SOURCES lss.c)
endif()

add_library(canfestival STATIC ${CF_CORE_SOURCES})

target_include_directories(canfestival PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/${CF_TARGET}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/drivers/${CF_TARGET}>
)

if(CF_ENABLE_LSS)
    target_compile_definitions(canfestival PUBLIC CO_ENABLE_LSS)
    if(CF_ENABLE_LSS_FS)
        target_compile_definitions(canfestival PUBLIC CO_ENABLE_LSS_FS)
    endif()
endif()

if(NOT CF_ENABLE_DLL_DRIVERS)
    target_compile_definitions(canfestival PUBLIC NOT_USE_DYNAMIC_LOADING)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    set_target_properties(canfestival PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

install(TARGETS canfestival ARCHIVE DESTINATION lib)
