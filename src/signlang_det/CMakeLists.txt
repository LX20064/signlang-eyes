add_executable(signlang_eyes_edgeai_signlang_det
    signlang_result.cpp
    main.cpp)

target_include_directories(signlang_eyes_edgeai_signlang_det PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(signlang_eyes_edgeai_signlang_det PRIVATE
    cxxopts::cxxopts)
