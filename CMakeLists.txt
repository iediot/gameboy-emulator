cmake_minimum_required(VERSION 3.20)

set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment version")

project(gameboy_emu CXX C)

set(CMAKE_CXX_STANDARD 20)

# ios is selected with -DCMAKE_SYSTEM_NAME=iOS, everything else is desktop
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(GB_IOS TRUE)
endif()

# emulator core plus imgui, shared by every platform
set(GB_SOURCES main.cpp
        cpu.cpp
        cpu.h
        memory.cpp
        memory.h
        ppu.cpp
        ppu.h
        platform.h
        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/backends/imgui_impl_sdl2.cpp
        external/imgui/backends/imgui_impl_sdlrenderer2.cpp
        app.cpp
        app.h
        ios_ui.cpp)

if(GB_IOS)
    # roms, covers and the bezel are staged into the app bundle, read back at runtime via SDL_GetBasePath
    set(GB_RESOURCES ${CMAKE_SOURCE_DIR}/sprites/emu-sprite.png
            ${CMAKE_SOURCE_DIR}/artworks
            ${CMAKE_SOURCE_DIR}/roms/game-roms)

    # asset catalog holding the app icon, xcode runs actool on it during the build
    set(GB_ASSETS ${CMAKE_SOURCE_DIR}/ios/Assets.xcassets)

    add_executable(gameboy_emu MACOSX_BUNDLE ${GB_SOURCES} ios_import.mm ${GB_RESOURCES} ${GB_ASSETS})
    set_source_files_properties(${GB_RESOURCES} ${GB_ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # sdl is built statically from source, brew only ships desktop libs
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    add_subdirectory(external/SDL)

    # stb backend decodes png with no external libpng dependency
    # imageio backend is off, its apple frameworks don't link into a static lib
    set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_BACKEND_IMAGEIO OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/SDL_image)

    target_link_libraries(gameboy_emu PRIVATE SDL2::SDL2main SDL2::SDL2-static SDL2_image::SDL2_image-static "-framework UIKit")

    set_target_properties(gameboy_emu PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/ios/Info.plist
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.iediot.gbemu"
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
            XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES")

    # the simulator needs no signing, a real device uses automatic signing so xcode can pick a team
    if(CMAKE_OSX_SYSROOT MATCHES "simulator")
        set_target_properties(gameboy_emu PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
                XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
    else()
        set_target_properties(gameboy_emu PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
        # optional, pass -DGB_DEV_TEAM=<team id> to preset the team instead of picking it in xcode
        if(GB_DEV_TEAM)
            set_target_properties(gameboy_emu PROPERTIES XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${GB_DEV_TEAM}")
        endif()
    endif()
else()
    add_executable(gameboy_emu ${GB_SOURCES})

    find_package(SDL2 REQUIRED)
    find_package(SDL2_image REQUIRED)
    target_link_libraries(gameboy_emu PRIVATE SDL2::SDL2 SDL2_image::SDL2_image)

    # native file dialog backs the 'add game' button, desktop only
    add_subdirectory(external/nativefiledialog-extended)
    target_link_libraries(gameboy_emu PRIVATE nfd)
endif()

target_include_directories(gameboy_emu PRIVATE external/imgui external/imgui/backends)
