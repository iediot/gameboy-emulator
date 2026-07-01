cmake_minimum_required(VERSION 4.1)

set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment version")

project(gameboy_emu)

set(CMAKE_CXX_STANDARD 20)

add_executable(gameboy_emu main.cpp
        cpu.cpp
        cpu.h
        memory.cpp
        memory.h
        ppu.cpp
        ppu.h
        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/backends/imgui_impl_sdl2.cpp
        external/imgui/backends/imgui_impl_sdlrenderer2.cpp
        app.cpp
        app.h)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
target_link_libraries(gameboy_emu PRIVATE SDL2::SDL2 SDL2_image::SDL2_image)
target_include_directories(gameboy_emu PRIVATE external/imgui external/imgui/backends)
add_subdirectory(external/nativefiledialog-extended)
target_link_libraries(gameboy_emu PRIVATE nfd)
