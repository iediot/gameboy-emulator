//
// desktop appearance query, macos keeps it in a global preference and everything else
// has no common answer so it falls back to dark
//

#include "platform.h"
#if GB_DESKTOP

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>

bool gb_system_dark() {
    CFPropertyListRef v = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"),
                                                    kCFPreferencesAnyApplication);
    if (!v)
        return false;               // the key is absent entirely in light mode
    bool dark = false;
    if (CFGetTypeID(v) == CFStringGetTypeID())
        dark = CFStringCompare((CFStringRef)v, CFSTR("Dark"), 0) == kCFCompareEqualTo;
    CFRelease(v);
    return dark;
}

#else
bool gb_system_dark() { return true; }
#endif

#endif
