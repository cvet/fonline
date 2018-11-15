# CMake initial cache

if( NOT DEFINED "ENV{SOURCE_FULL_PATH}" )
	message( FATAL_ERROR "Define SOURCE_FULL_PATH" )
endif()

set( CMAKE_TOOLCHAIN_FILE "$ENV{SOURCE_FULL_PATH}/BuildScripts/ios.toolchain.cmake" CACHE PATH "" FORCE )
add_definitions( "-D_LARGEFILE64_SOURCE" )
