#if defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1
#pragma worning("Used 'config-ios.h'")
#include "config-ios.h"
#elif defined(__APPLE__)
#pragma worning("Used 'config-mac.h'")
#include "config-mac.h"
#elif defined(EMSCRIPTEN)
#pragma worning("Used 'config-web.h'")
#include "config-web.h"
#elif defined(ANDROID) || defined(__ANDROID__)
#pragma worning("Used 'config-android.h'")
#include "config-android.h"
#elif defined(WIN32) || defined(_WIN32)
#pragma worning("Used 'config-win.h'")
#include "config-win.h"
#else
#pragma worning("Used 'config-posix.h'")
#include "config-posix.h"
#endif
