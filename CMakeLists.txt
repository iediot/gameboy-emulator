cmake_minimum_required(VERSION 3.20)

set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment version")

# one source of truth for the version, read by gradle too so no platform can drift
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version.txt GB_VERSION)
string(STRIP "${GB_VERSION}" GB_VERSION)

project(gameboy_emu VERSION ${GB_VERSION} LANGUAGES CXX C)

# a monotonic build number for the bundles, 1.2.3 -> 10203
string(REPLACE "." ";" GB_VER_PARTS "${GB_VERSION}")
list(GET GB_VER_PARTS 0 GB_VER_MAJOR)
list(GET GB_VER_PARTS 1 GB_VER_MINOR)
list(GET GB_VER_PARTS 2 GB_VER_PATCH)
math(EXPR GB_BUILD "${GB_VER_MAJOR} * 10000 + ${GB_VER_MINOR} * 100 + ${GB_VER_PATCH}")

set(CMAKE_CXX_STANDARD 20)

# an unoptimised build cannot keep up with the console it is emulating
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ios is selected with -DCMAKE_SYSTEM_NAME=iOS, android comes from the ndk toolchain
# gradle passes in, everything else is desktop
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(GB_IOS TRUE)
elseif(ANDROID)
    set(GB_ANDROID TRUE)
else()
    set(GB_DESKTOP TRUE)
endif()

set(GB_ROOT_DIR   ${CMAKE_CURRENT_SOURCE_DIR})
set(GB_ASSETS_DIR ${GB_ROOT_DIR}/assets)
set(GB_ROMS_DIR   ${GB_ROOT_DIR}/roms)

if(GB_ANDROID)
    # android 15 devices can use 16k memory pages, every .so in the apk has to be aligned
    # to match or the installer rejects the app, this has to precede every target below
    add_link_options("-Wl,-z,max-page-size=16384")
endif()

add_subdirectory(external)
add_subdirectory(src)
