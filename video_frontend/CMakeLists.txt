cmake_minimum_required(VERSION 3.20)

project(signlang_eyes_edgeai_video_frontend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(signlang_eyes_edgeai_video_frontend
        program_options.cpp
        v4l2_capture_device.cpp
        video_processor.cpp
        video_publisher.cpp
        main.cpp)

target_include_directories(signlang_eyes_edgeai_video_frontend PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(signlang_eyes_edgeai_video_frontend PRIVATE
        cxxopts::cxxopts
        iceoryx2-cxx::static-lib-cxx)

add_executable(signlang_eyes_edgeai_video_processor_test
        video_processor.cpp
        video_processor_test.cpp)

target_include_directories(signlang_eyes_edgeai_video_processor_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(signlang_eyes_edgeai_video_processor_test PRIVATE
        iceoryx2-cxx::static-lib-cxx)
