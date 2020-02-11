# CMake initial cache

if( NOT DEFINED "ENV{FO_ROOT}" )
	message( FATAL_ERROR "Define FO_ROOT" )
endif()

set( CMAKE_TOOLCHAIN_FILE "$ENV{FO_ROOT}/BuildScripts/ios.toolchain.cmake" CACHE PATH "Forced by FOnline" FORCE )
set( IOS_PLATFORM "OS64" CACHE STRING "Forced by FOnline" FORCE )
set( IOS_ARCH "arm64" CACHE STRING "Forced by FOnline" FORCE )
set( IOS_DEPLOYMENT_TARGET "10.0" CACHE STRING "Forced by FOnline" FORCE )
set( ENABLE_BITCODE FALSE CACHE BOOL "Forced by FOnline" FORCE )
