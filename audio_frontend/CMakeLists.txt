cmake_minimum_required(VERSION 3.20)

project(signlang_eyes_edgeai_audio_frontend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)


find_package(ALSA REQUIRED)

add_executable(signlang_eyes_edgeai_audio_frontend
        main.cpp)

target_link_libraries(signlang_eyes_edgeai_audio_frontend PRIVATE
        cxxopts::cxxopts
        iceoryx2_cxx
        ALSA::ALSA)
