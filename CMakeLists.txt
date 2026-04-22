cmake_minimum_required(VERSION 4.1)
project(gameboy_emu)

set(CMAKE_CXX_STANDARD 20)

add_executable(gameboy_emu main.cpp
        cpu.cpp
        cpu.h
        memory.cpp
        memory.h)
