#pragma once

#if defined(__APPLE__)
#include "TargetConditionals.h"
#if defined(TARGET_OS_IPHONE)
#pragma message("Used 'config-ios.h'")
#include "config-ios.h"
#else
#pragma message("Used 'config-mac.h'")
#include "config-mac.h"
#endif
#elif defined(EMSCRIPTEN)
#pragma message("Used 'config-web.h'")
#include "config-web.h"
#elif defined(ANDROID) || defined(__ANDROID__)
#pragma message("Used 'config-android.h'")
#include "config-android.h"
#elif defined(WIN32) || defined(_WIN32)
#pragma message("Used 'config-win.h'")
#include "config-win.h"
#else
#pragma message("Used 'config-posix.h'")
#include "config-posix.h"
#endif
