cmake_minimum_required(VERSION 3.22)
project(FOnlineTest)

include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "FOnlineTest.fomain")
SetOption(FO_DEV_NAME "FOTEST")
SetOption(FO_NICE_NAME "FOnlineTest")
SetOption(FO_APP_ICON "${CMAKE_CURRENT_LIST_DIR}/App.ico")
SetOption(FO_ENABLE_3D ON)
SetOption(FO_SPARK_PARTICLES ON)
SetOption(FO_EFFEKSEER_PARTICLES OFF)
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_MANAGED_SCRIPTING OFF)
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_MAP_HEX_WIDTH 32)
SetOption(FO_MAP_HEX_HEIGHT 16)
SetOption(FO_MAP_CAMERA_ANGLE 25.6589)

SetOption(FO_UNIT_TESTS ON)
SetOptionValues(
    FO_DATA_SOURCE_DAT ON
    FO_DATA_SOURCE_ZIP ON
    FO_DATABASE_JSON ON
    FO_DATABASE_SQLITE ON
    FO_DATABASE_MONGO ON
    FO_IMAGE_FRM ON
    FO_IMAGE_RIX ON
    FO_IMAGE_ART ON
    FO_IMAGE_SPR ON
    FO_IMAGE_ZAR ON
    FO_IMAGE_TIL ON
    FO_IMAGE_MOS ON
    FO_IMAGE_BAM ON)
SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

StartProjectGeneration()
RegisterProjectOptions()
AddThirdPartyLibraries()
AddEngineSources(SERVER TestServerExtension.cpp)
RegisterEngineSources()
SetupCodeGeneration()
BuildCoreLibraries()
BuildApplications()
SetupScriptsAndBaking()
BuildPackages()
FinalizeProjectGeneration()
