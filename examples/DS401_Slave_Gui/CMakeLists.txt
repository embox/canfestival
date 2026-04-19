canfestival_require_dcfs(DS401_Slave_Gui ObjDict)
if(DS401_Slave_Gui_SKIP)
    return()
endif()

find_package(wxWidgets COMPONENTS core base)
if(NOT wxWidgets_FOUND)
    message(STATUS "Skipping example DS401_Slave_Gui: wxWidgets not found")
    return()
endif()

enable_language(CXX)
include(${wxWidgets_USE_FILE})

set(GENERATED_SOURCES "")
canfestival_generate_od(GENERATED_SOURCES ObjDict)

add_executable(DS401_Slave_Gui
    main.cpp
    TestSlaveGui.cpp
    CallBack.cpp
    ${GENERATED_SOURCES})

if(WIN32)
    target_sources(DS401_Slave_Gui PRIVATE getopt.c)
endif()

target_include_directories(DS401_Slave_Gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(DS401_Slave_Gui PRIVATE
    canfestival
    canfestival_${CF_TARGET}
    ${wxWidgets_LIBRARIES})
