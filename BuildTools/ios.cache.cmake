# CMake initial cache

if( NOT DEFINED "ENV{FO_ROOT}" )
	message( FATAL_ERROR "Define FO_ROOT" )
endif()

set( CMAKE_TOOLCHAIN_FILE "$ENV{FO_ROOT}/BuildTools/ios.toolchain.cmake" CACHE PATH "Forced by FOnline" FORCE )
set( PLATFORM "OS64" CACHE STRING "Forced by FOnline" FORCE )
set( ARCHS "arm64" CACHE STRING "Forced by FOnline" FORCE )
set( DEPLOYMENT_TARGET "10.0" CACHE STRING "Forced by FOnline" FORCE )
set( ENABLE_BITCODE OFF CACHE BOOL "Forced by FOnline" FORCE )
set( ENABLE_ARC OFF CACHE BOOL "Forced by FOnline" FORCE )
set( ENABLE_VISIBILITY OFF CACHE BOOL "Forced by FOnline" FORCE )
set( ENABLE_STRICT_TRY_COMPILE OFF CACHE BOOL "Forced by FOnline" FORCE )
set( CMAKE_XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.fonline.default" CACHE STRING "Forced by FOnline" )
