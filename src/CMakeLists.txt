# emulator core plus the sdl frontend, shared by every platform
set(GB_SOURCES
        main.cpp
        platform.h
        core/cpu.cpp
        core/cpu.h
        core/memory.cpp
        core/memory.h
        core/ppu.cpp
        core/ppu.h
        core/apu.cpp
        core/apu.h
        frontend/app.cpp
        frontend/app.h
        frontend/glass.h
        frontend/mobile_ui.cpp
        platform/android_import.cpp)

if(GB_ANDROID)
    # sdlactivity dlopens libSDL2.so then libmain.so, so the app has to be a shared lib
    # with an entry point named SDL_main, not an executable
    add_library(gameboy_emu SHARED ${GB_SOURCES})
    set_target_properties(gameboy_emu PROPERTIES OUTPUT_NAME "main")

    target_link_libraries(gameboy_emu PRIVATE GLESv2 log android)
elseif(GB_IOS)
    # roms, covers and the bezel are staged into the app bundle, read back at runtime via SDL_GetBasePath
    set(GB_RESOURCES ${GB_ASSETS_DIR}/sprites/cartridge.png
            ${GB_ASSETS_DIR}/artworks
            ${GB_ROMS_DIR}/game-roms)

    # asset catalog holding the app icon, xcode runs actool on it during the build
    set(GB_ICON_ASSETS ${GB_ROOT_DIR}/ios/Assets.xcassets)

    add_executable(gameboy_emu MACOSX_BUNDLE ${GB_SOURCES} platform/ios_import.mm
            ${GB_RESOURCES} ${GB_ICON_ASSETS})
    set_source_files_properties(${GB_RESOURCES} ${GB_ICON_ASSETS}
            PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    target_link_libraries(gameboy_emu PRIVATE "-framework UIKit")

    # xcode builds a debug configuration at -O0, which cannot emulate at full speed on a
    # phone, so the debug build is optimised too and only keeps its symbols
    set_target_properties(gameboy_emu PROPERTIES
            "XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL[variant=Debug]" "2"
            MACOSX_BUNDLE_INFO_PLIST ${GB_ROOT_DIR}/ios/Info.plist
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
    if(APPLE)
        # a real .app so the dock shows a proper icon instead of a raw image, and so the
        # thing is double clickable, assets ride along in Contents/Resources
        set(GB_MAC_ASSETS ${GB_ROOT_DIR}/mac/AppIcon.icns
                ${GB_ASSETS_DIR}/sprites/cartridge.png
                ${GB_ASSETS_DIR}/sprites/icon-mac-light.png
                ${GB_ASSETS_DIR}/sprites/icon-mac-dark.png
                ${GB_ASSETS_DIR}/artworks
                ${GB_ROMS_DIR}/game-roms)
        add_executable(gameboy_emu MACOSX_BUNDLE ${GB_SOURCES} ${GB_MAC_ASSETS})
        set_source_files_properties(${GB_MAC_ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        set_target_properties(gameboy_emu PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST ${GB_ROOT_DIR}/mac/Info.plist)
    else()
        add_executable(gameboy_emu ${GB_SOURCES})
    endif()

    target_link_libraries(gameboy_emu PRIVATE nfd)

    # corefoundation reads the macos light/dark setting to pick the window icon
    if(APPLE)
        target_link_libraries(gameboy_emu PRIVATE "-framework CoreFoundation")
    endif()
endif()

# android is the exception, sdlactivity looks for a library literally named main
if(NOT GB_ANDROID)
    set_target_properties(gameboy_emu PROPERTIES
            OUTPUT_NAME "gameboy-emu"
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

target_link_libraries(gameboy_emu PRIVATE gb::sdl gb::imgui)
target_include_directories(gameboy_emu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
