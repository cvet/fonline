#pragma once

#if defined(WIN32)
#pragma message("Used 'eglib-config-win.h'")
#include "eglib-config-win.h"
#else
#pragma message("Used 'eglib-config-posix.h'")
#include "eglib-config-posix.h"
#endif
