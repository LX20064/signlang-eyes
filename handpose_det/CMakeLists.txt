cmake_minimum_required(VERSION 3.20)

project(signlang_eyes_edgeai_handpose_det LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(signlang_eyes_edgeai_handpose_det
        program_options.cpp
        main.cpp)

target_include_directories(signlang_eyes_edgeai_handpose_det PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR})

target_link_libraries(signlang_eyes_edgeai_handpose_det PRIVATE
        cxxopts::cxxopts
        iceoryx2-cxx::static-lib-cxx)
