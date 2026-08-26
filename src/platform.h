//
// Created by edi on 7/20/26.
//

#ifndef GAMEBOY_EMU_PLATFORM_H
#define GAMEBOY_EMU_PLATFORM_H

// __APPLE__ is true on macos too, so use TargetConditionals to single out the phone
#if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #define GB_IOS 1
    #endif
#endif

#if defined(__ANDROID__)
    #define GB_ANDROID 1
#endif

// GB_IOS for iphone/ipad, GB_ANDROID for android, GB_MOBILE for the touch ui the two
// of them share, GB_DESKTOP for mac/linux/windows, use these not raw __APPLE__
#ifndef GB_IOS
    #define GB_IOS 0
#endif

#ifndef GB_ANDROID
    #define GB_ANDROID 0
#endif

#if GB_IOS || GB_ANDROID
    #define GB_MOBILE  1
    #define GB_DESKTOP 0
#else
    #define GB_MOBILE  0
    #define GB_DESKTOP 1
#endif

// whether the os is currently in dark appearance, false when it cannot be determined
bool gb_system_dark();

#endif //GAMEBOY_EMU_PLATFORM_H
