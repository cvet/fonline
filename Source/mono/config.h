#pragma once

#if defined(__APPLE__)
#include "TargetConditionals.h"
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1
#include "config-ios.h"
#else
#include "config-mac.h"
#endif
#elif defined(EMSCRIPTEN)
#include "config-web.h"
#elif defined(ANDROID) || defined(__ANDROID__)
#include "config-android.h"
#elif defined(WIN32) || defined(_WIN32)
#include "config-win.h"
#else
#include "config-posix.h"
#endif
