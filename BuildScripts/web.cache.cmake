# CMake initial cache

# Build commands
# cd <build dir>
# export FO_SOURCE=<source> && cmake -C $FO_SOURCE/BuildScripts/web.cache.cmake $FO_SOURCE/Source
# make

if( NOT DEFINED "ENV{FO_SOURCE}" )
	message( FATAL_ERROR "Define FO_SOURCE" )
endif()

set( CMAKE_TOOLCHAIN_FILE "$ENV{EMSCRIPTEN_ROOT}/cmake/Modules/Platform/Emscripten.cmake" CACHE PATH "" FORCE )
