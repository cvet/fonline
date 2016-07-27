# CMake initial cache

# Build commands
# cd <build dir>
# export FO_SOURCE=<source> && export ANDROID_NDK=<ndk> && cmake -C $FO_SOURCE/Android/android.cache.cmake $FO_SOURCE
# make

if( NOT DEFINED "ENV{FO_SOURCE}" )
	message( FATAL_ERROR "Define FO_SOURCE" )
endif()
if( NOT DEFINED "ENV{ANDROID_NDK}" )
	message( FATAL_ERROR "Define ANDROID_NDK" )
endif()

set( CMAKE_TOOLCHAIN_FILE "$ENV{FO_SOURCE}/Android/android.toolchain.cmake" CACHE PATH "" FORCE )
set( ANDROID YES CACHE PATH "" FORCE )
set( ANDROID_NDK $ENV{ANDROID_NDK} CACHE PATH "" FORCE )
set( ANDROID_NATIVE_API_LEVEL "android-15" CACHE PATH "" FORCE )
set( ANDROID_STL "gnustl_static" CACHE PATH "" FORCE )
set( ANDROID_STL_FORCE_FEATURES OFF CACHE PATH "" FORCE )
