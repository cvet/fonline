# CMake initial cache

if( NOT DEFINED "ENV{FO_SOURCE}" )
	message( FATAL_ERROR "Define FO_SOURCE" )
endif()

set( CMAKE_TOOLCHAIN_FILE "$ENV{FO_SOURCE}/BuildScripts/ios.toolchain.cmake" CACHE PATH "" FORCE )
