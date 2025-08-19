
add_executable(SynchronizedDataOutput main.cpp)

target_link_libraries(SynchronizedDataOutput PUBLIC ${MIP_LIBRARIES} ${MICROSTRAIN_LIBRARIES})
