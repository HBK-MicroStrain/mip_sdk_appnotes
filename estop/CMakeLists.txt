
add_executable(EStopDemo main.cpp)

target_link_libraries(EStopDemo PUBLIC ${MIP_LIBRARIES} ${MICROSTRAIN_LIBRARIES})
