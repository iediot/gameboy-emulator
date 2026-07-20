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

// GB_IOS for iphone/ipad, GB_DESKTOP for mac/linux/windows, use these not raw __APPLE__
#ifndef GB_IOS
    #define GB_IOS 0
#endif

#if GB_IOS
    #define GB_DESKTOP 0
#else
    #define GB_DESKTOP 1
#endif

#endif //GAMEBOY_EMU_PLATFORM_H
