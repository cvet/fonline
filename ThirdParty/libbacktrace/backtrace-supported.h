/* backtrace-supported.h -- FOnline configuration of the vendored copy.
   Upstream generates this file from backtrace-supported.h.in with configure; the engine builds libbacktrace
   with CMake on Linux only, where configure produces these values.  */

#define BACKTRACE_SUPPORTED 1
#define BACKTRACE_USES_MALLOC 0
#define BACKTRACE_SUPPORTS_THREADS 1
#define BACKTRACE_SUPPORTS_DATA 1
#define BACKTRACE_SUPPORTS_MOREDATA 1
