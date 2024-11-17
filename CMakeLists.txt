cmake_minimum_required(VERSION 3.21)

project(
    audio_man
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS Off)

add_subdirectory(libs/miniz EXCLUDE_FROM_ALL)

add_executable(
    audio_man

    main.cpp

    audio_man/audio_man.cpp
    audio_man/audio_man.hpp

    audio_man/private/audio_man_impl.cpp
    audio_man/private/audio_man_impl.hpp

    libs/miniaudio/miniaudio.h
    libs/miniz/miniz.h
    libs/miniz/miniz.c
)

target_include_directories(
    audio_man
    PUBLIC libs/
    PUBLIC audio_man/
)

target_link_libraries(audio_man miniz)
