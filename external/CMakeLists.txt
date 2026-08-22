add_library(gb_sdl INTERFACE)
add_library(gb::sdl ALIAS gb_sdl)

if(GB_ANDROID)
    # sdl ships shared on android because the java side loads it by name, GBActivity
    # lists SDL2 and SDL2_image ahead of main so they are in memory first
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    add_subdirectory(SDL)

    # stb backend decodes png with no external libpng dependency, sdl_image stays shared
    # here because a static build makes it look for a system sdl2 instead of the in tree one
    set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
    add_subdirectory(SDL_image)

    target_link_libraries(gb_sdl INTERFACE SDL2::SDL2 SDL2::SDL2main SDL2_image::SDL2_image)
elseif(GB_IOS)
    # sdl is built statically from source, brew only ships desktop libs
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    add_subdirectory(SDL)

    # stb backend decodes png with no external libpng dependency
    # imageio backend is off, its apple frameworks don't link into a static lib
    set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_BACKEND_IMAGEIO OFF CACHE BOOL "" FORCE)
    add_subdirectory(SDL_image)

    target_link_libraries(gb_sdl INTERFACE SDL2::SDL2main SDL2::SDL2-static SDL2_image::SDL2_image-static)
else()
    find_package(SDL2 REQUIRED)
    find_package(SDL2_image REQUIRED)
    target_link_libraries(gb_sdl INTERFACE SDL2::SDL2 SDL2_image::SDL2_image)

    # native file dialog backs the 'add game' button, desktop only
    add_subdirectory(nativefiledialog-extended)
endif()

add_library(gb_imgui STATIC
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_sdl2.cpp
        imgui/backends/imgui_impl_sdlrenderer2.cpp)
add_library(gb::imgui ALIAS gb_imgui)

target_include_directories(gb_imgui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends)
target_link_libraries(gb_imgui PUBLIC gb::sdl)
