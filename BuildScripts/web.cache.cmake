# CMake initial cache

if( NOT DEFINED "ENV{ROOT_FULL_PATH}" )
	message( FATAL_ERROR "Define ROOT_FULL_PATH" )
endif()
if( NOT DEFINED "ENV{EMSCRIPTEN}" )
	message( FATAL_ERROR "Define EMSCRIPTEN" )
endif()

set( CMAKE_TOOLCHAIN_FILE "$ENV{EMSCRIPTEN}/cmake/Modules/Platform/Emscripten.cmake" CACHE PATH "" FORCE )
set( EMSCRIPTEN $ENV{EMSCRIPTEN} CACHE STRING "" FORCE )
