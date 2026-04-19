# Examples require a Python 3 interpreter to generate OD C code from DCF files
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(OBJDICTGEN ${CMAKE_SOURCE_DIR}/objdictgen/objdictgen.py)

# Helper: register a custom command to generate <name>.c and <name>.h from <name>.dcf
# Usage:  canfestival_generate_od(TARGET_VAR <name>)
#   - input  : ${CMAKE_CURRENT_SOURCE_DIR}/<name>.dcf
#   - output : ${CMAKE_CURRENT_BINARY_DIR}/<name>.c and <name>.h
# Appends generated sources to TARGET_VAR (a list variable in the caller).
function(canfestival_generate_od TARGET_VAR NAME)
    set(DCF ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.dcf)
    set(C_OUT ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.c)
    set(H_OUT ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.h)
    add_custom_command(
        OUTPUT ${C_OUT} ${H_OUT}
        COMMAND ${Python3_EXECUTABLE} ${OBJDICTGEN} ${DCF} ${C_OUT}
        DEPENDS ${DCF} ${OBJDICTGEN}
        COMMENT "Generating ${NAME}.c from ${NAME}.dcf"
        VERBATIM)
    list(APPEND ${TARGET_VAR} ${C_OUT})
    set(${TARGET_VAR} ${${TARGET_VAR}} PARENT_SCOPE)
endfunction()

# Skip an example with a status message if any of its .dcf files are missing.
# Usage:  canfestival_require_dcfs(<example_name> <dcf1> [dcf2 ...])
# Sets <example_name>_SKIP in parent scope if any is missing.
function(canfestival_require_dcfs EXAMPLE_NAME)
    foreach(DCF ${ARGN})
        if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${DCF}.dcf)
            message(STATUS "Skipping example ${EXAMPLE_NAME}: ${DCF}.dcf not found")
            set(${EXAMPLE_NAME}_SKIP TRUE PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

add_subdirectory(CANOpenShell)
add_subdirectory(DS401_Master)
add_subdirectory(DS401_Slave_Gui)
add_subdirectory(TestMasterSlave)
add_subdirectory(TestMasterSlaveLSS)
