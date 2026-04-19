canfestival_require_dcfs(TestMasterSlaveLSS TestMaster TestSlaveA TestSlaveB)
if(TestMasterSlaveLSS_SKIP)
    return()
endif()

if(NOT CF_ENABLE_LSS)
    message(STATUS "Skipping example TestMasterSlaveLSS: CF_ENABLE_LSS is OFF")
    return()
endif()

set(GENERATED_SOURCES "")
canfestival_generate_od(GENERATED_SOURCES TestMaster)
canfestival_generate_od(GENERATED_SOURCES TestSlaveA)
canfestival_generate_od(GENERATED_SOURCES TestSlaveB)

add_executable(TestMasterSlaveLSS
    TestMasterSlaveLSS.c
    Master.c
    SlaveA.c
    SlaveB.c
    getopt.c
    ${GENERATED_SOURCES})

target_include_directories(TestMasterSlaveLSS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(TestMasterSlaveLSS PRIVATE
    canfestival
    canfestival_${CF_TARGET})
