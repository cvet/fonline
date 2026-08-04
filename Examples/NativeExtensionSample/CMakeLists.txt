cmake_minimum_required(VERSION 3.22)
project(FOnlineNativeExtensionSample)

include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "FOnlineNativeExtensionSample.fomain")
SetOption(FO_DEV_NAME "FONATIVE")
SetOption(FO_NICE_NAME "FOnline Native Extension Sample")
SetOption(FO_ENABLE_3D OFF)
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_MONO_SCRIPTING OFF)
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_MAP_HEX_WIDTH 32)
SetOption(FO_MAP_HEX_HEIGHT 16)
SetOption(FO_MAP_CAMERA_ANGLE 25.6589)
SetOption(FO_APP_ICON "Engine/Resources/Radiation.ico")
SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

StartProjectGeneration()
RegisterProjectOptions()
AddThirdPartyLibraries()

add_library(NativeExtensionCore STATIC
    SourceExt/NativeExtensionCore.cpp)
target_include_directories(NativeExtensionCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/SourceExt")
target_compile_features(NativeExtensionCore PUBLIC cxx_std_20)
target_compile_definitions(NativeExtensionCore PUBLIC
    FO_NATIVE_EXTENSION_CORE_LINKED=1)

if(MSVC)
    target_compile_options(NativeExtensionCore PRIVATE /W4 /WX)
else()
    target_compile_options(NativeExtensionCore PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(FONATIVE_NativeExtensionCoreTest
    Tests/NativeExtensionCoreTest.cpp)
target_link_libraries(FONATIVE_NativeExtensionCoreTest PRIVATE
    NativeExtensionCore)

AddProjectLibraries(
    ROLES SERVER
    LIBRARIES NativeExtensionCore)
AddEngineSources(SERVER SourceExt/ServerExtension.cpp)
RegisterEngineSources()
SetupCodeGeneration()
BuildCoreLibraries()
BuildApplications()
SetupScriptsAndBaking()
BuildPackages()

if(FO_BUILD_SERVER AND FO_BUILD_BAKER)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_custom_target(RunNativeExtensionChecks
        COMMAND $<TARGET_FILE:FONATIVE_NativeExtensionCoreTest>
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/run_native_extension_smoke.py"
            $<TARGET_FILE:FONATIVE_ServerHeadless>
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"
        DEPENDS
            FONATIVE_NativeExtensionCoreTest
            FONATIVE_ServerHeadless
            BakeResources
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        VERBATIM
        USES_TERMINAL
        COMMENT "Run FOnline native extension contract checks")
endif()

FinalizeProjectGeneration()
