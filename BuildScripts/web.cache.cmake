# CMake initial cache

if( NOT DEFINED "ENV{FO_SOURCE}" )
	message( FATAL_ERROR "Define FO_SOURCE" )
endif()

set( CMAKE_TOOLCHAIN_FILE "$ENV{EMSCRIPTEN_ROOT}/cmake/Modules/Platform/Emscripten.cmake" CACHE PATH "" FORCE )
