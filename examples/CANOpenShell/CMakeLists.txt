canfestival_require_dcfs(CANOpenShell CANOpenShellMasterOD CANOpenShellSlaveOD)
if(CANOpenShell_SKIP)
    return()
endif()

set(GENERATED_SOURCES "")
canfestival_generate_od(GENERATED_SOURCES CANOpenShellMasterOD)
canfestival_generate_od(GENERATED_SOURCES CANOpenShellSlaveOD)

add_executable(CANOpenShell
    CANOpenShell.c
    ${GENERATED_SOURCES})

target_include_directories(CANOpenShell PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(CANOpenShell PRIVATE
    canfestival
    canfestival_${CF_TARGET})
