cmake_minimum_required(VERSION 3.22)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FOnlineResourcePackHash LANGUAGES CXX)
endif()

add_library(FOnlineResourcePackHash SHARED ResourcePackHash.h ResourcePackHash.cpp)
# Python loads this host tool without the engine's allocator, profiler or sanitizer runtime
set_property(TARGET FOnlineResourcePackHash PROPERTY COMPILE_OPTIONS "")
set_property(TARGET FOnlineResourcePackHash PROPERTY LINK_OPTIONS "")
target_compile_features(FOnlineResourcePackHash PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(FOnlineResourcePackHash PRIVATE /W4 /WX)
else()
    target_compile_options(FOnlineResourcePackHash PRIVATE -Wall -Wextra -Werror -Wconversion)
endif()

if(DEFINED FO_OUTPUT_PATH)
    set(hashOutput "${FO_OUTPUT_PATH}/Binaries/BuildTools-${FO_BUILD_PLATFORM}")
else()
    set(hashOutput "${CMAKE_CURRENT_BINARY_DIR}/bin")
endif()
set_target_properties(FOnlineResourcePackHash PROPERTIES
    PREFIX ""
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    LIBRARY_OUTPUT_DIRECTORY "${hashOutput}"
    RUNTIME_OUTPUT_DIRECTORY "${hashOutput}")
foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${config}" configUpper)
    set_target_properties(FOnlineResourcePackHash PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${configUpper} "${hashOutput}"
        RUNTIME_OUTPUT_DIRECTORY_${configUpper} "${hashOutput}")
endforeach()

