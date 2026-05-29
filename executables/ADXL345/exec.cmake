add_executable(adxl345 ${CMAKE_CURRENT_LIST_DIR}/main.cpp)
target_link_libraries(adxl345
        hardware_spi)
pico_enable_stdio_usb(adxl345 1)
pico_enable_stdio_uart(adxl345 0)
