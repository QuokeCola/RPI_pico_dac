add_executable(dac ${CMAKE_CURRENT_LIST_DIR}/main.cpp)
target_link_libraries(dac hardware_dma hardware_pio)