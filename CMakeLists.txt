cmake_minimum_required(VERSION 4.0)
include(toolchain.cmake)

project(RPI_pico_dac ASM C CXX)

set(CMAKE_CXX_STANDARD 20)


include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")
include("$ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake")


# If you want debug output from USB (pass -DPICO_STDIO_USB=1) this ensures you don't lose any debug output while USB is set up
if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
    set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)
endif()

# Allow using the example keys, as this is the examples repository
if (NOT DEFINED PICO_ALLOW_EXAMPLE_KEYS)
    set(PICO_ALLOW_EXAMPLE_KEYS 1)
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

add_executable(RPI_pico_dac main.cpp)
# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(RPI_pico_dac pico_stdlib)
