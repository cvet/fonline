# CMake initial cache

if( NOT DEFINED "ENV{ROOT_FULL_PATH}" )
	message( FATAL_ERROR "Define ROOT_FULL_PATH" )
endif()

set( CMAKE_TOOLCHAIN_FILE "$ENV{ROOT_FULL_PATH}/BuildScripts/ios.toolchain.cmake" CACHE PATH "" FORCE )
set( IOS_PLATFORM "OS64" CACHE STRING "" FORCE )
set( IOS_ARCH "arm64" CACHE STRING "" FORCE )
set( ENABLE_BITCODE FALSE CACHE BOOL "" FORCE )
