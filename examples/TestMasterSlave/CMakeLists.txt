canfestival_require_dcfs(TestMasterSlave TestMaster TestSlave)
if(TestMasterSlave_SKIP)
    return()
endif()

set(GENERATED_SOURCES "")
canfestival_generate_od(GENERATED_SOURCES TestMaster)
canfestival_generate_od(GENERATED_SOURCES TestSlave)

add_executable(TestMasterSlave
    TestMasterSlave.c
    Master.c
    Slave.c
    getopt.c
    ${GENERATED_SOURCES})

target_include_directories(TestMasterSlave PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(TestMasterSlave PRIVATE
    canfestival
    canfestival_${CF_TARGET})
