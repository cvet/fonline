# CMake initial cache

if(NOT DEFINED ENV{FO_ENGINE_ROOT})
	message(FATAL_ERROR "Define FO_ENGINE_ROOT")
endif()
if(NOT DEFINED ENV{SCE_ORBIS_SDK_DIR})
	message(FATAL_ERROR "Define SCE_ORBIS_SDK_DIR")
endif()

set(CMAKE_TOOLCHAIN_FILE "$ENV{FO_ENGINE_ROOT}/BuildTools/ps4.toolchain.cmake" CACHE PATH "Forced by FOnline" FORCE)
set(PS4 YES CACHE BOOL "Forced by FOnline" FORCE)
set(PS4_SDK "$ENV{SCE_ORBIS_SDK_DIR}" CACHE STRING "Forced by FOnline" FORCE)
