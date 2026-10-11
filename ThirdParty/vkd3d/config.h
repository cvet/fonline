/*
 * FOnline replacement for the autoconf-generated include/config.h: the engine builds libvkd3d-shader
 * through CMake, so every probe configure.ac would run is answered here from the compiler and platform.
 */

#define PACKAGE_NAME "vkd3d"
#define PACKAGE_TARNAME "vkd3d"
#define PACKAGE_VERSION "2.1"
#define PACKAGE_STRING "vkd3d 2.1"
#define PACKAGE_BUGREPORT "https://bugs.winehq.org"
#define PACKAGE_URL "https://gitlab.winehq.org/wine/vkd3d"

#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_WCHAR_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_LIBM 1

/* The SPIR-V headers are the engine's SPIRV-Cross copies, staged under spirv/unified1/ by vkd3d.cmake */
#define HAVE_SPIRV_UNIFIED1_SPIRV_H 1
#define HAVE_SPIRV_UNIFIED1_GLSL_STD_450_H 1

#if defined(__GNUC__) || defined(__clang__)
#define HAVE_BUILTIN_CLZ 1
#define HAVE_BUILTIN_CTZ 1
#define HAVE_BUILTIN_POPCOUNT 1
#define HAVE_BUILTIN_ADD_OVERFLOW 1
#define HAVE_SYNC_ADD_AND_FETCH 1
#define HAVE_SYNC_BOOL_COMPARE_AND_SWAP 1
#define HAVE_ATOMIC_EXCHANGE_N 1
#endif

#if defined(_WIN32)
#define HAVE__STRTOF_L 1
#else
#define HAVE_UNISTD_H 1
#define HAVE_STRINGS_H 1
#define HAVE_PTHREAD_H 1
#define HAVE_DLFCN_H 1
#endif

#if defined(__APPLE__)
#define HAVE_XLOCALE_H 1
#define HAVE_PTHREAD_THREADID_NP 1
#elif defined(__linux__)
#define HAVE_STRTOF_L 1
#define HAVE_GETTID 1
#elif defined(__EMSCRIPTEN__)
#define HAVE_STRTOF_L 1
#endif
