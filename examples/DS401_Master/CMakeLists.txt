canfestival_require_dcfs(DS401_Master TestMaster)
if(DS401_Master_SKIP)
    return()
endif()

set(GENERATED_SOURCES "")
canfestival_generate_od(GENERATED_SOURCES TestMaster)

add_executable(DS401_Master
    TestMasterMicroMod.c
    getopt.c
    ${GENERATED_SOURCES})

target_include_directories(DS401_Master PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(DS401_Master PRIVATE
    canfestival
    canfestival_${CF_TARGET})
