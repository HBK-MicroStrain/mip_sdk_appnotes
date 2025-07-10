
add_executable(SquareWaveOutput main.cpp)

target_link_libraries(SquareWaveOutput PUBLIC ${MIP_LIBRARIES} ${MICROSTRAIN_LIBRARIES})
