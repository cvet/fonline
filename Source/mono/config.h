#if defined(APPLE) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1
#warning "Used 'config-ios.h'"
#include "config-ios.h"
#elif defined(APPLE)
#warning "Used 'config-mac.h'"
#include "config-mac.h"
#elif defined(EMSCRIPTEN)
#warning "Used 'config-web.h'"
#include "config-web.h"
#elif defined(ANDROID) || defined(__ANDROID__)
#warning "Used 'config-android.h'"
#include "config-android.h"
#elif defined(WIN32) || defined(_WIN32)
#warning "Used 'config-win.h'"
#include "config-win.h"
#else
#warning "Used 'config-posix.h'"
#include "config-posix.h"
#endif
