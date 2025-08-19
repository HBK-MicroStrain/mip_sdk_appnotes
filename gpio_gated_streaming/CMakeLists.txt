
add_executable(GpioGatedStreaming main.cpp)

target_link_libraries(GpioGatedStreaming PUBLIC ${MIP_LIBRARIES} ${MICROSTRAIN_LIBRARIES})
