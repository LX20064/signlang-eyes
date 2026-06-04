cmake_minimum_required(VERSION 3.20)

project(signlang_eyes_edgeai_speech_asr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(signlang_eyes_edgeai_speech_asr
        audio_ring_buffer.cpp
        iceoryx_gateway.cpp
        program_options.cpp
        speech_asr_result.cpp
        main.cpp)

target_include_directories(signlang_eyes_edgeai_speech_asr PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/third_party/rknn_runtime/include)

target_link_libraries(signlang_eyes_edgeai_speech_asr PRIVATE
        cxxopts::cxxopts
        iceoryx2-cxx::static-lib-cxx
        Threads::Threads)

install(TARGETS signlang_eyes_edgeai_speech_asr DESTINATION .)
