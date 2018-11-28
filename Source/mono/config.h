#if defined(APPLE)
#include "config-mac.h"
#elif defined(EMSCRIPTEN)
#include "config-web.h"
#elif defined(ANDROID) || defined(__ANDROID__)
#include "config-android.h"
#elif defined(WIN32) || defined(_WIN32)
#include "config-win.h"
#else
#include "config-posix.h"
#endif
