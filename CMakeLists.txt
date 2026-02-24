cmake_minimum_required(VERSION 3.22)
project(mip_sdk_appnotes)

set(CMAKE_CXX_STANDARD 17)

#
# Configure the MIP SDK as needed for these demos.
#

# In particular, turn on full logging support for debugging.
# This causes the MIP SDK to compile with logging support built in, but
# does not automatically enable it for downstream projects like these (see below).
set(MICROSTRAIN_LOGGING_MAX_LEVEL MICROSTRAIN_LOGGING_LEVEL_TRACE_ CACHE INTERNAL "" FORCE)
set(MIP_LOGGING_MAX_LEVEL         MICROSTRAIN_LOGGING_LEVEL_TRACE_ CACHE INTERNAL "" FORCE)

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


if(MIP_SDK_DIR)
    # If the user has defined MIP_SDK_DIR, then use the mip sdk at that path.
    # The binary directory must be specified when the path is outside this source tree.
    add_subdirectory(${MIP_SDK_DIR} ${CMAKE_CURRENT_BINARY_DIR}/MIP_SDK)

else()
    # MIP SDK not specified, so download it from github.

    include(FetchContent)

    FetchContent_Declare(
        mip_sdk
        GIT_REPOSITORY "https://github.com/HBK-MicroStrain/mip_sdk.git"
        GIT_TAG 92d90f61e567d71eff778b5a54d34eeff13b6a02  # develop, Feb 2026
    )

    FetchContent_MakeAvailable(mip_sdk)
endif()

# Enable microstrain logging in all subprojects.
# This is required to enable logging in these projects vs the mip sdk itself.
# Otherwise, these projects won't enable logging even if the mip sdk is compiled with it enabled.
add_compile_definitions("MICROSTRAIN_LOGGING_MAX_LEVEL=MICROSTRAIN_LOGGING_LEVEL_TRACE_")
add_compile_definitions("MIP_LOGGING_MAX_LEVEL=MICROSTRAIN_LOGGING_LEVEL_TRACE_")

# Register all of the subprojects.
add_subdirectory(estop)
add_subdirectory(gpio_gated_streaming)
add_subdirectory(synchronized_data_output)
add_subdirectory(square_wave_output)
