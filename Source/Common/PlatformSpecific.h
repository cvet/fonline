#ifndef ___PLATFORM_SPECIFIC___
#define ___PLATFORM_SPECIFIC___

//
// Operating system (passed by cmake)
// FO_WINDOWS
// FO_LINUX
// FO_MAC
// FO_IOS
// FO_ANDROID
// FO_WEB
//
// CPU
// FO_X86
// FO_X64
//
// Compiler
// FO_GCC
// FO_MSVC
//
// Render
// FO_OGL_ES
//

#if !defined ( FO_WINDOWS ) && !defined ( FO_LINUX ) && !defined ( FO_MAC ) && \
    !defined ( FO_ANDROID ) && !defined ( FO_IOS ) && !defined ( FO_WEB )
# error "Unknown operating system."
#endif

#if defined ( FO_MAC ) || defined ( FO_IOS )
# include <TargetConditionals.h>
#endif

#if defined ( FO_WEB )
# include <emscripten.h>
# include <emscripten/html5.h>
#endif

#if defined ( FO_IOS ) || defined ( FO_ANDROID ) || defined ( FO_WEB )
# define FO_OGL_ES
#endif

#if defined ( FO_WINDOWS )
# define FO_HAVE_DX
#endif

// Detect compiler
#if defined ( __GNUC__ )
# define FO_GCC
#elif defined ( _MSC_VER ) && !defined ( __MWERKS__ )
# define FO_MSVC
#else
# error "Unknown compiler."
#endif

// Detect CPU
#if ( defined ( FO_MSVC ) && defined ( _M_IX86 ) ) || ( defined ( FO_GCC ) && !defined ( __LP64__ ) )
# define FO_X86
#elif ( defined ( FO_MSVC ) && defined ( _M_X64 ) ) || ( defined ( FO_GCC ) && defined ( __LP64__ ) )
# define FO_X64
#else
# error "Unknown CPU."
#endif

// Function name
#if defined ( FO_MSVC )
# define _FUNC_    __FUNCTION__
#elif defined ( FO_GCC )
# define _FUNC_    __PRETTY_FUNCTION__
#endif

// Disable deprecated notification in GCC
#if defined ( FO_GCC )
# undef __DEPRECATED
#endif

#endif // ___PLATFORM_SPECIFIC___
