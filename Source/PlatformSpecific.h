#ifndef __PLATFORM_SPECIFIC__
#define __PLATFORM_SPECIFIC__

//
// Operating system
// FO_WINDOWS
// FO_LINUX
//
// CPU
// FO_X86
// FO_X64
//
// Compiler
// FO_GCC
// FO_MSVC
//

// GCC options
// Compiler
// -mthreads
// Linker
// -lws2_32
// -lwinmm
// -lstlport
// Warnings
// -Wno-invalid-offsetof
// Defines
// _STLP_USE_STATIC_LIB

// Detect operating system
#if defined(_WIN32) || defined(_WIN64)
	#define FO_WINDOWS
#elif defined(__linux__)
	#define FO_LINUX
#else
	#error "Unknown operating system."
#endif

// Detect compiler
#if defined(__GNUC__)
	#define FO_GCC
#elif defined(_MSC_VER) && !defined(__MWERKS__)
	#define FO_MSVC
#else
	#error "Unknown compiler."
#endif

// Detect CPU
#if (defined(FO_MSVC) && defined(_M_IX86)) || (defined(FO_GCC) && !defined(__LP64__))
	#define FO_X86
#elif (defined(FO_MSVC) && defined(_M_X64)) || (defined(FO_GCC) && defined(__LP64__))
	#define FO_X64
#else
	#error "Unknown CPU."
#endif

// TLS
#if defined(FO_MSVC)
	#define THREAD __declspec(thread)
#elif defined(FO_GCC)
	#define THREAD __thread
#endif

// Types
#if defined(FO_MSVC)
	typedef unsigned char    uchar;
	typedef unsigned short   ushort;
	typedef unsigned int     uint;
	typedef unsigned __int64 uint64;
	typedef __int64          int64;
#elif defined(FO_GCC)
	#include <inttypes.h>
	typedef unsigned char    uchar;
	typedef unsigned short   ushort;
	typedef unsigned int     uint;
	typedef uint64_t         uint64;
	typedef int64_t          int64;
#endif

// Function name
#if defined(FO_MSVC)
	#define _FUNC_ __FUNCTION__
#elif defined(FO_GCC)
	#define _FUNC_ __PRETTY_FUNCTION__
#endif

// Disable deprecated notification in GCC
#if defined(FO_GCC)
    #undef __DEPRECATED
#endif

// Linux sleeping
#if defined(FO_LINUX)
    #include <unistd.h>
	#define Sleep(ms) usleep((ms)*1000)
#endif

// Libevent
// For now bugged for Windows IOCP, use own variant
#if !defined(FO_WINDOWS)
	#define USE_LIBEVENT
#endif

#endif // __PLATFORM_SPECIFIC__
