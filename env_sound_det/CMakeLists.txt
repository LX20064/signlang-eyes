cmake_minimum_required(VERSION 3.20)

project(signlang_eyes_edgeai_env_sound_det LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ENV_SOUND_DET_RUNTIME_ROOT ${CMAKE_SOURCE_DIR}/third_party/rknn_runtime)
set(ENV_SOUND_DET_RKNN_INCLUDE_DIR ${ENV_SOUND_DET_RUNTIME_ROOT}/include)
set(ENV_SOUND_DET_RKNN_LIBRARY ${ENV_SOUND_DET_RUNTIME_ROOT}/lib/aarch64/librknnrt.so)
set(ENV_SOUND_DET_MODEL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/model)
set(ENV_SOUND_DET_MODEL_FILE ${ENV_SOUND_DET_MODEL_DIR}/yamnet_3s.rknn)
set(ENV_SOUND_DET_CLASS_MAP_FILE ${ENV_SOUND_DET_MODEL_DIR}/yamnet_class_map.txt)

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    message(STATUS "Skipping signlang_eyes_edgeai_env_sound_det: RKNN runtime is project-local aarch64 only")
    return()
endif()

foreach(required_file
        ${ENV_SOUND_DET_RKNN_INCLUDE_DIR}/rknn_api.h
        ${ENV_SOUND_DET_RKNN_LIBRARY}
        ${ENV_SOUND_DET_MODEL_FILE}
        ${ENV_SOUND_DET_CLASS_MAP_FILE})
    if(NOT EXISTS ${required_file})
        message(FATAL_ERROR "Missing required environment sound detector asset: ${required_file}")
    endif()
endforeach()

add_library(rknnrt SHARED IMPORTED GLOBAL)
set_target_properties(rknnrt PROPERTIES
        IMPORTED_LOCATION ${ENV_SOUND_DET_RKNN_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${ENV_SOUND_DET_RKNN_INCLUDE_DIR})

find_package(Threads REQUIRED)

add_executable(signlang_eyes_edgeai_env_sound_det
        audio_ring_buffer.cpp
        iceoryx_gateway.cpp
        program_options.cpp
        yamnet_model.cpp
        main.cpp)

target_include_directories(signlang_eyes_edgeai_env_sound_det PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(signlang_eyes_edgeai_env_sound_det PRIVATE
        cxxopts::cxxopts
        iceoryx2-cxx::static-lib-cxx
        rknnrt
        Threads::Threads
        dl)

set_target_properties(signlang_eyes_edgeai_env_sound_det PROPERTIES
        BUILD_RPATH ${ENV_SOUND_DET_RUNTIME_ROOT}/lib/aarch64
        INSTALL_RPATH "$ORIGIN/lib")

add_custom_command(TARGET signlang_eyes_edgeai_env_sound_det POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:signlang_eyes_edgeai_env_sound_det>/model"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ENV_SOUND_DET_MODEL_FILE}
        "$<TARGET_FILE_DIR:signlang_eyes_edgeai_env_sound_det>/model/yamnet_3s.rknn"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ENV_SOUND_DET_CLASS_MAP_FILE}
        "$<TARGET_FILE_DIR:signlang_eyes_edgeai_env_sound_det>/model/yamnet_class_map.txt")

install(TARGETS signlang_eyes_edgeai_env_sound_det DESTINATION .)
install(FILES ${ENV_SOUND_DET_RKNN_LIBRARY} DESTINATION lib)
install(FILES
        ${ENV_SOUND_DET_MODEL_FILE}
        ${ENV_SOUND_DET_CLASS_MAP_FILE}
        DESTINATION model)
