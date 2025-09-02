cmake_minimum_required(VERSION 3.22)
project(mip_sdk_appnotes)

set(CMAKE_CXX_STANDARD 17)

set(MICROSTRAIN_LOGGING_MAX_LEVEL MICROSTRAIN_LOG_LEVEL_TRACE CACHE INTERNAL "" FORCE )
set(MICROSTRAIN_ENABLE_LOGGING      ON  CACHE INTERNAL "" FORCE)
set(MICROSTRAIN_ENABLE_EXTRAS       ON  CACHE INTERNAL "" FORCE)
set(MICROSTRAIN_ENABLE_SERIAL       ON  CACHE INTERNAL "" FORCE)
set(MICROSTRAIN_ENABLE_TCP          OFF CACHE INTERNAL "" FORCE)
set(MICROSTRAIN_BUILD_PACKAGE       OFF CACHE INTERNAL "" FORCE)
set(MICROSTRAIN_BUILD_EXAMPLES      OFF CACHE INTERNAL "" FORCE)
set(MICROSTRAIN_BUILD_TESTS         OFF CACHE INTERNAL "" FORCE)
set(MICROSTRAIN_BUILD_DOCUMENTATION OFF CACHE INTERNAL "" FORCE)
set(MICROSTRAIN_CMAKE_DEBUG         OFF CACHE INTERNAL "" FORCE)
set(MIP_ENABLE_DIAGNOSTICS          OFF CACHE INTERNAL "" FORCE)
set(MIP_ENABLE_EXTRAS               OFF CACHE INTERNAL "" FORCE)

include(FetchContent)

FetchContent_Declare(
    mip_sdk
    GIT_REPOSITORY "https://github.com/LORD-MicroStrain/mip_sdk.git"
    GIT_TAG 6df21bd53dc143ffe7140b0b0686801971b8ff7b  # develop
)

FetchContent_MakeAvailable(mip_sdk)

add_subdirectory(estop)
add_subdirectory(gpio_gated_streaming)
add_subdirectory(synchronized_data_output)
add_subdirectory(square_wave_output)
