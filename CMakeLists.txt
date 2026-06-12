cmake_minimum_required(VERSION 3.20)

project(signlang_eyes_edgeai LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Disable PIE to allow linking with prebuilt FFTW library (which was not compiled with -fPIC)
# set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-pie")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-pie")

option(SIGNLANG_ENABLE_TESTS "Build signlang-eyes-edgeai test executables" ON)

if(SIGNLANG_ENABLE_TESTS)
    enable_testing()
endif()

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Do not update already-populated FetchContent dependencies")

set(BUILD_TESTING OFF CACHE BOOL "Disable third-party dependency tests" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable third-party dependency examples" FORCE)
set(IOX2_CXX_STD_VERSION 20 CACHE STRING "C++ standard used by iceoryx2" FORCE)

set(SIGNLANG_MODEL_ROOT
        ${CMAKE_CURRENT_SOURCE_DIR}/models
        CACHE PATH "Local root directory for model assets")

if(NOT TARGET cxxopts)
    FetchContent_Declare(
            cxxopts
            GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
            GIT_TAG v3.3.1
            GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(cxxopts)
endif()

if(NOT TARGET iceoryx2-cxx)

    FetchContent_Declare(
            iceoryx2
            GIT_REPOSITORY https://github.com/eclipse-iceoryx/iceoryx2.git
            GIT_TAG 2e64dd8c18d7abe2065760d3d31addfedcd17161
    )

    FetchContent_MakeAvailable(iceoryx2)

endif()

find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)

set(SIGNLANG_IS_AARCH64 OFF)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(SIGNLANG_IS_AARCH64 ON)
endif()

enable_language(C)

# Use prebuilt FFTW from third_party
set(SIGNLANG_FFTW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fftw)
set(SIGNLANG_FFTW3F_INCLUDE_DIRS ${SIGNLANG_FFTW_ROOT}/include)

if(SIGNLANG_IS_AARCH64)
    set(SIGNLANG_FFTW3F_LIBRARY ${SIGNLANG_FFTW_ROOT}/lib/aarch64/libfftw3f.a)
else()
    message(FATAL_ERROR "FFTW prebuilt library only available for aarch64")
endif()

if(NOT EXISTS "${SIGNLANG_FFTW3F_LIBRARY}")
    message(FATAL_ERROR "FFTW library not found at ${SIGNLANG_FFTW3F_LIBRARY}")
endif()

if(NOT EXISTS "${SIGNLANG_FFTW3F_INCLUDE_DIRS}/fftw3.h")
    message(FATAL_ERROR "FFTW header not found at ${SIGNLANG_FFTW3F_INCLUDE_DIRS}/fftw3.h")
endif()

message(STATUS "Using prebuilt FFTW: ${SIGNLANG_FFTW3F_LIBRARY}")

if(NOT TARGET fftw3f)
    add_library(fftw3f STATIC IMPORTED GLOBAL)
    set_target_properties(fftw3f PROPERTIES
            IMPORTED_LOCATION ${SIGNLANG_FFTW3F_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${SIGNLANG_FFTW3F_INCLUDE_DIRS})
endif()

set(SIGNLANG_RKNN_RUNTIME_ROOT
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rknn_runtime
        CACHE PATH "Project-local RKNN runtime root")
set(SIGNLANG_RKNN_INCLUDE_DIR ${SIGNLANG_RKNN_RUNTIME_ROOT}/include)
set(SIGNLANG_RKNN_LIBRARY ${SIGNLANG_RKNN_RUNTIME_ROOT}/lib/aarch64/librknnrt.so)

add_library(signlang_rknn_headers INTERFACE)
target_include_directories(signlang_rknn_headers INTERFACE
        ${SIGNLANG_RKNN_INCLUDE_DIR})
add_library(signlang::rknn_headers ALIAS signlang_rknn_headers)

if(SIGNLANG_IS_AARCH64
        AND NOT TARGET signlang::rknn_runtime
        AND EXISTS "${SIGNLANG_RKNN_INCLUDE_DIR}/rknn_api.h"
        AND EXISTS "${SIGNLANG_RKNN_LIBRARY}")
    add_library(signlang_rknn_runtime SHARED IMPORTED GLOBAL)
    set_target_properties(signlang_rknn_runtime PROPERTIES
            IMPORTED_LOCATION ${SIGNLANG_RKNN_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${SIGNLANG_RKNN_INCLUDE_DIR})
    target_link_libraries(signlang_rknn_runtime INTERFACE
            signlang::rknn_headers)
    add_library(signlang::rknn_runtime ALIAS signlang_rknn_runtime)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/audio_frontend)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/speech_asr)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/env_sound_det)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/video_frontend)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/handpose_det)
