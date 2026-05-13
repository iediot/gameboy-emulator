cmake_minimum_required(VERSION 4.1)
project(gameboy_emu)

set(CMAKE_CXX_STANDARD 20)

add_executable(gameboy_emu main.cpp
        cpu.cpp
        cpu.h
        memory.cpp
        memory.h
        ppu.cpp
        ppu.h)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
target_link_libraries(gameboy_emu PRIVATE SDL2::SDL2 SDL2_image::SDL2_image)
