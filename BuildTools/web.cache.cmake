# CMake initial cache

if( NOT DEFINED "ENV{FO_ROOT}" )
	message( FATAL_ERROR "Define FO_ROOT" )
endif()
if( NOT DEFINED "ENV{EMSDK}" )
	message( FATAL_ERROR "Define EMSDK" )
endif()

set( CMAKE_TOOLCHAIN_FILE "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" CACHE PATH "Forced by FOnline" FORCE )
set( EMSCRIPTEN YES CACHE STRING "Forced by FOnline" FORCE )
