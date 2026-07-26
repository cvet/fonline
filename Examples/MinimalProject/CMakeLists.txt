cmake_minimum_required(VERSION 3.22)
project(FOnlineStarter)

include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "FOnlineStarter.fomain")
SetOption(FO_DEV_NAME "FOSTART")
SetOption(FO_NICE_NAME "FOnline Starter")
SetOption(FO_ENABLE_3D ON)
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_MONO_SCRIPTING OFF)
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_MAP_HEX_WIDTH 32)
SetOption(FO_MAP_HEX_HEIGHT 16)
SetOption(FO_MAP_CAMERA_ANGLE 25.6589)
SetOption(FO_APP_ICON "Engine/Resources/Radiation.ico")

SetOption(FO_UNIT_TESTS ON)
SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

StartProjectGeneration()
RegisterProjectOptions()
AddThirdPartyLibraries()
AddEngineSources(SERVER StarterServerExtension.cpp)
RegisterEngineSources()
SetupCodeGeneration()
BuildCoreLibraries()
BuildApplications()
SetupScriptsAndBaking()
BuildPackages()

if(FO_BUILD_SERVER AND FO_BUILD_BAKER)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_custom_target(RunStarterSmoke
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/run_starter_smoke.py"
            $<TARGET_FILE:FOSTART_ServerHeadless>
            "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"
            "${FO_OUTPUT_PATH}/Baking/Metadata/Metadata.fometa-server"
            "${FO_OUTPUT_PATH}/Baking/Metadata/Metadata.fometa-client"
        DEPENDS FOSTART_ServerHeadless BakeResources
        WORKING_DIRECTORY ${FO_OUTPUT_PATH}
        VERBATIM
        USES_TERMINAL
        COMMENT "Run minimal FOnline starter smoke")
endif()

FinalizeProjectGeneration()
