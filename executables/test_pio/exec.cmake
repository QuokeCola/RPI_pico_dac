add_executable(pwm ${CMAKE_CURRENT_LIST_DIR}/main.cpp)
pico_generate_pio_header(pwm ${CMAKE_CURRENT_LIST_DIR}/pwm.pio)

target_link_libraries(pwm hardware_dma hardware_pio)