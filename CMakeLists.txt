cmake_minimum_required(VERSION 4.0)
# Set toolchain
set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc     CACHE STRING "" FORCE)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc     CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++     CACHE STRING "" FORCE)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy CACHE STRING "" FORCE)
set(CMAKE_SIZE_UTILITY ${TOOLCHAIN_PREFIX}size    CACHE STRING "" FORCE)

# Include Pico SDK
set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/libs/pico-sdk")
set(PICO_EXTRAS_PATH "${CMAKE_SOURCE_DIR}/libs/pico-extras")

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
include("${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake")

project(RPI_pico_dac ASM C CXX)

set(CMAKE_CXX_STANDARD 20)
# Init Pico SDK
# If you want debug output from USB (pass -DPICO_STDIO_USB=1) this ensures you don't lose any debug output while USB is set up
if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
    set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)
endif()

# Initialize the SDK
pico_sdk_init()

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
)
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif()

# Find the dependency targets
get_directory_property(LIB_TARGETS BUILDSYSTEM_TARGETS)

# Include all executables
file(GLOB_RECURSE ALL_TARGETS ${PROJECT_SOURCE_DIR} "${PROJECT_SOURCE_DIR}/executables/**/exec.cmake")
FOREACH(TARGET ${ALL_TARGETS})
    include(${TARGET})
ENDFOREACH()

set(ELF_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build)
set(ELF_OUTPUT_NAME firmware)
add_definitions(-DBUILD_TARGET_NAME="$<TARGET_PROPERTY:NAME>")

# Get all targets
get_directory_property(TARGET_LIST BUILDSYSTEM_TARGETS)

# Setup all executables targets
foreach (exec_target ${TARGET_LIST})
    list(FIND LIB_TARGETS ${exec_target} idx)
    if(idx EQUAL -1 AND NOT(${exec_target} MATCHES "_pio_h"))
        message("Setting up ${exec_target}.")

        # Set unique output file
        set_target_properties(${exec_target} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY ${ELF_OUTPUT_PATH}
                ARCHIVE_OUTPUT_DIRECTORY ${ELF_OUTPUT_PATH}
                OUTPUT_NAME ${ELF_OUTPUT_NAME})  #

        # Add pico_stdlib library which aggregates commonly used features
        target_link_libraries(${exec_target} pico_stdlib)

        if (PICO_CYW43_SUPPORTED)
            target_link_libraries(${exec_target} pico_cyw43_arch_none)
        endif()

        # create map/bin/hex file etc.
        pico_add_extra_outputs(${exec_target})

        # Run size utility after link to show the size of elf file
        add_custom_command(TARGET ${exec_target} POST_BUILD
                COMMAND echo
                COMMAND ${CMAKE_SIZE_UTILITY} ${ELF_OUTPUT_PATH}/${ELF_OUTPUT_NAME}.elf
                COMMAND echo)  # execute size utility to show size of executable file

    endif ()
endforeach (exec_target)